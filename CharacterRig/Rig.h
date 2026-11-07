#pragma once
#include <optional>
#include <vector>
#include "CharacterControlCore/CoreTypes.h"
#include "CharacterRig/Condition.h"

namespace CharacterControlOBS
{
struct RigLayer
{
	// Identifier for the layer, mostly to help UI
	StrId Name;

	// If any, should this layer be relative to any other layer?
	StrId Parent;

	// Which slots are applicable to this layer?
	std::vector<StrId> Slots;

	bool operator==(const RigLayer&) const = default;
};

struct SlotRule
{
	// What value
	StrId Value;

	// What needs to be true to set this value
	Condition When;

	bool operator==(const SlotRule&) const = default;
};

struct RigSlot
{
	// Identifies what this slot is meant to switch between
	StrId Name;

	// The different states this slot can be in
	std::vector<StrId> Values;

	// Logic that describes how this slot changes between different values.
	std::vector<SlotRule> Rules;

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

	bool operator==(const Rig&) const = default;
};

[[nodiscard]] std::optional<Rig> ParseRig(const Str& Text, FilePathRef Path);

[[nodiscard]] std::optional<Rig> LoadRig(FilePathRef Path);
}
