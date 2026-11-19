// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "CharacterRig/Attribute.h"

namespace CharacterControlOBS
{
struct Condition
{
	enum class Operator : uint8_t
	{
		All,
		Any,
		Not,
		Equal,
		NotEqual,
		Less,
		Greater,
		LessEqual,
		GreaterEqual,
		Contains
	};

	Operator Op = Operator::All;

	std::vector<Condition> ChildConditions;

	StrId Attribute;
	Attribute::Value Value;

	bool operator==(const Condition&) const = default;
};

constexpr int MAX_CONDITION_DEPTH = 32;

std::optional<Condition> ParseCondition(const nlohmann::json& Json);

std::optional<Attribute::Value> ReadAttributeValue(const nlohmann::json& Json);
}
