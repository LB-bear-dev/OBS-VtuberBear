#include "PCH.h"
#include "Render/WatchedImage.h"

#include <array>
#include <memory>
#include <optional>
#include <obs-module.h>

#include "CharacterControlCore/Messaging.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "Psd/Psd.h"
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
#include "Psd/PsdPlatform.h"
#include "Psd/PsdSyncFileReader.h"
#include "Psd/PsdSyncFileUtil.h"
#include "Psd/PsdVectorMask.h"

std::unordered_map<FilePath, std::weak_ptr<CharacterControlRender::WatchedFileImageLibrary>> CharacterControlRender::CharacterImageLibrary::LoadedImageLibraries;
namespace
{
// psd_sdk sizes its buffers with 32-bit width*height math, these should be the limits based on those constraints.
constexpr unsigned int MAX_PSD_SIDE = 16384;
constexpr int64_t MAX_LAYER_PIXELS = 256ll * 1024 * 1024;
static_assert(MAX_LAYER_PIXELS < 1ll << 32, "psd_sdk multiplies width and height in 32 bits");

// The most output a decoder can produce per input byte
constexpr uint64_t MAX_DECODE_RATIO = 1032;

void UnloadImage(gs_image_file_ex* Image)
{
	if(Image != nullptr)
	{
		obs_enter_graphics();
		gs_image_file_ex_free(Image);
		obs_leave_graphics();
	}
}

bool LoadImage(const char* FileName, gs_image_file_ex* Image)
{
	UnloadImage(Image);

	if(FileName != nullptr && strlen(FileName) > 0)
	{
		DEBUG_("loading texture '%s'", FileName);
		gs_image_file_ex_init(Image, FileName, GS_IMAGE_ALPHA_PREMULTIPLY);

		obs_enter_graphics();
		gs_image_file_ex_init_texture(Image);
		obs_leave_graphics();

		if(!Image->loaded)
		{
			WARN_("failed to load texture '%s'", FileName);
			return false;
		}
	} else
	{
		return false;
	}

	return true;
}

std::string GetLayerName(const psd::Layer& Layer)
{
	std::string LayerName;
	if(Layer.utf16Name)
	{
		const auto WideName = reinterpret_cast<const wchar_t*>(Layer.utf16Name);
		const int Size = WideCharToMultiByte(CP_UTF8, 0, WideName, -1, nullptr, 0, nullptr, nullptr);
		if(Size > 1)
		{
			LayerName.resize(Size - 1);
			WideCharToMultiByte(CP_UTF8, 0, WideName, -1, LayerName.data(), Size, nullptr, nullptr);
		}
	} else
	{
		LayerName = Layer.name.c_str();
	}

	return LayerName;
}

// Layer name prefixed by the groups it is in, e.g. "Head/Mouth/Open", so same-named layers in different groups stay distinct
std::string GetLayerPath(const psd::Layer& Layer)
{
	if(Layer.parent == nullptr)
	{
		return GetLayerName(Layer);
	}

	return GetLayerPath(*Layer.parent) + "/" + GetLayerName(Layer);
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

	auto Data = static_cast<const uint8_t*>(MaskData->data);
	return Data[(Y - MaskData->top) * (MaskData->right - MaskData->left) + (X - MaskData->left)];
}

// The rectangle has to be well formed and not too big, and the channel can't decode to more pixels than the bytes it has allow
template<typename Rect>
bool ChannelFitsStream(const Rect& Bounds, const psd::Channel& Channel, const uint64_t FileSize)
{
	const int64_t Width = static_cast<int64_t>(Bounds.right) - Bounds.left;
	const int64_t Height = static_cast<int64_t>(Bounds.bottom) - Bounds.top;

	// Each side is checked on its own first so the product below cannot overflow
	if(Width < 0 || Height < 0 || Width > MAX_LAYER_PIXELS || Height > MAX_LAYER_PIXELS)
	{
		return false;
	}

	// A channel always holds at least its 2-byte compression type; a shorter one makes psd_sdk size a buffer from a negative length
	if(Channel.size < 2)
	{
		return false;
	}

	const int64_t Pixels = Width * Height;
	return Pixels <= MAX_LAYER_PIXELS && static_cast<uint64_t>(Pixels) <= std::min<uint64_t>(Channel.size, FileSize) * MAX_DECODE_RATIO;
}

// Checks every channel against the rectangle psd_sdk will size its data from
bool LayerFitsStream(const psd::Layer& Layer, const uint64_t FileSize)
{
	for(unsigned int ChannelIndex = 0; ChannelIndex < Layer.channelCount; ++ChannelIndex)
	{
		const psd::Channel& Channel = Layer.channels[ChannelIndex];

		bool Fits = true;
		if(Channel.type == psd::channelType::LAYER_OR_VECTOR_MASK)
		{
			if(Layer.vectorMask != nullptr)
			{
				Fits = ChannelFitsStream(*Layer.vectorMask, Channel, FileSize);
			} else if(Layer.layerMask != nullptr)
			{
				Fits = ChannelFitsStream(*Layer.layerMask, Channel, FileSize);
			}
		} else if(Channel.type == psd::channelType::LAYER_MASK)
		{
			// A channel of a mask the layer doesn't have can't be extracted
			Fits = Layer.layerMask != nullptr && ChannelFitsStream(*Layer.layerMask, Channel, FileSize);
		} else
		{
			Fits = ChannelFitsStream(Layer, Channel, FileSize);
		}

		if(!Fits)
		{
			return false;
		}
	}

	return true;
}

// The channels that make up the picture: red, green, blue and transparency. The others are masks, which ExtractLayer moves into
// the layer's masks, or leaves where they were, sized for a mask, when the layer has none to move them to.
bool IsImageChannel(const psd::Channel& Channel)
{
	return Channel.type == psd::channelType::TRANSPARENCY_MASK || (Channel.type >= psd::channelType::R && Channel.type <= psd::channelType::B);
}

// ExtractLayer leaves a channel without data when it can't decode it, such as one with an unknown compression type.
// Same channels the copy loop in LoadPsd picks up.
bool HasUndecodedChannel(const psd::Layer& Layer)
{
	for(unsigned int ChannelIndex = 0; ChannelIndex < Layer.channelCount; ++ChannelIndex)
	{
		const psd::Channel& Channel = Layer.channels[ChannelIndex];
		if(IsImageChannel(Channel) && Channel.data == nullptr)
		{
			return true;
		}
	}

	return false;
}

// Layers can potentially declare their size to be larger than the whole file. Sanity check this (discovered in fuzz testing)
bool LayerSectionFitsFile(const psd::Document& Document, psd::File& File, const uint64_t FileSize)
{
	const auto& [offset, length] = Document.layerMaskInfoSection;
	const uint64_t LengthSize = Document.isLargeDocument ? 8 : 4;

	// The Document's offset is just after the length field, which can itself be past the end of a short file
	if(offset < LengthSize || offset > FileSize)
	{
		return false;
	}

	psd::SyncFileReader Reader(&File);
	Reader.SetPosition(offset - LengthSize);
	const uint64_t Declared = Document.isLargeDocument ? psd::fileUtil::ReadFromFileBE<uint64_t>(Reader) : psd::fileUtil::ReadFromFileBE<uint32_t>(Reader);

	return Declared <= FileSize - offset;
}

// These are all convenience tools to clean up psd resources after scoped use.
struct DocumentDeleter
{
	psd::Allocator* Allocator = nullptr;

