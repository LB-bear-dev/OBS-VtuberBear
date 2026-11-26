// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <map>
#include <memory>
#include <vector>
#include <glaze/forward.hpp>
#include "CharacterControlCore/CoreTypes.h"
#include "CharacterRig/Rig.h"
#include "CharacterRig/Style.h"

namespace CharacterControlOBS
{
struct RigRef
{
	Str Name;

	std::shared_ptr<const Rig> LoadedRig;

	bool Load(FilePathRef Root, StrRef CharacterName);

	bool operator==(const RigRef& Other) const
	{
		return Name == Other.Name;
	}
};

struct CharacterFile
{
	// The files that compose the available layers and slots, and the conditions that choose which slots are
	// active based on attributes. Rigs are allowed to be composed out of multiple files, so you could make rigs
	// for particular features and compose them together to get the final set of functionality
	std::vector<RigRef> Rigs;

	// A list of alternative names for the character. Think like Twitch name, Discord ID etc, whatever another
	// attribute provider might need to like attributes it wants to set to a particular character.
	std::map<StrId, std::vector<StrId>> Aliases;

	// Mapping of this specific character's images to the rig. Sparse and partial implementations are accepted,
	// in the way that multiple styles can be active at once, so a layer's slot will look through any active style
	// to apply an image. If a layer ends up with a slot no active style fills, the layer will have no image.
	std::vector<Style> Styles;

	static CharacterFile Load(FilePathRef Root, StrRef Name);

	static void LoadAll(FilePathRef Root);

	bool Save(FilePathRef Folder) const;

	bool operator==(const CharacterFile&) const = default;
};

std::shared_ptr<const CharacterFile> FindCharacterByName(StrRef Name);

std::shared_ptr<const CharacterFile> FindCharacterByAlias(StrIdRef Service, StrIdRef Id);
}

template<>
struct glz::meta<CharacterControlOBS::RigRef>
{
	static constexpr auto value = &CharacterControlOBS::RigRef::Name;
};
