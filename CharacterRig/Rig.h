// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <map>
#include <vector>
#include "CharacterControlCore/CoreTypes.h"
#include "CharacterRig/Attribute.h"
#include "CharacterRig/Condition.h"

namespace CharacterControlOBS
{
using SlotValues = std::map<StrId, StrId>;

struct RigLayer
{
	// Identifier for the layer, mostly to help UI
	StrId Name;

	// If any, should this layer be relative to any other layer?
	StrId Parent;

	// Which slot this layer is driven by
	StrId Slot;

	bool operator==(const RigLayer&) const = default;
};

struct SlotRule
{
	// What value
	StrId Value;

	// What needs to be true to set this value
	Condition SlotCondition;

	bool operator==(const SlotRule&) const = default;
};

struct RigSlot
{
	// Identifies what this slot is meant to switch between
	StrId Name;

	// Logic that describes how this slot changes between different values.
	std::vector<SlotRule> Rules;

	// If the rules fall through, this is what this slot will end up with
	StrId Default;

	bool operator==(const RigSlot&) const = default;
};

// Rigs are composed of layers and slots. Layers break up the visual into separate areas and slots define how those layers can change depending
// on the state of the character.
struct Rig
{
	// Visual regions on the rig, like "body", "head", "hat" etc. They occupy space with a named purpose, and can form a hierarchy of child layers
	// who are placed relative to the parent layers
	std::vector<RigLayer> Layers;

	// Slots are names for states that layers could be in. "Speaking", "Joined", "Excited" etc. They are adjectives or verbs that can describe the
	// state of the layers
	std::vector<RigSlot> Slots;

	// Loads from disk and sets up rig file. Always tries to make the rig even if the data is incomplete/malformed
	// If the file is totally unusable it will return a blank rig with no layers or slots
	static Rig Load(FilePathRef Path);

	// Saves the rig file to disk
	bool Save(FilePathRef Path) const;

	// Figure out which slots should be set on which layers, based on the attributes this rig has set on it
	SlotValues EvaluateSlots(const AttributeMap& Attributes) const;

	bool operator==(const Rig&) const = default;
};
}
