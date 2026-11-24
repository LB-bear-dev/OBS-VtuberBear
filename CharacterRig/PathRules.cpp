// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/PathRules.h"
#include <filesystem>
#include <vector>

std::optional<FilePath> CharacterControlOBS::ResolveInsideFolder(FilePathRef Folder, Utf8StrRef Path)
{
	const FilePath Relative = FilePath(std::u8string(Path.begin(), Path.end())).lexically_normal();

	// Make sure the relative path is actually relative. It can't try to escape the current folder or name a drive if it's relative.
	if(Relative.empty() || Relative == "." || Relative.has_root_name() || Relative.has_root_directory() || *Relative.begin() == FilePath(".."))
	{
		return {};
	}

	return Folder / Relative;
}

Utf8Str CharacterControlOBS::LogPath(FilePathRef Path)
{
	const std::u8string Utf8 = Path.u8string();
	return {Utf8.begin(), Utf8.end()};
}

FilePath CharacterControlOBS::RigsFolder(FilePathRef Root)
{
	return Root / "Rigs";
}

FilePath CharacterControlOBS::CharactersFolder(FilePathRef Root)
{
	return Root / "Characters";
}

std::optional<FilePath> CharacterControlOBS::CharacterFolder(FilePathRef Root, StrIdRef Name)
{
	return ResolveInsideFolder(CharactersFolder(Root), Name);
}

std::optional<FilePath> CharacterControlOBS::FindRig(FilePathRef Root, StrIdRef CharacterName, StrIdRef RigName)
{
	const Utf8Str FileName = RigName + ".Json";

	std::vector<FilePath> SearchFolders;
	if(const std::optional<FilePath> Character = CharacterFolder(Root, CharacterName))
	{
		SearchFolders.push_back(RigsFolder(*Character));
	}

	SearchFolders.push_back(RigsFolder(Root));

	for(const FilePath& Folder : SearchFolders)
	{
		const std::optional<FilePath> Path = ResolveInsideFolder(Folder, FileName);
		if(std::error_code Error; Path && is_regular_file(*Path, Error))
		{
			return Path;
		}
	}

	return {};
}
