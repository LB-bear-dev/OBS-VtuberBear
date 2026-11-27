// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
// ReSharper disable CppUnusedIncludeDirective
#include "PCH.h"
#include "Render/WatchedImage.h"

#include <algorithm>
#include <array>
#include <memory>
#include <obs-module.h>
#include <optional>
#include <ranges>
#include <span>

#include "CharacterControlCore/Messaging.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "Psd/Psd.h"
#include "Psd/PsdPlatform.h"
#include "Psd/PsdChannel.h"
#include "Psd/PsdChannelType.h"
#include "Psd/PsdColorMode.h"
#include "Psd/PsdDocument.h"
#include "Psd/PsdInterleave.h"
#include "Psd/PsdLayer.h"
#include "Psd/PsdLayerCanvasCopy.h"
#include "Psd/PsdLayerMask.h"
#include "Psd/PsdLayerMaskSection.h"
#include "Psd/PsdLayerType.h"
#include "Psd/PsdMallocAllocator.h"
#include "Psd/PsdNativeFile.h"
#include "Psd/PsdParseDocument.h"
#include "Psd/PsdParseLayerMaskSection.h"
#include "Psd/PsdSyncFileReader.h"
#include "Psd/PsdSyncFileUtil.h"
#include "Psd/PsdVectorMask.h"

std::unordered_map<FilePath, std::weak_ptr<CharacterControlRender::WatchedFileImageLibrary>> CharacterControlRender::CharacterImageLibrary::LoadedImageLibraries;
namespace
{
// psd_sdk sizes its buffers with 32-bit width*height math, these should be the limits based on those constraints.
constexpr unsigned int MAX_PSD_SIDE = 16384;

void UnloadImage(gs_image_file_ex* Image)
{
	if(Image != nullptr)
	{
		obs_enter_graphics();
		gs_image_file_ex_free(Image);
		obs_leave_graphics();
	}
}

bool LoadImage(FilePathRef FileName, gs_image_file_ex* Image)
{
	UnloadImage(Image);

	if(FileName.empty())
	{
		return false;
	}

	const std::string LogName = reinterpret_cast<const char*>(FileName.u8string().c_str());

	DEBUG_("loading texture '%s'", LogName.c_str());
	gs_image_file_ex_init(Image, LogName.c_str(), GS_IMAGE_ALPHA_PREMULTIPLY);

	obs_enter_graphics();
	gs_image_file_ex_init_texture(Image);
	obs_leave_graphics();

	if(!Image->loaded)
	{
		WARN_("failed to load texture '%s'", LogName.c_str());
		return false;
	}

	return true;
}

StrId GetLayerName(const psd::Layer& Layer)
{
	StrId LayerName;
	if(Layer.utf16Name)
	{
		const auto* const WideName = reinterpret_cast<const wchar_t*>(Layer.utf16Name);
		if(const int Size = WideCharToMultiByte(CP_UTF8, 0, WideName, -1, nullptr, 0, nullptr, nullptr); Size > 1)
		{
			LayerName.resize(Size - 1);
			WideCharToMultiByte(CP_UTF8, 0, WideName, -1, reinterpret_cast<char*>(LayerName.data()), Size, nullptr, nullptr);
		}
	} else
	{
		const std::string_view LayerNameRaw = Layer.name.c_str();
		LayerName.assign(LayerNameRaw.begin(), LayerNameRaw.end());
	}

	return LayerName;
}

StrId GetLayerPath(const psd::Layer& Layer)
{
	if(Layer.parent == nullptr)
	{
		return GetLayerName(Layer);
	}

	return GetLayerPath(*Layer.parent) + u8"/" + GetLayerName(Layer);
}

// Combine the opacity of all groups the layer is part of to get the final opacity, so like if a parent is at 50% opacity and this layer is at
// 10% opacity, the actual opacity is 5%.
unsigned int GetEffectiveOpacity(const psd::Layer& Layer)
{
	unsigned int Opacity = Layer.opacity;
	for(const psd::Layer* Parent = Layer.parent; Parent != nullptr; Parent = Parent->parent)
	{
		Opacity = Opacity * Parent->opacity / 255;
	}

	return Opacity;
}

// Grab mask opacity samples from a mask texture
template<typename Mask>
unsigned int SampleMask(const Mask* MaskData, int X, int Y)
{
	if(MaskData == nullptr || MaskData->data == nullptr)
	{
		return 255;
	}

	if(X < MaskData->left || X >= MaskData->right || Y < MaskData->top || Y >= MaskData->bottom)
	{
		return MaskData->defaultColor;
	}

	const auto* Data = static_cast<const uint8_t*>(MaskData->data);
	return Data[(Y - MaskData->top) * (MaskData->right - MaskData->left) + (X - MaskData->left)];
}

bool IsImageChannel(const psd::Channel& Channel)
{
	return Channel.type == psd::channelType::TRANSPARENCY_MASK || (Channel.type >= psd::channelType::R && Channel.type <= psd::channelType::B);
}

bool HasUndecodedChannel(const psd::Layer& Layer)
{
	return std::ranges::any_of(std::span(Layer.channels, Layer.channelCount),
		[](const psd::Channel& Channel)
		{
			return IsImageChannel(Channel) && Channel.data == nullptr;
		});
}

// Layers can potentially declare their size to be larger than the whole file. Sanity check this (discovered in fuzz testing)
bool LayerSectionFitsFile(const psd::Document& Document, psd::File& File, const uint64_t FileSize)
{
	const uint64_t Offset = Document.layerMaskInfoSection.offset;
	const uint64_t LengthSize = Document.isLargeDocument ? 8 : 4;

	if(Offset < LengthSize || Offset > FileSize)
	{
		return false;
	}

	psd::SyncFileReader Reader(&File);
	Reader.SetPosition(Offset - LengthSize);
	const uint64_t Declared = Document.isLargeDocument ? psd::fileUtil::ReadFromFileBE<uint64_t>(Reader) : psd::fileUtil::ReadFromFileBE<uint32_t>(Reader);

	return Declared <= FileSize - Offset;
}

// These are all convenience tools to clean up psd resources after scoped use.
struct PsdDeleter
{
	psd::Allocator* Allocator = nullptr;

