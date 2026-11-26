// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Render/ImageLayer.h"

using namespace CharacterControlRender;

ImageLayer::ImageLayer() : Translation({.x = 0, .y = 0}), Scale({.x = 1, .y = 1}) {}

void ImageLayer::CreateChildLayer(StrIdRef Layer)
{
	ImageLayer Child;
	Child.LayerName = Layer;
	ChildLayersByZOrder.push_back(Child);
}

void ImageLayer::CreateSlot(StrIdRef Slot)
{
	SlotNames.push_back({.Slot = Slot, .Name = u8""});
	for(ImageLayer& Layer : ChildLayersByZOrder)
	{
		Layer.CreateSlot(Slot);
	}
}

void ImageLayer::AddImageLibraryToLayer(FilePathRef Path)
{
	ImageLibraries.AddImageLibrary(Path);
	UpdateActiveImage();
}

void ImageLayer::AddImageLibraryToLayer(FilePathRef Path, StrIdRef OverrideName)
{
	ImageLibraries.AddImageLibrary(Path, OverrideName);
	UpdateActiveImage();
}

void ImageLayer::AddLayersFromFile(FilePathRef Path, const b2Vec2& Offset)
{
	AddImageLibraryToLayer(Path);

	const FileImageLibrary* FileLayers = ImageLibraries.GetLayers(Path);
	if(FileLayers == nullptr)
	{
		return;
	}

	std::vector<const FileImageLibrary::value_type*> Ordered;
	for(const auto& Entry : *FileLayers)
	{
		Ordered.push_back(&Entry);
	}

	std::ranges::sort(Ordered,
		[](const FileImageLibrary::value_type* Lhs, const FileImageLibrary::value_type* Rhs)
		{
			return Lhs->second.ZOrder < Rhs->second.ZOrder;
		});

	for(const FileImageLibrary::value_type* Entry : Ordered)
	{
		ImageLayer& Child = ChildLayersByZOrder.emplace_back();
		Child.LayerName = Entry->first;
		Child.ActiveImageName = Entry->first;
		Child.Translation = {.x = Entry->second.Offset.x + Offset.x, .y = Entry->second.Offset.y + Offset.y};
		Child.AddImageLibraryToLayer(Path);
	}
}

void ImageLayer::Activate()
{
	Active = true;
}

void ImageLayer::Deactivate()
{
	Active = false;
}

void ImageLayer::Render(const gs_effect_t* Effect) const
{
	if(!Active)
	{
		return;
	}

	const bool Previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	RenderTree(gs_effect_get_param_by_name(Effect, "image"));

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(Previous);
}

ImageLayer* ImageLayer::GetLayerByName(StrIdRef Name)
{
	const auto FoundLayer = std::ranges::find_if(ChildLayersByZOrder,
		[&Name](const ImageLayer& Layer)
		{
			return Layer.LayerName == Name;
		});

	if(FoundLayer == ChildLayersByZOrder.end())
	{
		return nullptr;
	}

	return &*FoundLayer;
}

b2Vec2 ImageLayer::GetMaxResolution() const
{
	b2Vec2 MaxRes = ImageLibraries.GetMaxResolution();

	for(const auto& ChildLayer : ChildLayersByZOrder)
	{
		auto [X, Y] = ChildLayer.GetMaxResolution();
		MaxRes.x = std::max(MaxRes.x, X);
		MaxRes.y = std::max(MaxRes.y, Y);
	}

	return MaxRes;
}

void ImageLayer::SetSlot(StrIdRef SlotIn, StrIdRef NameIn)
{
	bool Changed = false;
	for(auto& [Slot, Name] : SlotNames)
	{
		if(Slot == SlotIn && Name != NameIn)
		{
			Name = NameIn;
			Changed = true;
		}
	}

	if(Changed)
	{
		RefreshActiveImageFromSlots();
	}

	for(ImageLayer& Layer : ChildLayersByZOrder)
	{
		Layer.SetSlot(SlotIn, NameIn);
	}
}

const b2Vec2& ImageLayer::GetTranslation() const
{
	return Translation;
}

void ImageLayer::SetTranslation(const b2Vec2& NewTranslation)
{
	Translation = NewTranslation;
}

const b2Vec2& ImageLayer::GetScale() const
{
	return Scale;
}

void ImageLayer::SetScale(const b2Vec2& NewScale)
{
	Scale = NewScale;
}

float ImageLayer::GetRotation() const
{
	return Rotation;
}

void ImageLayer::SetRotation(const float NewRotation)
{
	Rotation = NewRotation;
}

const ImageLayer* ImageLayer::GetNextActiveChildLayer(uint32_t& StartIndex) const
{
	for(; StartIndex < ChildLayersByZOrder.size(); ++StartIndex)
	{
		if(ChildLayersByZOrder[StartIndex].Active)
		{
			return &ChildLayersByZOrder[StartIndex++];
		}
	}

	return nullptr;
}

void ImageLayer::RenderTree(gs_eparam_t* ImageParam) const
{
	gs_matrix_push();
	gs_matrix_translate3f(Translation.x, Translation.y, 0);

	if(ActiveImage != nullptr)
	{
		if(gs_texture_t* Texture = ActiveImage->texture)
		{
			gs_effect_set_texture_srgb(ImageParam, Texture);
			gs_draw_sprite(Texture, 0, ActiveImage->cx, ActiveImage->cy);
		}
	}

	uint32_t Index = 0;
	while(const auto* Layer = GetNextActiveChildLayer(Index))
	{
		Layer->RenderTree(ImageParam);
	}
	gs_matrix_pop();
}

void ImageLayer::RefreshActiveImageFromSlots()
{
	ActiveImageName.clear();

	for(StrIdRef ImageName : ImageLibraries.GetValidImageNames())
	{
		for(const auto& [Slot, Name] : SlotNames)
		{
			if(!Name.empty() && ImageName.find(Name) != StrId::npos)
			{
				ActiveImageName = ImageName;
				break;
			}
		}

		if(!ActiveImageName.empty())
		{
			break;
		}
	}

	UpdateActiveImage();
}

void ImageLayer::UpdateActiveImage()
{
	ActiveImage = ImageLibraries.GetImageFromName(ActiveImageName);
}
