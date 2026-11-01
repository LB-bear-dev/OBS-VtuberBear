#pragma once
#include <map>
#include "Render/Coord.h"
#include "Render/WatchedImage.h"

namespace CharacterControlRender
{
class ImageLayer
{
public:
	ImageLayer();

	void CreateChildLayer(StrIdRef Layer);
	void CreateSlot(StrIdRef Slot);

	void AddImageLibraryToLayer(FilePathRef Path);
	void AddImageLibraryToLayer(FilePathRef Path, StrIdRef OverrideName);

	// Adds one child per layer in the file, in the file's order, named after the layer and
	// positioned at its offset. A plain image file is a single layer.
	void AddLayersFromFile(FilePathRef Path, const Coord& Offset = {});

	void Activate();
	void Deactivate();

	void Render(gs_effect_t* Effect) const;

	ImageLayer* GetLayerByName(StrIdRef Name);

	[[nodiscard]] Coord GetMaxResolution() const;

	void SetSlot(StrIdRef SlotIn, StrIdRef NameIn);

	[[nodiscard]] const Coord& GetTranslation() const;
	void SetTranslation(const Coord& NewTranslation);

	[[nodiscard]] const Coord& GetScale() const;
	void SetScale(const Coord& NewScale);

	[[nodiscard]] float GetRotation() const;
	void SetRotation(float NewRotation);

private:
	const ImageLayer* GetNextActiveChildLayer(uint32_t& StartIndex) const;
	void RenderTree(gs_eparam_t* ImageParam) const;
	void RefreshActiveImageFromSlots();
	// Looks up ActiveImage again. Call whenever ActiveImageName or ImageLibraries change.
	void UpdateActiveImage();

	StrId LayerName;

	struct SlotName
	{
		StrId Slot;
		StrId Name;
	};
	std::vector<SlotName> SlotNames;

	StrId ActiveImageName;
	// The image ActiveImageName names, looked up ahead of time so rendering doesn't search for it every frame.
	// Owned by ImageLibraries, whose files are loaded once and never reloaded.
	const gs_image_file_ex* ActiveImage = nullptr;

	bool Active = true;

	Coord Translation;
	Coord Scale;
	float Rotation = 0.0f;

	CharacterImageLibrary ImageLibraries;
	std::vector<ImageLayer> ChildLayersByZOrder;
};
}
