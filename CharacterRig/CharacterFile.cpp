// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/CharacterFile.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <glaze/glaze.hpp>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

namespace
{
std::shared_ptr<const Rig> GetRig(FilePathRef Path)
{
	static std::map<FilePath, std::shared_ptr<const Rig>> LoadedRigs;
	auto [Found, Added] = LoadedRigs.try_emplace(Path.lexically_normal());
	if(Added)
	{
		Found->second = std::make_shared<const Rig>(Rig::Load(Path));
	}

	return Found->second;
}

std::map<StrId, std::shared_ptr<const CharacterFile>> LoadedCharacters;
}

CharacterFile CharacterFile::Load(FilePathRef Root, StrIdRef Name)
{
	CharacterFile Loaded;

	const std::optional<FilePath> Folder = CharacterFolder(Root, Name);
	if(!Folder)
	{
		ERROR_("\"%s\" is not a valid character name.", Name.c_str());
		return Loaded;
	}

	const FilePath Path = CharacterFilePath(*Folder);

	std::ifstream File(Path, std::ios::binary);
	if(!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return Loaded;
	}

	const Utf8Str FileContents{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};
	if(const auto Error = glz::read<glz::opts{.error_on_unknown_keys = false}>(Loaded, FileContents))
	{
		ERROR_("%s has errors, character may be incomplete.\n%s", LogPath(Path).c_str(), format_error(Error, FileContents).c_str());
	}

	INFO_("Character file %s loaded", LogPath(Path).c_str());

	for(auto& [RigName, Loaded] : Loaded.Rigs)
	{
		if(const std::optional<FilePath> RigPath = FindRig(Root, Name, RigName))
		{
			Loaded = GetRig(*RigPath);
		}

		WARN_("%s: rig \"%s\" was not found, skipping.", Name.c_str(), RigName.c_str());
	}

	return Loaded;
}

void CharacterFile::LoadAll(FilePathRef Root)
{
	LoadedCharacters.clear();

	std::error_code Error;
	for(const auto& Entry : std::filesystem::directory_iterator(CharactersFolder(Root), Error))
	{
		const FilePath& Folder = Entry.path();
		if(HasCharacterFile(Folder))
		{
			const StrId Name = LogPath(Folder.filename());
			LoadedCharacters[Name] = std::make_shared<const CharacterFile>(Load(Root, Name));
		}
	}
}

bool CharacterFile::Save(FilePathRef Folder) const
{
	const FilePath Path = CharacterFilePath(Folder);

	Utf8Str FileContents;
	if(const auto Error = glz::write<glz::opts{.minified = true}>(*this, FileContents))
	{
		ERROR_("Cannot write character %s.\n%s", LogPath(Path).c_str(), format_error(Error, FileContents).c_str());
		return false;
	}

	std::ofstream File(Path, std::ios::binary);
	if(!(File << FileContents))
	{
		ERROR_("Cannot save to disk %s.", LogPath(Path).c_str());
		return false;
	}

	INFO_("Character file %s written successfully", LogPath(Path).c_str());

	return true;
}

std::shared_ptr<const CharacterFile> CharacterControlOBS::FindCharacterByName(StrIdRef Name)
{
	const auto Found = LoadedCharacters.find(Name);
	return Found != LoadedCharacters.end() ? Found->second : nullptr;
}

std::shared_ptr<const CharacterFile> CharacterControlOBS::FindCharacterByAlias(StrIdRef Service, StrIdRef Id)
{
	if(!Id.empty())
	{
		for(const std::shared_ptr<const CharacterFile>& Character : LoadedCharacters | std::views::values)
		{
			const auto Ids = Character->Aliases.find(Service);
			if(Ids != Character->Aliases.end() && std::ranges::find(Ids->second, Id) != Ids->second.end())
			{
				return Character;
			}
		}
	}

	ERROR_("Character %s was not found, trying default", Id.c_str());
	if(std::shared_ptr<const CharacterFile> Default = FindCharacterByName(DefaultCharacterName))
	{
		return Default;
	}

	ERROR_("Default character also not found");
	return {};
}
