#pragma once
#include <optional>

namespace CharacterControlOBS
{
// gets a filename from a path that is relative to a folder
[[nodiscard]] std::optional<FilePath> ResolveInsideFolder(FilePathRef Folder, const Str& Utf8Path);
}
