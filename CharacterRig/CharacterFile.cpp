// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/CharacterFile.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/JsonParser.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

namespace
{
constexpr auto CHARACTER_FILE_NAME = "character.json";

constexpr auto DEFAULT_FOLDER_NAME = "default";

bool HasName(const std::vector<StrId>& Names, StrIdRef Name)
{
	return std::ranges::find(Names, Name) != Names.end();
}

class CharacterParser : JsonParser
{
public:
	CharacterParser(Str InLogName, FilePath InFolder) : JsonParser(std::move(InLogName)), Folder(std::move(InFolder)) {}

	using JsonParser::ParseObject;

	std::optional<CharacterFile> Parse(const nlohmann::json& Json) const;

private:
	std::optional<Str> ReadDisplayName(const nlohmann::json& Json) const;

	std::optional<FilePath> ReadPath(const nlohmann::json& Json, const Str& Where) const;

	std::optional<std::vector<ComponentEntry>> ReadComponents(const nlohmann::json& Json) const;

	std::optional<std::map<StrId, Attribute::Value>> ReadDefaults(const nlohmann::json& Json) const;

	std::optional<std::map<StrId, FilePath>> ReadStyles(const nlohmann::json& Json) const;

	std::optional<std::vector<Look>> ReadLooks(const nlohmann::json& Json, const std::map<StrId, FilePath>& Styles) const;

	std::optional<std::vector<StrId>> ReadChannels(const nlohmann::json& Json) const;

	std::optional<std::vector<Hotkey>> ReadHotkeys(const nlohmann::json& Json, const std::vector<StrId>& Channels) const;

	std::optional<Hotkey> ReadHotkey(const nlohmann::json& Json, int Number, const std::vector<StrId>& Channels) const;

