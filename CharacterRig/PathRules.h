#pragma once
#include <optional>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
// gets a filename from a path that is relative to a folder
[[nodiscard]] std::optional<FilePath> ResolveInsideFolder(FilePathRef Folder, const Str& Utf8Path);
}
