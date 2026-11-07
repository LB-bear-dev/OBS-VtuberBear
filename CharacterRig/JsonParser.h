#pragma once
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
class JsonParser
{
public:
	explicit JsonParser(Str InLogName);

	[[nodiscard]] const Str& GetLogName() const;

	[[nodiscard]] std::optional<nlohmann::json> ParseObject(const Str& Text) const;

	[[nodiscard]] std::optional<StrId> ReadName(const nlohmann::json& Json, const char* Field) const;

	[[nodiscard]] std::optional<std::vector<StrId>> ReadNames(const nlohmann::json& Json, const char* Field) const;

private:
	Str LogName;
};
}
