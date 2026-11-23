// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/CharacterFile.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/JsonParser.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

namespace
{
constexpr auto CHARACTER_FILE_NAME = "character.json";

constexpr auto DEFAULT_FOLDER_NAME = "default";

class CharacterParser : JsonParser
{
public:
	CharacterParser(Str InLogName, FilePath InFolder) : JsonParser(std::move(InLogName)), Folder(std::move(InFolder)) {}

	using JsonParser::ParseObject;

	std::optional<LoadedCharacter> Parse(const nlohmann::json& Json) const;

private:
	std::optional<Str> ReadDisplayName(const nlohmann::json& Json) const;

	std::optional<Rig> ReadRig(const nlohmann::json& Json) const;

	std::optional<std::vector<Style>> ReadStyles(const nlohmann::json& Json) const;

	FilePath Folder;
};

std::optional<LoadedCharacter> CharacterParser::Parse(const nlohmann::json& Json) const
{
	std::optional<Str> Name = ReadDisplayName(Json);
	std::optional<std::vector<Style>> Styles = Name ? ReadStyles(Json) : std::nullopt;
	std::optional<Rig> RigDefinition = Styles ? ReadRig(Json) : std::nullopt;
	if(!RigDefinition)
	{
		return std::nullopt;
	}

	return LoadedCharacter{.Name = std::move(*Name), .RigDefinition = std::move(*RigDefinition), .StyleDefinitions = std::move(*Styles)};
}

std::optional<Str> CharacterParser::ReadDisplayName(const nlohmann::json& Json) const
{
	const auto Name = Json.find("name");
	if(Name == Json.end())
	{
		const std::u8string FolderName = Folder.filename().u8string();
		return Str(FolderName.begin(), FolderName.end());
	}

	if(!Name->is_string())
	{
		ERROR_("%s: \"name\" must be a string.", GetLogName().c_str());
		return std::nullopt;
	}

	return Name->get<Str>();
}

std::optional<Rig> CharacterParser::ReadRig(const nlohmann::json& Json) const
{
	const auto Found = Json.find("rig");
	if(Found == Json.end())
	{
		ERROR_("%s: \"rig\" is missing.", GetLogName().c_str());
		return std::nullopt;
	}

	if(!Found->is_string())
	{
		ERROR_("%s: \"rig\" must be a string.", GetLogName().c_str());
		return std::nullopt;
	}

	const auto& Written = Found->get_ref<const Str&>();
	const std::optional<FilePath> RigPath = ResolveInsideFolder(Folder, Written);
	if(!RigPath)
	{
		ERROR_("%s: \"rig\" is outside the character folder: \"%s\".", GetLogName().c_str(), Written.c_str());
		return std::nullopt;
	}

	// The path is only needed to find the rig, so it is loaded right away rather than stored.
	return LoadRig(*RigPath);
}

std::optional<std::vector<Style>> CharacterParser::ReadStyles(const nlohmann::json& Json) const
{
	std::vector<Style> Styles;
	const auto Found = Json.find("styles");
	if(Found == Json.end())
	{
		return Styles;
	}

	// A list rather than an object so the order styles are layered in is the order they are written in.
	if(!Found->is_array())
	{
		ERROR_("%s: \"styles\" must be a list.", GetLogName().c_str());
		return std::nullopt;
	}

	int Number = 0;
	for(const nlohmann::json& Entry : *Found)
	{
		++Number;
		std::optional<Style> Parsed = ParseStyle(Entry, GetLogName() + ": style " + std::to_string(Number), Folder);
		if(!Parsed)
		{
			return std::nullopt;
		}

		const auto SameName = [&Parsed](const Style& Other)
		{
			return Other.Name == Parsed->Name;
		};

		if(std::ranges::any_of(Styles, SameName))
		{
			ERROR_("%s: style %d: \"%s\" is already used by another style.", GetLogName().c_str(), Number, Parsed->Name.c_str());
			return std::nullopt;
		}

		Styles.push_back(std::move(*Parsed));
	}

	return Styles;
}
}

std::optional<LoadedCharacter> CharacterControlOBS::LoadCharacterFile(FilePathRef Folder)
{
	const FilePath Path = Folder / CHARACTER_FILE_NAME;
	std::ifstream File(Path, std::ios::binary);
	if(!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return std::nullopt;
	}

	const Str Text{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};
	const CharacterParser Parser(LogPath(Path), Folder);
	const std::optional<nlohmann::json> Json = Parser.ParseObject(Text);
	if(!Json)
	{
		return std::nullopt;
	}

	return Parser.Parse(*Json);
}

std::optional<FilePath> CharacterControlOBS::FindCharacterFolder(FilePathRef CharactersFolder, StrIdRef DiscordId)
{
	const auto HasCharacterFile = [](FilePathRef Folder)
	{
		std::error_code Error;
		return std::filesystem::is_regular_file(Folder / CHARACTER_FILE_NAME, Error);
	};

	const auto IsDigit = [](char Letter)
	{
		return Letter >= '0' && Letter <= '9';
	};

	if(!DiscordId.empty() && std::ranges::all_of(DiscordId, IsDigit))
	{
		FilePath Own = CharactersFolder / DiscordId;
		if(HasCharacterFile(Own))
		{
			return Own;
		}
	}

	FilePath Default = CharactersFolder / DEFAULT_FOLDER_NAME;
	if(HasCharacterFile(Default))
	{
		return Default;
	}

	WARN_("No character folder for \"%s\" and no default in %s.", DiscordId.c_str(), LogPath(CharactersFolder).c_str());
	return std::nullopt;
}
