// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <optional>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
	// gets a filename from a path that is relative to a folder
	std::optional<FilePath> ResolveInsideFolder(FilePathRef Folder, const Str& Utf8Path);

	Str LogPath(FilePathRef Path);
}