	void operator()(psd::Document* Document) const
	{
		DestroyDocument(Document, Allocator);
	}

	void operator()(psd::LayerMaskSection* Section) const
	{
		DestroyLayerMaskSection(Section, Allocator);
	}

	void operator()(uint8_t* Plane) const
	{
		Allocator->Free(Plane);
	}

	void operator()(psd::NativeFile* File) const
	{
		File->Close();
	}
};

template<typename T>
using PsdPtr = std::unique_ptr<T, PsdDeleter>;

PsdPtr<psd::Document> ParseSupportedDocument(FilePathRef FileName, psd::NativeFile& File, psd::Allocator& Allocator)
{
	const std::string LogName = reinterpret_cast<const char*>(FileName.u8string().c_str());

	PsdPtr<psd::Document> Document(CreateDocument(&File, &Allocator), PsdDeleter{&Allocator});
	if(!Document)
	{
		ERROR_("Cannot create document for %s.", LogName.c_str());
		return nullptr;
	}

	if(Document->bitsPerChannel != 8 || Document->colorMode != psd::colorMode::RGB)
	{
		ERROR_("%s is not an 8-bit RGB PSD.", LogName.c_str());
		return nullptr;
	}

	if(Document->width == 0 || Document->height == 0 || Document->width > MAX_PSD_SIDE || Document->height > MAX_PSD_SIDE)
	{
		ERROR_("%s is %ux%u, max is %ux%u.", LogName.c_str(), Document->width, Document->height, MAX_PSD_SIDE, MAX_PSD_SIDE);
		return nullptr;
	}

	if(!LayerSectionFitsFile(*Document, File, File.GetSize()))
	{
		ERROR_("%s ends before its layers.", LogName.c_str());
		return nullptr;
	}

	return Document;
}

std::unique_ptr<gs_image_file_ex> MakeLayerImage(const psd::Layer& Layer, psd::Allocator& Allocator, const int Left, const int Top, const int Right, const int Bottom)
{
	constexpr int AlphaPlane = 3;
	const unsigned int Width = Right - Left;
	const unsigned int Height = Bottom - Top;
	const size_t PlaneSize = static_cast<size_t>(Width) * Height;

	const auto NewPlane = [&Allocator, PlaneSize](const uint8_t Fill)
	{
		PsdPtr<uint8_t> Plane(static_cast<uint8_t*>(Allocator.Allocate(PlaneSize, 16u)), PsdDeleter{&Allocator});
		memset(Plane.get(), Fill, PlaneSize);
		return Plane;
	};

	std::array<PsdPtr<uint8_t>, 4> Planes;
	for(const psd::Channel& Channel : std::span(Layer.channels, Layer.channelCount))
	{
		if(IsImageChannel(Channel))
		{
			PsdPtr<uint8_t>& Plane = Planes[Channel.type == psd::channelType::TRANSPARENCY_MASK ? AlphaPlane : Channel.type];
			Plane = NewPlane(0);
			psd::imageUtil::CopyLayerData(static_cast<const uint8_t*>(Channel.data), Plane.get(), Layer.left - Left, Layer.top - Top, Layer.right - Left, Layer.bottom - Top, Width, Height);
		}
	}

	for(int PlaneIndex = 0; PlaneIndex < 4; ++PlaneIndex)
	{
		if(!Planes[PlaneIndex])
		{
			Planes[PlaneIndex] = NewPlane(PlaneIndex == AlphaPlane ? 255u : 0u);
		}
	}

	auto Image = std::make_unique<gs_image_file_ex>();
	Image->space = GS_CS_SRGB;
	Image->alpha_mode = GS_IMAGE_ALPHA_PREMULTIPLY;
	Image->format = GS_RGBA;
	Image->cx = Width;
	Image->cy = Height;
	Image->mem_usage = static_cast<size_t>(Width) * Height * 4;
	Image->texture_data = static_cast<uint8_t*>(bmalloc(Image->mem_usage));

	psd::imageUtil::InterleaveRGBA(Planes[0].get(), Planes[1].get(), Planes[2].get(), Planes[3].get(), Image->texture_data, Width, Height);

	return Image;
}

void BakeMasksAndOpacity(const psd::Layer& Layer, uint8_t* Pixels, const int Left, const int Top, const int Right, const int Bottom)
{
	const unsigned int Opacity = GetEffectiveOpacity(Layer);
	uint8_t* Pixel = Pixels;
	for(int Y = Top; Y < Bottom; ++Y)
	{
		for(int X = Left; X < Right; ++X, Pixel += 4)
		{
			unsigned int Alpha = Pixel[3];
			Alpha = Alpha * SampleMask(Layer.layerMask, X, Y) / 255;
			Alpha = Alpha * SampleMask(Layer.vectorMask, X, Y) / 255;
			Alpha = Alpha * Opacity / 255;

			Pixel[0] = static_cast<uint8_t>(Pixel[0] * Alpha / 255);
			Pixel[1] = static_cast<uint8_t>(Pixel[1] * Alpha / 255);
			Pixel[2] = static_cast<uint8_t>(Pixel[2] * Alpha / 255);
			Pixel[3] = static_cast<uint8_t>(Alpha);
		}
	}
}

std::optional<b2Vec2> TrimTransparentBorders(gs_image_file_ex& Image)
{
	const unsigned int Width = Image.cx;
	const unsigned int Height = Image.cy;

	int MinX = static_cast<int>(Width), MinY = static_cast<int>(Height), MaxX = -1, MaxY = -1;
	const uint8_t* AlphaPixel = Image.texture_data + 3;
	for(int Y = 0; Y < static_cast<int>(Height); ++Y)
	{
		for(int X = 0; X < static_cast<int>(Width); ++X, AlphaPixel += 4)
		{
			if(*AlphaPixel != 0)
			{
				MinX = std::min(MinX, X);
				MaxX = std::max(MaxX, X);
				MinY = std::min(MinY, Y);
				MaxY = std::max(MaxY, Y);
			}
		}
	}

	if(MaxX < 0)
	{
		return std::nullopt;
	}

	const unsigned int TrimmedWidth = MaxX - MinX + 1;
	if(const unsigned int TrimmedHeight = MaxY - MinY + 1; TrimmedWidth != Width || TrimmedHeight != Height)
	{
		for(unsigned int Row = 0; Row < TrimmedHeight; ++Row)
		{
			memmove(Image.texture_data + static_cast<size_t>(Row) * TrimmedWidth * 4, Image.texture_data + (static_cast<size_t>(MinY + Row) * Width + MinX) * 4, static_cast<size_t>(TrimmedWidth) * 4);
		}

		Image.cx = TrimmedWidth;
		Image.cy = TrimmedHeight;
		Image.mem_usage = static_cast<size_t>(TrimmedWidth) * TrimmedHeight * 4;
	}

	return b2Vec2{.x = static_cast<float>(MinX), .y = static_cast<float>(MinY)};
}

bool LoadPsd(FilePathRef FileName, CharacterControlRender::FileImageLibrary& ImageLibrary, b2Vec2& CanvasSize)
{
	const std::string LogName = reinterpret_cast<const char*>(FileName.u8string().c_str());

	psd::MallocAllocator Allocator;
	psd::NativeFile File(&Allocator);

	// try opening the file. if it fails, bail out.
	if(!File.OpenRead(FileName.c_str()))
	{
		ERROR_("Cannot open file %s.\n", LogName.c_str());
		return false;
	}

	const PsdPtr<psd::NativeFile> PsdOpenHandle(&File, PsdDeleter{&Allocator});

	const PsdPtr<psd::Document> Document = ParseSupportedDocument(FileName, File, Allocator);
	if(!Document)
	{
		return false;
	}

	const PsdPtr<psd::LayerMaskSection> LayerMaskSection(ParseLayerMaskSection(Document.get(), &File, &Allocator), PsdDeleter{&Allocator});
	if(!LayerMaskSection)
	{
		ERROR_("Cannot read the layers of %s.", LogName.c_str());
		return false;
	}

	CanvasSize = {.x = static_cast<float>(Document->width), .y = static_cast<float>(Document->height)};

	for(unsigned int LayerIndex = 0; LayerIndex < LayerMaskSection->layerCount; ++LayerIndex)
	{
		psd::Layer& Layer = LayerMaskSection->layers[LayerIndex];

		// Other layer types are like folders or dividers, non image things.
		if(Layer.type != psd::layerType::ANY)
		{
			continue;
		}

		const int Left = std::max(Layer.left, 0);
		const int Top = std::max(Layer.top, 0);
		const int Right = std::min(Layer.right, static_cast<int>(Document->width));
		const int Bottom = std::min(Layer.bottom, static_cast<int>(Document->height));
		if(Right <= Left || Bottom <= Top)
		{
			continue;
		}

		ExtractLayer(Document.get(), &File, &Allocator, &Layer);

		if(HasUndecodedChannel(Layer))
		{
			WARN_("Cannot decode %s layer %u.", LogName.c_str(), LayerIndex);
			continue;
		}

		std::unique_ptr<gs_image_file_ex> Image = MakeLayerImage(Layer, Allocator, Left, Top, Right, Bottom);
		BakeMasksAndOpacity(Layer, Image->texture_data, Left, Top, Right, Bottom);

		const std::optional<b2Vec2> Kept = TrimTransparentBorders(*Image);
		if(!Kept)
		{
			// Nothing visible
			bfree(Image->texture_data);
			continue;
		}

		const b2Vec2 Offset{.x = static_cast<float>(Left) + Kept->x, .y = static_cast<float>(Top) + Kept->y};

		Image->loaded = true;
		obs_enter_graphics();
		gs_image_file_ex_init_texture(Image.get());
		obs_leave_graphics();

		const StrId LayerPath = GetLayerPath(Layer);

		// only add new layers. try_emplace leaves Image alone when the name is taken.
		if(!ImageLibrary.try_emplace(LayerPath, std::move(Image), static_cast<int>(LayerIndex), Offset).second)
		{
			WARN_("%s has duplicate layer '%s'.", LogName.c_str(), reinterpret_cast<const char*>(LayerPath.c_str()));
			UnloadImage(Image.get());
		}
	}

	return true;
}
}

