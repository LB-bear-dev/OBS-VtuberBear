#include "CharacterRig/CharacterFile.h"
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"

namespace
{
constexpr auto CHARACTER_FILE_NAME = "character.json";

// Reads a required field holding a path inside the character's folder
std::optional<FilePath> ReadPathField(const nlohmann::json& Json, const char* Field, FilePathRef Folder, const char* LogName)
{
	const auto Found = Json.find(Field);
	if(Found == Json.end())
	{
		ERROR_("%s: \"%s\" is missing.", LogName, Field);
		return std::nullopt;
	}

	if(!Found->is_string())
	{
		ERROR_("%s: \"%s\" must be a string.", LogName, Field);
		return std::nullopt;
	}

	const auto& Written = Found->get_ref<const Str&>();
	std::optional<FilePath> Resolved = CharacterControlOBS::ResolveInsideFolder(Folder, Written);
	if(!Resolved)
	{
		ERROR_("%s: \"%s\" must be a path inside the character's folder, not \"%s\".", LogName, Field, Written.c_str());
	}

	return Resolved;
}
}

std::optional<CharacterControlOBS::CharacterFile> CharacterControlOBS::ParseCharacterFile(const Str& Text, FilePathRef Folder)
{
	const Str LogName = LogPath(Folder / CHARACTER_FILE_NAME);

	const nlohmann::json Json = nlohmann::json::parse(Text, nullptr, false);
	if(Json.is_discarded())
	{
		ERROR_("%s: Json is malformed.", LogName.c_str());
		return std::nullopt;
	}

	if(!Json.is_object())
	{
		ERROR_("%s: It must be a JSON object.", LogName.c_str());
		return std::nullopt;
	}

	CharacterFile Character;

	// The name is only for display, so it falls back to the folder's name, which is usually the owner's Discord ID
	if(const auto Name = Json.find("name"); Name != Json.end())
	{
		if(!Name->is_string())
		{
			ERROR_("%s: \"name\" must be a string.", LogName.c_str());
			return std::nullopt;
		}

		Character.Name = Name->get<Str>();
	} else
	{
		const std::u8string FolderName = Folder.filename().u8string();
		Character.Name.assign(FolderName.begin(), FolderName.end());
	}

	std::optional<FilePath> Script = ReadPathField(Json, "script", Folder, LogName.c_str());
	if(!Script)
	{
		return std::nullopt;
	}

	std::optional<FilePath> BaseStyle = ReadPathField(Json, "base", Folder, LogName.c_str());
	if(!BaseStyle)
	{
		return std::nullopt;
	}

	Character.Script = std::move(*Script);
	Character.BaseStyle = std::move(*BaseStyle);
	return Character;
}

std::optional<CharacterControlOBS::CharacterFile> CharacterControlOBS::LoadCharacterFile(FilePathRef Folder)
{
	const FilePath Path = Folder / CHARACTER_FILE_NAME;
	std::ifstream File(Path, std::ios::binary);
	if(!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return std::nullopt;
	}

	const Str Text{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};
	return ParseCharacterFile(Text, Folder);
}
