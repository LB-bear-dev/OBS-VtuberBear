#include "CharacterRig/JsonParser.h"
#include <algorithm>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"

using namespace CharacterControlOBS;

JsonParser::JsonParser(Str InLogName) : LogName(std::move(InLogName)) {}

const Str& JsonParser::GetLogName() const
{
	return LogName;
}

std::optional<nlohmann::json> JsonParser::ParseObject(const Str& Text) const
{
	nlohmann::json Json = nlohmann::json::parse(Text, nullptr, false);
	if(Json.is_discarded())
	{
		ERROR_("%s: Json is malformed.", LogName.c_str());
		return std::nullopt;
	}

	if(!Json.is_object())
	{
		ERROR_("%s: must be a JSON object.", LogName.c_str());
		return std::nullopt;
	}

	return Json;
}

bool JsonParser::HasOnlyKeys(const nlohmann::json& Json, const std::initializer_list<const char*> Keys) const
{
	for(const auto& Item : Json.items())
	{
		const auto IsItem = [&Item](const char* Key)
		{
			return Item.key() == Key;
		};

		if(std::ranges::none_of(Keys, IsItem))
		{
			ERROR_("%s: unknown key \"%s\"", LogName.c_str(), Item.key().c_str());
			return false;
		}
	}

	return true;
}

std::optional<StrId> JsonParser::ReadName(const nlohmann::json& Json, const char* Field) const
{
	const auto Found = Json.find(Field);
	if(Found == Json.end())
	{
		ERROR_("%s needs a \"%s\".", LogName.c_str(), Field);
		return std::nullopt;
	}

	if(!Found->is_string() || Found->get_ref<const Str&>().empty())
	{
		ERROR_("%s: \"%s\" must be a non-empty string.", LogName.c_str(), Field);
		return std::nullopt;
	}

	return Found->get<StrId>();
}

std::optional<std::vector<StrId>> JsonParser::ReadNames(const nlohmann::json& Json, const char* Field) const
{
	const auto Found = Json.find(Field);
	if(Found == Json.end())
	{
		ERROR_("%s needs \"%s\".", LogName.c_str(), Field);
		return std::nullopt;
	}

	if(!Found->is_array())
	{
		ERROR_("%s: \"%s\" must be a list of names.", LogName.c_str(), Field);
		return std::nullopt;
	}

	std::vector<StrId> Names;
	for(const nlohmann::json& Entry : *Found)
	{
		if(!Entry.is_string() || Entry.get_ref<const Str&>().empty())
		{
			ERROR_("%s: \"%s\" must only hold non-empty strings.", LogName.c_str(), Field);
			return std::nullopt;
		}

		const auto& Name = Entry.get_ref<const Str&>();
		if(std::ranges::find(Names, Name) != Names.end())
		{
			ERROR_("%s: \"%s\" lists \"%s\" twice.", LogName.c_str(), Field, Name.c_str());
			return std::nullopt;
		}

		Names.push_back(Name);
	}

	return Names;
}