const std::vector<StrId>& CharacterControlRender::WatchedFileImageLibrary::GetImageNames() const
{
	return ImageNames;
}

b2Vec2 CharacterControlRender::WatchedFileImageLibrary::GetMaxResolution() const
{
	return MaxResolution;
}

bool CharacterControlRender::WatchedFileImageLibrary::UpdateInternal()
{
	bool Success;
	if(GetPath().extension() == ".psd")
	{
		Success = LoadPsd(GetPath(), *GetMutable(), MaxResolution);
	} else
	{
		const StrId Name = !OverrideName.empty() ? OverrideName : GetPath().stem().u8string();
		const FileLayer& AddedLayer = GetMutable()->emplace(Name, FileLayer{.Image = std::make_unique<gs_image_file_ex>(), .ZOrder = 0, .Offset = {.x = 0, .y = 0}}).first->second;
		Success = LoadImage(GetPath(), AddedLayer.Image.get());
	}

	// update valid image names
	ImageNames.clear();
	if(const FileImageLibrary* Library = GetMutable())
	{
		for(const auto& [LayerName, Layer] : *Library)
		{
			if(Layer.Image != nullptr)
			{
				ImageNames.push_back(LayerName);
				MaxResolution.x = std::max(MaxResolution.x, Layer.Offset.x + static_cast<float>(Layer.Image->cx));
				MaxResolution.y = std::max(MaxResolution.y, Layer.Offset.y + static_cast<float>(Layer.Image->cy));
			}
		}
	}

	return Success;
}

