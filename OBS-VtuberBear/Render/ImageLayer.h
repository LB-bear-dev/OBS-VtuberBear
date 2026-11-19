// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include "box2d/math_functions.h"
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

	void AddLayersFromFile(FilePathRef Path, const b2Vec2& Offset = {});

	void Activate();
	void Deactivate();

	void Render(const gs_effect_t* Effect) const;

	ImageLayer* GetLayerByName(StrIdRef Name);

	b2Vec2 GetMaxResolution() const;

	void SetSlot(StrIdRef SlotIn, StrIdRef NameIn);

	const b2Vec2& GetTranslation() const;
	void SetTranslation(const b2Vec2& NewTranslation);

	const b2Vec2& GetScale() const;
	void SetScale(const b2Vec2& NewScale);

	float GetRotation() const;
	void SetRotation(float NewRotation);

private:
	const ImageLayer* GetNextActiveChildLayer(uint32_t& StartIndex) const;
	void RenderTree(gs_eparam_t* ImageParam) const;
	void RefreshActiveImageFromSlots();
	void UpdateActiveImage();

	StrId LayerName;

	struct SlotName
	{
		StrId Slot;
		StrId Name;
	};
	std::vector<SlotName> SlotNames;

	StrId ActiveImageName;
	const gs_image_file_ex* ActiveImage = nullptr;

	bool Active = true;

	b2Vec2 Translation{};
	b2Vec2 Scale{};
	float Rotation = 0.0f;

	CharacterImageLibrary ImageLibraries;
	std::vector<ImageLayer> ChildLayersByZOrder;
};
}
