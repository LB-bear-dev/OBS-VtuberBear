#include "CharacterRig/PathRules.h"

std::optional<FilePath> CharacterControlOBS::ResolveInsideFolder(FilePathRef Folder, const Str& Utf8Path)
{
	const FilePath Relative = FilePath(std::u8string(Utf8Path.begin(), Utf8Path.end())).lexically_normal();

	// Make sure the relative path is actually relative. It can't try to escape the current folder or name a drive if it's relative.
	if(Relative.empty() || Relative == "." || Relative.has_root_name() || Relative.has_root_directory() || *Relative.begin() == FilePath(".."))
	{
		return std::nullopt;
	}

	return Folder / Relative;
}

Str CharacterControlOBS::LogPath(FilePathRef Path)
{
	const std::u8string Utf8 = Path.u8string();
	return {Utf8.begin(), Utf8.end()};
}
