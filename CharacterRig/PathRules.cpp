// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/PathRules.h"
#include <filesystem>
#include <vector>

std::optional<FilePath> CharacterControlOBS::ResolveInsideFolder(FilePathRef Folder, StrRef Path)
{
	const FilePath Relative = FilePath(std::u8string(Path.begin(), Path.end())).lexically_normal();

	// Make sure the relative path is actually relative. It can't try to escape the current folder or name a drive if it's relative.
	if(Relative.empty() || Relative == "." || Relative.has_root_name() || Relative.has_root_directory() || *Relative.begin() == FilePath(".."))
	{
		return {};
	}

	return Folder / Relative;
}

Str CharacterControlOBS::LogPath(FilePathRef Path)
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

std::optional<FilePath> CharacterControlOBS::CharacterFolder(FilePathRef Root, StrRef Name)
{
	return ResolveInsideFolder(CharactersFolder(Root), Name);
}

FilePath CharacterControlOBS::DefaultCharacterFolder(FilePathRef Root)
{
	return CharactersFolder(Root) / DefaultCharacterName;
}

FilePath CharacterControlOBS::CharacterFilePath(FilePathRef Folder)
{
	return Folder / "character.json";
}

bool CharacterControlOBS::HasCharacterFile(FilePathRef Folder)
{
	std::error_code Error;
	return is_regular_file(CharacterFilePath(Folder), Error);
}

std::optional<FilePath> CharacterControlOBS::FindRigPath(FilePathRef Root, StrRef CharacterName, StrRef RigName)
{
	const Str FileName = RigName + u8".Json";

	std::vector<FilePath> SearchFolders;
	if(const std::optional<FilePath> Character = CharacterFolder(Root, CharacterName))
	{
		SearchFolders.push_back(RigsFolder(*Character));
	}

	SearchFolders.push_back(RigsFolder(Root));

	for(FilePathRef Folder : SearchFolders)
	{
		const std::optional<FilePath> Path = ResolveInsideFolder(Folder, FileName);
		if(std::error_code Error; Path && is_regular_file(*Path, Error))
		{
			return Path->lexically_normal();
		}
	}

	return {};
}
