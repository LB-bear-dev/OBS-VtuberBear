// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <optional>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
inline constexpr auto DefaultCharacterName = u8"default";

// gets a filename from a path that is relative to a folder
std::optional<FilePath> ResolveInsideFolder(FilePathRef Folder, StrRef Path);

Str LogPath(FilePathRef Path);

FilePath RigsFolder(FilePathRef Root);

FilePath CharactersFolder(FilePathRef Root);

std::optional<FilePath> CharacterFolder(FilePathRef Root, StrRef Name);

FilePath DefaultCharacterFolder(FilePathRef Root);

FilePath CharacterFilePath(FilePathRef Folder);

bool HasCharacterFile(FilePathRef Folder);

std::optional<FilePath> FindRigPath(FilePathRef Root, StrRef CharacterName, StrRef RigName);
}