	void operator()(psd::Document* Document) const
	{
		DestroyDocument(Document, Allocator);
	}
};

using DocumentPtr = std::unique_ptr<psd::Document, DocumentDeleter>;

struct LayerMaskSectionDeleter
{
	psd::Allocator* Allocator = nullptr;

	void operator()(psd::LayerMaskSection* Section) const
	{
		DestroyLayerMaskSection(Section, Allocator);
	}
};

struct PlaneDeleter
{
	psd::Allocator* Allocator = nullptr;

	void operator()(uint8_t* Plane) const
	{
		Allocator->Free(Plane);
	}
};

using PlanePtr = std::unique_ptr<uint8_t, PlaneDeleter>;

// Reads the document and checks it is one this loader handles, logging why when it isn't
DocumentPtr ParseSupportedDocument(const char* FileName, psd::NativeFile& File, psd::Allocator& Allocator)
{
	DocumentPtr Document(CreateDocument(&File, &Allocator), DocumentDeleter{&Allocator});
	if(!Document)
	{
		ERROR_("Cannot create document for %s.", FileName);
		return nullptr;
	}

	if(Document->bitsPerChannel != 8 || Document->colorMode != psd::colorMode::RGB)
	{
		ERROR_("%s: only 8-bit RGB PSDs are supported.", FileName);
		return nullptr;
	}

	if(Document->width == 0 || Document->height == 0 || Document->width > MAX_PSD_SIDE || Document->height > MAX_PSD_SIDE)
	{
		ERROR_("%s: canvas is %ux%u, the largest supported is %ux%u.", FileName, Document->width, Document->height, MAX_PSD_SIDE, MAX_PSD_SIDE);
		return nullptr;
	}

	if(!LayerSectionFitsFile(*Document, File, File.GetSize()))
	{
		ERROR_("%s: the file ends before its layers do. It may still be being saved.", FileName);
		return nullptr;
	}

	return Document;
}

// Copies the layer's extracted channels into an RGBA image of its part of the canvas, Left/Top to Right/Bottom. The image owns
// its pixels (bmalloc) but has no texture yet.
std::unique_ptr<gs_image_file_ex> MakeLayerImage(const psd::Layer& Layer, psd::Allocator& Allocator, const int Left, const int Top, const int Right, const int Bottom)
{
	constexpr int ALPHA_PLANE = 3;
	const unsigned int Width = Right - Left;
	const unsigned int Height = Bottom - Top;
	const size_t PlaneSize = sizeof(uint8_t) * Width * Height;

	// InterleaveRGBA reads the planes 16 bytes at a time, so they come from psd_sdk's aligned allocator
	const auto NewPlane = [&Allocator, PlaneSize](const uint8_t Fill)
	{
		PlanePtr Plane(static_cast<uint8_t*>(Allocator.Allocate(PlaneSize, 16u)), PlaneDeleter{&Allocator});
		memset(Plane.get(), Fill, PlaneSize);
		return Plane;
	};

	std::array<PlanePtr, 4> Planes;
	for(unsigned int ChannelIndex = 0; ChannelIndex < Layer.channelCount; ++ChannelIndex)
	{
		const psd::Channel& Channel = Layer.channels[ChannelIndex];

		if(IsImageChannel(Channel))
		{
			PlanePtr& Plane = Planes[Channel.type == psd::channelType::TRANSPARENCY_MASK ? ALPHA_PLANE : Channel.type];
			Plane = NewPlane(0);
			psd::imageUtil::CopyLayerData(static_cast<const uint8_t*>(Channel.data),
				Plane.get(),
				Layer.left - Left,
				Layer.top - Top,
				Layer.right - Left,
				Layer.bottom - Top,
				Width,
				Height);
		}
	}

	// The texture is always RGBA, whatever channels the layer had. Fill with black for RGB and opaque for A.
	for(int PlaneIndex = 0; PlaneIndex < 4; ++PlaneIndex)
	{
		if(!Planes[PlaneIndex])
		{
			Planes[PlaneIndex] = NewPlane(PlaneIndex == ALPHA_PLANE ? 255u : 0u);
		}
	}

	auto Image = std::make_unique<gs_image_file_ex>();
	Image->space = GS_CS_SRGB;
	Image->alpha_mode = GS_IMAGE_ALPHA_PREMULTIPLY;
	Image->format = GS_RGBA;
	Image->cx = Width;
	Image->cy = Height;
	Image->mem_usage = static_cast<size_t>(Width) * Height * 4 * sizeof(uint8_t);
	Image->texture_data = static_cast<uint8_t*>(bmalloc(Image->mem_usage));

	psd::imageUtil::InterleaveRGBA(Planes[0].get(), Planes[1].get(), Planes[2].get(), Planes[3].get(), Image->texture_data, Width, Height);

	return Image;
}

// Bakes the layer's masks and opacity into the alpha of its RGBA pixels, which cover Left/Top to Right/Bottom of the canvas, then
// premultiplies the color by it
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

// Crops the fully transparent borders off the image. Gives back where the part it kept starts in the image, or nothing when no
// pixel is visible.
std::optional<CharacterControlRender::Coord> TrimTransparentBorders(gs_image_file_ex& Image)
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
	const unsigned int TrimmedHeight = MaxY - MinY + 1;
	if(TrimmedWidth != Width || TrimmedHeight != Height)
	{
		// Pack the kept rows at the start of the buffer; each row moves to an earlier or equal address
		for(unsigned int Row = 0; Row < TrimmedHeight; ++Row)
		{
			memmove(Image.texture_data + static_cast<size_t>(Row) * TrimmedWidth * 4,
				Image.texture_data + (static_cast<size_t>(MinY + Row) * Width + MinX) * 4,
				static_cast<size_t>(TrimmedWidth) * 4);
		}

		Image.cx = TrimmedWidth;
		Image.cy = TrimmedHeight;
		Image.mem_usage = static_cast<size_t>(TrimmedWidth) * TrimmedHeight * 4 * sizeof(uint8_t);
	}

