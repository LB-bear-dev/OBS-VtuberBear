// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <optional>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
inline constexpr auto DefaultCharacterName = "default";

// gets a filename from a path that is relative to a folder
std::optional<FilePath> ResolveInsideFolder(FilePathRef Folder, Utf8StrRef Path);

Utf8Str LogPath(FilePathRef Path);

FilePath RigsFolder(FilePathRef Root);

FilePath CharactersFolder(FilePathRef Root);

std::optional<FilePath> CharacterFolder(FilePathRef Root, StrIdRef Name);

FilePath DefaultCharacterFolder(FilePathRef Root);

FilePath CharacterFilePath(FilePathRef Folder);

bool HasCharacterFile(FilePathRef Folder);

std::optional<FilePath> FindRig(FilePathRef Root, StrIdRef CharacterName, StrIdRef RigName);
}