void CharacterControlRender::WatchedFileImageLibrary::DisposeInternal()
{
	if(const FileImageLibrary* Library = GetMutable())
	{
		for(const FileLayer& Layer : *Library | std::views::values)
		{
			UnloadImage(Layer.Image.get());
		}
	}
}

void CharacterControlRender::CharacterImageLibrary::AddImageLibrary(FilePathRef Path)
{
	AddImageLibrary(Path, u8"");
}

void CharacterControlRender::CharacterImageLibrary::AddImageLibrary(FilePathRef Path, StrIdRef OverrideName)
{
	// Already exists
	if(std::ranges::any_of(ImageLibraries,
		[&Path](const auto& ImgLib)
		{
			return ImgLib != nullptr && ImgLib->GetPath() == Path;
		}))
	{
		return;
	}

	// If this already exists in the global library, don't load it again
	if(const auto Library = LoadedImageLibraries.find(Path); Library != LoadedImageLibraries.end())
	{
		if(const auto Ptr = Library->second.lock())
		{
			ImageLibraries.push_back(Ptr);
			return;
		}
	}

	// Load, put in this image and put in global library
	const auto Ptr = std::make_shared<WatchedFileImageLibrary>(Path, OverrideName);
	ImageLibraries.push_back(Ptr);
	LoadedImageLibraries[Path] = Ptr;
}