	return CharacterControlRender::Coord{static_cast<float>(MinX), static_cast<float>(MinY)};
}

bool LoadPsd(const char* FileName, CharacterControlRender::FileImageLibrary& ImageLibrary, CharacterControlRender::Coord& CanvasSize)
{
	std::array<wchar_t, MAX_PATH> WideFilename = {};
	size_t Unused;
	if(mbstowcs_s(&Unused, WideFilename.data(), WideFilename.size(), FileName, MAX_PATH) != 0)
	{
		ERROR_("Cannot convert file name %s.", FileName);
		return false;
	}

	psd::MallocAllocator Allocator;
	psd::NativeFile File(&Allocator);

	// try opening the file. if it fails, bail out.
	if(!File.OpenRead(WideFilename.data()))
	{
		ERROR_("Cannot open file %s.\n", FileName);
		return false;
	}

	// psd::File doesn't close itself. Declared before the document and its layers, so it closes after they are gone.
	struct FileCloser
	{
		psd::File& File;

		~FileCloser()
		{
			File.Close();
		}
	} CloseFile{File};

	const DocumentPtr Document = ParseSupportedDocument(FileName, File, Allocator);
	if(!Document)
	{
		return false;
	}

	const std::unique_ptr<psd::LayerMaskSection, LayerMaskSectionDeleter> LayerMaskSection(psd::ParseLayerMaskSection(Document.get(), &File, &Allocator),
		LayerMaskSectionDeleter{&Allocator});
	if(!LayerMaskSection)
	{
		ERROR_("Cannot read the layers of %s.", FileName);
		return false;
	}

	CanvasSize = {static_cast<float>(Document->width), static_cast<float>(Document->height)};
	const uint64_t FileSize = File.GetSize();

	for(unsigned int LayerIndex = 0; LayerIndex < LayerMaskSection->layerCount; ++LayerIndex)
	{
		psd::Layer& Layer = LayerMaskSection->layers[LayerIndex];

		// Other layer types are like folders or dividers, non image things.
		if(Layer.type != psd::layerType::ANY)
		{
			continue;
		}

		if(!LayerFitsStream(Layer, FileSize))
		{
			WARN_("%s: skipping layer %u, its size does not fit the file.", FileName, LayerIndex);
			continue;
		}

		// Each texture only covers the part of the layer that is on the canvas
		const int Left = std::max(Layer.left, 0);
		const int Top = std::max(Layer.top, 0);
		const int Right = std::min(Layer.right, static_cast<int>(Document->width));
		const int Bottom = std::min(Layer.bottom, static_cast<int>(Document->height));
		if(Right <= Left || Bottom <= Top)
		{
			continue;
		}

		psd::ExtractLayer(Document.get(), &File, &Allocator, &Layer);

		if(HasUndecodedChannel(Layer))
		{
			WARN_("%s: skipping layer %u, one of its channels could not be decoded.", FileName, LayerIndex);
			continue;
		}

		std::unique_ptr<gs_image_file_ex> Image = MakeLayerImage(Layer, Allocator, Left, Top, Right, Bottom);
		BakeMasksAndOpacity(Layer, Image->texture_data, Left, Top, Right, Bottom);

		// Trimmed after masks and opacity have had their say
		const std::optional<CharacterControlRender::Coord> Kept = TrimTransparentBorders(*Image);
		if(!Kept)
		{
			// Nothing visible
			bfree(Image->texture_data);
			continue;
		}

		const CharacterControlRender::Coord Offset{static_cast<float>(Left) + Kept->X, static_cast<float>(Top) + Kept->Y};

		Image->loaded = true;
		obs_enter_graphics();
		gs_image_file_ex_init_texture(Image.get());
		obs_leave_graphics();

		std::string LayerPath = GetLayerPath(Layer);

		// only add new layers. try_emplace leaves Image alone when the name is taken.
		if(!ImageLibrary.try_emplace(LayerPath, std::move(Image), static_cast<int>(LayerIndex), Offset).second)
		{
			WARN_("%s: more than one layer is named '%s'; only the first is used.", FileName, LayerPath.c_str());
			UnloadImage(Image.get());
		}
	}

	return true;
}
}

