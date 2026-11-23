// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/CoreTypes.h"
#include "CharacterRig/Rig.h"
#include "CharacterRig/Style.h"

namespace CharacterControlOBS
{
	// Root definition for a particular object that has visual states and different visual pieces
	struct LoadedCharacter
	{
		// Display name for the character. Whatever you want this character to show up as in UI screens and OBS windows
		Str Name;

		// Definition of what pieces this character is made up of, what different parts can be swapped to form reactions,
		// What conditions cause parts to be swapped. This forms the "contract" of the character, all slots of each layer 
		// is the list of images which should have a definition somewhere. The rig is external to a character because
		// it can be reused among characters.
		Rig RigDefinition;
		
		// Mapping of this specific character's images to the rig. Sparse and partial implementations are accepted, 
		// in the way that multiple styles can be active at once, so a layer's slot will look through any active style
		// to apply an image. If a layer ends up with a slot no active style fills, the layer will have no image.
		std::vector<Style> StyleDefinitions;
	};

	std::optional<LoadedCharacter> LoadCharacterFile(FilePathRef Folder);

	std::optional<FilePath> FindCharacterFolder(FilePathRef CharactersFolder, StrIdRef DiscordId);
}