#include "PCH.h"
#include "ImageLayer.h"

using namespace CharacterControlRender;

ImageLayer::ImageLayer() : Translation({0, 0}), Scale({1, 1}) {}

void ImageLayer::CreateChildLayer(StrIdRef Layer)
{
	ImageLayer Child;
	Child.LayerName = Layer;
	ChildLayersByZOrder.push_back(Child);
}

void ImageLayer::CreateSlot(StrIdRef Slot)
{
	SlotNames.push_back({Slot, ""});
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

void ImageLayer::AddLayersFromFile(FilePathRef Path, const Coord& Offset)
{
	AddImageLibraryToLayer(Path);

	const FileImageLibrary* FileLayers = ImageLibraries.GetLayers(Path);
	if(FileLayers == nullptr)
	{
		return;
	}

	// The library is keyed by name, so sort back into the file's order; the children vector keeps it from here
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
		Child.Translation = {Entry->second.Offset.X + Offset.X, Entry->second.Offset.Y + Offset.Y};
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

void ImageLayer::Render(gs_effect_t* Effect) const
{
	if(!Active)
	{
		return;
	}

	// Every layer draws with the same state, so set it once for the whole tree
	const bool Previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	RenderTree(gs_effect_get_param_by_name(Effect, "image"));

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(Previous);
}

void ImageLayer::RenderTree(gs_eparam_t* ImageParam) const
{
	gs_matrix_push();
	gs_matrix_translate3f(Translation.X, Translation.Y, 0);

	if(ActiveImage != nullptr)
	{
		if(gs_texture_t* Texture = ActiveImage->texture)
		{
			gs_effect_set_texture_srgb(ImageParam, Texture);
			gs_draw_sprite(Texture, 0, ActiveImage->cx, ActiveImage->cy);
		}
	}

	uint32_t Index = 0;
	while(auto Layer = GetNextActiveChildLayer(Index))
	{
		Layer->RenderTree(ImageParam);
	}
	gs_matrix_pop();
}

ImageLayer* ImageLayer::GetLayerByName(StrIdRef Name)
{
	auto FoundLayer = std::ranges::find_if(ChildLayersByZOrder,
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

Coord ImageLayer::GetMaxResolution() const
{
	Coord MaxRes = ImageLibraries.GetMaxResolution();

	for(auto& ChildLayer : ChildLayersByZOrder)
	{
		auto [X, Y] = ChildLayer.GetMaxResolution();
		MaxRes.X = std::max(MaxRes.X, X);
		MaxRes.Y = std::max(MaxRes.Y, Y);
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

	// Layers without this slot keep the image they already have
	if(Changed)
	{
		RefreshActiveImageFromSlots();
	}

	for(ImageLayer& Layer : ChildLayersByZOrder)
	{
		Layer.SetSlot(SlotIn, NameIn);
	}
}

const Coord& ImageLayer::GetTranslation() const
{
	return Translation;
}

void ImageLayer::SetTranslation(const Coord& NewTranslation)
{
	Translation = NewTranslation;
}

const Coord& ImageLayer::GetScale() const
{
	return Scale;
}

void ImageLayer::SetScale(const Coord& NewScale)
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

// Picks the first image whose name contains every slot's current value
void ImageLayer::RefreshActiveImageFromSlots()
{
	ActiveImageName.clear();

	for(const std::string& ImageName : ImageLibraries.GetValidImageNames())
	{
		bool MatchesAllSlots = std::ranges::all_of(SlotNames,
			[&ImageName](const SlotName& SlotEntry)
			{
				return ImageName.find(SlotEntry.Name) != std::string::npos;
			});

		if(MatchesAllSlots)
		{
			ActiveImageName = ImageName;
			break;
		}
	}

	UpdateActiveImage();
}

void ImageLayer::UpdateActiveImage()
{
	ActiveImage = ImageLibraries.GetImageFromName(ActiveImageName);
}
