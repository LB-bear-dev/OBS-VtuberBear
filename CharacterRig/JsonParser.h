// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
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

		const Str& GetLogName() const;

		std::optional<nlohmann::json> ParseObject(const Str& Text) const;

		std::optional<StrId> ReadName(const nlohmann::json& Json, const char* Field) const;

		std::optional<std::vector<StrId>> ReadNames(const nlohmann::json& Json, const char* Field) const;

	private:
		Str LogName;
	};
}