const std::vector<std::string>& CharacterControlRender::WatchedFileImageLibrary::GetImageNames() const
{
	return ImageNames;
}

CharacterControlRender::Coord CharacterControlRender::WatchedFileImageLibrary::GetMaxResolution() const
{
	return MaxResolution;
}

bool CharacterControlRender::WatchedFileImageLibrary::UpdateInternal()
{
	bool Success;
	if(GetPath().extension() == ".psd")
	{
		Success = LoadPsd(GetPath().string().c_str(), *GetMutable(), MaxResolution);
	} else
	{
		const std::string& Name = !OverrideName.empty() ? OverrideName : GetPath().stem().string();
		FileLayer& AddedLayer = GetMutable()->emplace(Name, FileLayer{std::make_unique<gs_image_file_ex>(), 0, {0, 0}}).first->second;
		Success = LoadImage(this->GetPath().string().c_str(), AddedLayer.Image.get());
	}

	// update valid image names
	ImageNames.clear();
	if(const FileImageLibrary* Library = GetMutable())
	{
		for(auto& [name, fileLayer] : *Library)
		{
			if(fileLayer.Image != nullptr)
			{
				ImageNames.push_back(name);
				MaxResolution.X = std::max(MaxResolution.X, fileLayer.Offset.X + static_cast<float>(fileLayer.Image->cx));
				MaxResolution.Y = std::max(MaxResolution.Y, fileLayer.Offset.Y + static_cast<float>(fileLayer.Image->cy));
			}
		}
	}

	return Success;
}

