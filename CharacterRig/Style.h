// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <map>
#include <optional>
#include "CharacterControlCore/CoreTypes.h"
#include "box2d/math_functions.h"

namespace CharacterControlOBS
{
	// Represents one discrete image. Could be a single file like a png, or a layer out of a PSD file.
	struct StyleImage
	{
		// Path to the image
		FilePath File;

		// If this is a PSD, which layer to use
		StrId Layer;

		bool operator==(const StyleImage&) const = default;
	};

	// Contains an image for any slots that would change the look of the layer
	// For instance you might have a "body" layer which would respond to "speaking" and "not speaking"
	// You could also have a "hat" layer which just has one image, for the hat. Swapping hats means swapping styles
	// but every hat may never change due to slots changing. Pivots and offsets let you put a hat in the same spot on the character
	// and sync it if that spot moves on the character as it animates
	struct StyleLayer
	{
		// If this should render by default
		bool Hidden = false;

		// Where this layer is relative to the rest of the character, relative to the pivot
		b2Vec2 Offset{};

		// If it exists, where the placement point is relative to the image.
		std::optional<b2Vec2> Pivot;

		// A mapping of all the images to each possible slot that could fill it. So this could be like "speaking" and "not speaking"
		std::map<StrId, StyleImage> SlotImages;

		bool operator==(const StyleLayer&) const = default;
	};

	// A style is one "look" for a character, and it is made of a sparse set of layers. They override layers
	// and map specific images to slots. Slots are basically events that happen while animations are playing,
	// styles swap out how the character looks, and they need to have images to respond to each slot that the character should look different for.
	struct Style
	{
		std::map<StrId, StyleLayer> Layers;

		bool operator==(const Style&) const = default;
	};

	std::optional<Style> ParseStyle(const Str& Text, FilePathRef Path, FilePathRef Folder);

	std::optional<Style> LoadStyle(FilePathRef Path, FilePathRef Folder);
}