std::vector<StrId> CharacterControlRender::CharacterImageLibrary::GetValidImageNames() const
{
	std::vector<StrId> List;

	for(const auto& ImageLibrary : ImageLibraries)
	{
		const std::vector<StrId>& Names = ImageLibrary->GetImageNames();
		List.insert(List.end(), Names.begin(), Names.end());
	}

	return List;
}

gs_image_file_ex* CharacterControlRender::CharacterImageLibrary::GetImageFromName(StrIdRef Name) const
{
	for(const auto& ImageLibrary : ImageLibraries)
	{
		if(const FileImageLibrary* ImageFile = ImageLibrary->Get())
		{
			if(const auto FoundIt = ImageFile->find(Name); FoundIt != ImageFile->end())
			{
				return FoundIt->second.Image.get();
			}
		}
	}

	return nullptr;
}

const CharacterControlRender::FileImageLibrary* CharacterControlRender::CharacterImageLibrary::GetLayers(FilePathRef Path) const
{
	for(const auto& ImageLibrary : ImageLibraries)
	{
		if(ImageLibrary != nullptr && ImageLibrary->GetPath() == Path)
		{
			return ImageLibrary->Get();
		}
	}

	return nullptr;
}

b2Vec2 CharacterControlRender::CharacterImageLibrary::GetMaxResolution() const
{
	b2Vec2 Size{.x = 0, .y = 0};
	for(const auto& LibraryPtr : ImageLibraries)
	{
		if(const WatchedFileImageLibrary* Library = LibraryPtr.get())
		{
			auto [X, Y] = Library->GetMaxResolution();
			Size.x = std::max(Size.x, X);
			Size.y = std::max(Size.y, Y);
		}
	}

	return Size;
}