void CharacterControlRender::WatchedFileImageLibrary::DisposeInternal()
{
	if(GetMutable() != nullptr && !GetMutable()->empty())
	{
		for(auto& [name, fileLayer] : *GetMutable())
		{
			UnloadImage(fileLayer.Image.get());
		}
	}
}

void CharacterControlRender::CharacterImageLibrary::AddImageLibrary(FilePathRef Path)
{
	// An empty override name keeps the file's own name
	AddImageLibrary(Path, "");
}

void CharacterControlRender::CharacterImageLibrary::AddImageLibrary(FilePathRef Path, StrIdRef OverrideName)
{
	// Already exists
	for(auto& ImgLib : ImageLibraries)
	{
		if(ImgLib != nullptr && ImgLib->GetPath() == Path)
		{
			return;
		}
	}

	// If this already exists in the global library, don't load it again
	auto Library = LoadedImageLibraries.find(Path);
	if(Library != LoadedImageLibraries.end() && !Library->second.expired())
	{
		if(auto Ptr = Library->second.lock())
		{
			ImageLibraries.push_back(Ptr);
		}
	} else
	{
		// Load, put in this image and put in global library
		auto Ptr = std::make_shared<WatchedFileImageLibrary>(Path, OverrideName);
		ImageLibraries.push_back(Ptr);
		LoadedImageLibraries[Path] = Ptr;
	}
}

std::vector<std::string> CharacterControlRender::CharacterImageLibrary::GetValidImageNames() const
{
	std::vector<std::string> List;

	for(auto& ImageLibrary : ImageLibraries)
	{
		List.insert(List.end(), ImageLibrary->GetImageNames().begin(), ImageLibrary->GetImageNames().end());
	}

	return List;
}

gs_image_file_ex* CharacterControlRender::CharacterImageLibrary::GetImageFromName(StrIdRef Name) const
{
	for(auto& ImageLibrary : ImageLibraries)
	{
		if(const FileImageLibrary* ImageFile = ImageLibrary->Get())
		{
			auto FoundIt = ImageFile->find(Name);
			if(FoundIt != ImageFile->end())
			{
				return FoundIt->second.Image.get();
			}
		}
	}

	return nullptr;
}

const CharacterControlRender::FileImageLibrary* CharacterControlRender::CharacterImageLibrary::GetLayers(FilePathRef Path) const
{
	for(auto& ImageLibrary : ImageLibraries)
	{
		if(ImageLibrary != nullptr && ImageLibrary->GetPath() == Path)
		{
			return ImageLibrary->Get();
		}
	}

	return nullptr;
}

CharacterControlRender::Coord CharacterControlRender::CharacterImageLibrary::GetMaxResolution() const
{
	Coord Size{0, 0};
	for(auto& LibraryPtr : ImageLibraries)
	{
		if(const WatchedFileImageLibrary* Library = LibraryPtr.get())
		{
			auto [X, Y] = Library->GetMaxResolution();
			Size.X = std::max(Size.X, X);
			Size.Y = std::max(Size.Y, Y);
		}
	}

	return Size;
}