	FilePath Folder;
};

std::optional<CharacterFile> CharacterParser::Parse(const nlohmann::json& Json) const
{
	CharacterFile Character;

	std::optional<Str> Name = ReadDisplayName(Json);
	std::optional<FilePath> RigPath;
	if(Json.contains("rig"))
	{
		RigPath = ReadPath(Json["rig"], "\"rig\"");
	} else
	{
		ERROR_("%s: \"rig\" is missing.", GetLogName().c_str());
	}

	if(!Name || !RigPath)
	{
		return std::nullopt;
	}

	std::optional<std::vector<ComponentEntry>> Components = ReadComponents(Json);
	std::optional<std::map<StrId, Attribute::Value>> Defaults = Components ? ReadDefaults(Json) : std::nullopt;
	std::optional<std::map<StrId, FilePath>> Styles = Defaults ? ReadStyles(Json) : std::nullopt;
	std::optional<std::vector<Look>> Looks = Styles ? ReadLooks(Json, *Styles) : std::nullopt;
	std::optional<std::vector<StrId>> Channels = Looks ? ReadChannels(Json) : std::nullopt;
	std::optional<std::vector<Hotkey>> Hotkeys = Channels ? ReadHotkeys(Json, *Channels) : std::nullopt;
	if(!Hotkeys)
	{
		return std::nullopt;
	}

	Character.Name = std::move(*Name);
	Character.RigPath = std::move(*RigPath);
	Character.Components = std::move(*Components);
	Character.Defaults = std::move(*Defaults);
	Character.Styles = std::move(*Styles);
	Character.Looks = std::move(*Looks);
	Character.Channels = std::move(*Channels);
	Character.Hotkeys = std::move(*Hotkeys);
	return Character;
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

std::optional<FilePath> CharacterParser::ReadPath(const nlohmann::json& Json, const Str& Where) const
{
	if(!Json.is_string())
	{
		ERROR_("%s: %s must be a string.", GetLogName().c_str(), Where.c_str());
		return std::nullopt;
	}

	const auto& Written = Json.get_ref<const Str&>();
	std::optional<FilePath> Resolved = ResolveInsideFolder(Folder, Written);
	if(!Resolved)
	{
		ERROR_("%s: %s is outside the character folder: \"%s\".", GetLogName().c_str(), Where.c_str(), Written.c_str());
	}

	return Resolved;
}

std::optional<std::vector<ComponentEntry>> CharacterParser::ReadComponents(const nlohmann::json& Json) const
{
	std::vector<ComponentEntry> Components;
	const auto Found = Json.find("components");
	if(Found == Json.end())
	{
		return Components;
	}

	if(!Found->is_array())
	{
		ERROR_("%s: \"components\" must be a list.", GetLogName().c_str());
		return std::nullopt;
	}

	int Number = 0;
	for(const nlohmann::json& Entry : *Found)
	{
		++Number;
		if(!Entry.is_object())
		{
			ERROR_("%s: component %d must be a JSON object.", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		std::optional<StrId> Type = ReadName(Entry, "type");
		if(!Type)
		{
			return std::nullopt;
		}

		Components.push_back(ComponentEntry{.Type = std::move(*Type), .Settings = Entry});
	}

	return Components;
}

std::optional<std::map<StrId, Attribute::Value>> CharacterParser::ReadDefaults(const nlohmann::json& Json) const
{
	std::map<StrId, Attribute::Value> Defaults;
	const auto Found = Json.find("defaults");
	if(Found == Json.end())
	{
		return Defaults;
	}

	if(!Found->is_object())
	{
		ERROR_("%s: \"defaults\" must be a JSON object.", GetLogName().c_str());
		return std::nullopt;
	}

	for(const auto& Item : Found->items())
	{
		if(Item.key().empty())
		{
			ERROR_("%s: \"defaults\" has an empty name.", GetLogName().c_str());
			return std::nullopt;
		}

		std::optional<Attribute::Value> Value = ReadAttributeValue(Item.value());
		if(!Value)
		{
			ERROR_("%s: default \"%s\" must be a bool, number or string.", GetLogName().c_str(), Item.key().c_str());
			return std::nullopt;
		}

		Defaults.emplace(Item.key(), std::move(*Value));
	}

	return Defaults;
}

std::optional<std::map<StrId, FilePath>> CharacterParser::ReadStyles(const nlohmann::json& Json) const
{
	std::map<StrId, FilePath> Styles;
	const auto Found = Json.find("styles");
	if(Found == Json.end())
	{
		return Styles;
	}

	if(!Found->is_object())
	{
		ERROR_("%s: \"styles\" must be a JSON object.", GetLogName().c_str());
		return std::nullopt;
	}

	for(const auto& Item : Found->items())
	{
		if(Item.key().empty())
		{
			ERROR_("%s: \"styles\" has an empty name.", GetLogName().c_str());
			return std::nullopt;
		}

		std::optional<FilePath> Path = ReadPath(Item.value(), "style \"" + Item.key() + "\"");
		if(!Path)
		{
			return std::nullopt;
		}

		Styles.emplace(Item.key(), std::move(*Path));
	}

	return Styles;
}

std::optional<std::vector<Look>> CharacterParser::ReadLooks(const nlohmann::json& Json, const std::map<StrId, FilePath>& Styles) const
{
	std::vector<Look> Looks;
	const auto Found = Json.find("looks");
	if(Found == Json.end())
	{
		return Looks;
	}

	if(!Found->is_array())
	{
		ERROR_("%s: \"looks\" must be a list.", GetLogName().c_str());
		return std::nullopt;
	}

	int Number = 0;
	for(const nlohmann::json& Entry : *Found)
	{
		++Number;
		if(!Entry.is_object())
		{
			ERROR_("%s: look %d must be a JSON object.", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		const bool HasHeading = Entry.contains("heading");
		const bool HasStyle = Entry.contains("style");
		if(HasHeading == HasStyle)
		{
			ERROR_("%s: look %d needs exactly one of \"heading\" or \"style\".", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		Look Result;
		if(HasHeading)
		{
			std::optional<StrId> Heading = ReadName(Entry, "heading");
			if(!Heading)
			{
				return std::nullopt;
			}

			Result.Heading = std::move(*Heading);
			Looks.push_back(std::move(Result));
			continue;
		}

		std::optional<StrId> Style = ReadName(Entry, "style");
		if(!Style)
		{
			return std::nullopt;
		}

		if(!Styles.contains(*Style))
		{
			ERROR_("%s: look %d: no style \"%s\".", GetLogName().c_str(), Number, Style->c_str());
			return std::nullopt;
		}

		Result.Style = std::move(*Style);
		if(const auto When = Entry.find("when"); When != Entry.end())
		{
			std::optional<Condition> Parsed = ParseCondition(*When);
			if(!Parsed)
			{
				ERROR_("%s: look %d has an invalid condition.", GetLogName().c_str(), Number);
				return std::nullopt;
			}

			Result.When = std::move(*Parsed);
		}

		Looks.push_back(std::move(Result));
	}

	return Looks;
}

std::optional<std::vector<StrId>> CharacterParser::ReadChannels(const nlohmann::json& Json) const
{
	if(!Json.contains("channels"))
	{
		return DefaultChannels();
	}

	std::optional<std::vector<StrId>> Channels = ReadNames(Json, "channels");
	if(Channels && Channels->empty())
	{
		ERROR_("%s: \"channels\" is empty.", GetLogName().c_str());
		return std::nullopt;
	}

	return Channels;
}

std::optional<std::vector<Hotkey>> CharacterParser::ReadHotkeys(const nlohmann::json& Json, const std::vector<StrId>& Channels) const
{
	std::vector<Hotkey> Hotkeys;
	const auto Found = Json.find("hotkeys");
	if(Found == Json.end())
	{
		return Hotkeys;
	}

	if(!Found->is_array())
	{
		ERROR_("%s: \"hotkeys\" must be a list.", GetLogName().c_str());
		return std::nullopt;
	}

	int Number = 0;
	for(const nlohmann::json& Entry : *Found)
	{
		std::optional<Hotkey> Result = ReadHotkey(Entry, ++Number, Channels);
		if(!Result)
		{
			return std::nullopt;
		}

		Hotkeys.push_back(std::move(*Result));
	}

	return Hotkeys;
}

std::optional<Hotkey> CharacterParser::ReadHotkey(const nlohmann::json& Json, int Number, const std::vector<StrId>& Channels) const
{
	if(!Json.is_object())
	{
		ERROR_("%s: hotkey %d must be a JSON object.", GetLogName().c_str(), Number);
		return std::nullopt;
	}

	std::optional<StrId> Name = ReadName(Json, "name");
	std::optional<StrId> Attr = Name ? ReadName(Json, "attr") : std::nullopt;
	std::optional<StrId> Channel = Attr ? ReadName(Json, "channel") : std::nullopt;
	if(!Channel)
	{
		return std::nullopt;
	}

	if(!HasName(Channels, *Channel))
	{
		ERROR_("%s: hotkey %d: no channel \"%s\".", GetLogName().c_str(), Number, Channel->c_str());
		return std::nullopt;
	}

	Hotkey Result{.Name = std::move(*Name), .Attribute = std::move(*Attr), .Channel = std::move(*Channel)};

	const auto Mode = Json.find("mode");
	if(Mode == Json.end() || !Mode->is_string())
	{
		ERROR_("%s: hotkey %d: \"mode\" must be set, toggle or clear.", GetLogName().c_str(), Number);
		return std::nullopt;
	}

	const Str& ModeName = Mode->get_ref<const Str&>();
	if(ModeName == "set")
	{
		Result.Mode = HotkeyMode::Set;
	} else if(ModeName == "toggle")
	{
		Result.Mode = HotkeyMode::Toggle;
	} else if(ModeName == "clear")
	{
		Result.Mode = HotkeyMode::Clear;
	} else
	{
		ERROR_("%s: hotkey %d: unknown mode \"%s\".", GetLogName().c_str(), Number, ModeName.c_str());
		return std::nullopt;
	}

	if(Result.Mode != HotkeyMode::Clear)
	{
		const auto Value = Json.find("value");
		if(Value == Json.end())
		{
			ERROR_("%s: hotkey %d: \"value\" is missing.", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		Result.Value = ReadAttributeValue(*Value);
		if(!Result.Value)
		{
			ERROR_("%s: hotkey %d: \"value\" must be a bool, number or string.", GetLogName().c_str(), Number);
			return std::nullopt;
		}
	}

	if(const auto Seconds = Json.find("for"); Seconds != Json.end())
	{
		if(!Seconds->is_number() || Seconds->get<double>() <= 0)
		{
			ERROR_("%s: hotkey %d: \"for\" must be above zero.", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		Result.Seconds = Seconds->get<double>();
	}

	return Result;
}
}

std::vector<StrId> CharacterControlOBS::DefaultChannels()
{
	return {"Integrations", "Hotkeys", "Manual"};
}

std::optional<CharacterFile> CharacterControlOBS::ParseCharacterFile(const Str& Text, FilePathRef Folder)
{
	const CharacterParser Parser(LogPath(Folder / CHARACTER_FILE_NAME), Folder);
	const std::optional<nlohmann::json> Json = Parser.ParseObject(Text);
	if(!Json)
	{
		return std::nullopt;
	}

	return Parser.Parse(*Json);
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
	std::optional<CharacterFile> Character = ParseCharacterFile(Text, Folder);
	if(!Character)
	{
		return std::nullopt;
	}

	std::optional<Rig> RigDefinition = LoadRig(Character->RigPath);
	if(!RigDefinition)
	{
		return std::nullopt;
	}

	return LoadedCharacter{.File = std::move(*Character), .RigDefinition = std::move(*RigDefinition)};
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
