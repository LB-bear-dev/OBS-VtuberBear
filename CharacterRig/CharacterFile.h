#pragma once
#include <optional>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
// What a character's character.json says. Paths are resolved inside the character's folder.
struct CharacterFile
{
	Str Name;
	FilePath Script;
	FilePath BaseStyle;
};

// Reads a character from the text of its character.json. On failure, logs why and gives back nothing.
[[nodiscard]] std::optional<CharacterFile> ParseCharacterFile(const Str& Text, FilePathRef Folder);

// Reads Folder's character.json
[[nodiscard]] std::optional<CharacterFile> LoadCharacterFile(FilePathRef Folder);
}
