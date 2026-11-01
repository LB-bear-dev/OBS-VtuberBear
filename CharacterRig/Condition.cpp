#include "CharacterRig/Condition.h"
#include <algorithm>
#include <array>
#include <limits>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterControlCore/Overloaded.h"

using namespace CharacterControlOBS;

namespace
{
constexpr std::array<const char*, 4> CONDITION_KEYS = {"all", "any", "not", "attr"};

struct OperatorKey
{
	const char* Key;
	Condition::Operator Op;
};

constexpr std::array<OperatorKey, 7> OPERATORS = {
	{
		{.Key = "eq", .Op = Condition::Operator::Equal},
		{.Key = "ne", .Op = Condition::Operator::NotEqual},
		{.Key = "lt", .Op = Condition::Operator::Less},
		{.Key = "gt", .Op = Condition::Operator::Greater},
		{.Key = "le", .Op = Condition::Operator::LessEqual},
		{.Key = "ge", .Op = Condition::Operator::GreaterEqual},
		{.Key = "contains", .Op = Condition::Operator::Contains},
	}
};

const OperatorKey* GetOperatorFromName(const Str& Key)
{
	const auto Found = std::ranges::find_if(OPERATORS,
		[&Key](const OperatorKey& Entry)
		{
			return Key == Entry.Key;
		});

	return Found != OPERATORS.end() ? &*Found : nullptr;
}

bool OperationIsAllowed(const Attribute::Value& Val, const Condition::Operator Op)
{
	const bool Equality = Op == Condition::Operator::Equal || Op == Condition::Operator::NotEqual;
	const bool Ordering = Op != Condition::Operator::Contains;
	const bool Text = Equality || Op == Condition::Operator::Contains;

	// @formatter:off
	return std::visit(Overloaded{
		[Equality](bool) { return Equality; },
		[Ordering](int64_t) { return Ordering; },
		[Ordering](double) { return Ordering; },
		[Text](const Str&) { return Text; },
		[]<typename T>(const T&) { static_assert(ALWAYS_FALSE<T>); },
	}, Val);
	// @formatter:on
}

std::optional<Condition> ParseGroup(Condition::Operator Op, const char* Key, const nlohmann::json& Json, int Depth);

std::optional<Condition> ParseCompare(const nlohmann::json& Json);

std::optional<Condition> Parse(const nlohmann::json& Json, const int Depth)
{
	if(Depth > MAX_CONDITION_DEPTH)
	{
		ERROR_("Conditions can nest at most %d deep.", MAX_CONDITION_DEPTH);
		return std::nullopt;
	}

	if(!Json.is_object())
	{
		ERROR_("A condition must be a JSON object.");
		return std::nullopt;
	}

	const char* Key = nullptr;
	for(const char* Candidate : CONDITION_KEYS)
	{
		if(Json.contains(Candidate))
		{
			if(Key != nullptr)
			{
				Key = nullptr;
				break;
			}

			Key = Candidate;
		}
	}

	if(Key == nullptr)
	{
		ERROR_("A condition needs exactly one of \"all\", \"any\", \"not\" or \"attr\".");
		return std::nullopt;
	}

	const Str KeyName = Key;
	if(KeyName == "attr")
	{
		return ParseCompare(Json);
	}

	for(const auto& Item : Json.items())
	{
		if(Item.key() != KeyName)
		{
			ERROR_("Unknown key \"%s\" in a \"%s\" condition.", Item.key().c_str(), Key);
			return std::nullopt;
		}
	}

	if(KeyName == "all")
	{
		return ParseGroup(Condition::Operator::All, Key, Json, Depth);
	}

	if(KeyName == "any")
	{
		return ParseGroup(Condition::Operator::Any, Key, Json, Depth);
	}

	std::optional<Condition> Child = Parse(Json.at(Key), Depth + 1);
	if(!Child)
	{
		return std::nullopt;
	}

	Condition Not;
	Not.Op = Condition::Operator::Not;
	Not.ChildConditions.push_back(std::move(*Child));
	return Not;
}

std::optional<Condition> ParseGroup(const Condition::Operator Op, const char* Key, const nlohmann::json& Json, const int Depth)
{
	const nlohmann::json& List = Json.at(Key);
	if(!List.is_array())
	{
		ERROR_("\"%s\" must be a list of conditions.", Key);
		return std::nullopt;
	}

	Condition Group;
	Group.Op = Op;
	for(const nlohmann::json& ChildJson : List)
	{
		std::optional<Condition> Child = Parse(ChildJson, Depth + 1);
		if(!Child)
		{
			return std::nullopt;
		}

		Group.ChildConditions.push_back(std::move(*Child));
	}

	return Group;
}

std::optional<Condition> ParseCompare(const nlohmann::json& Json)
{
	const nlohmann::json& AttributeJson = Json.at("attr");
	if(!AttributeJson.is_string())
	{
		ERROR_("\"attr\" must be a string.");
		return std::nullopt;
	}

	const auto& Name = AttributeJson.get_ref<const Str&>();
	if(Json.size() < 2)
	{
		ERROR_("The condition on \"%s\" has no operator.", Name.c_str());
		return std::nullopt;
	}

	if(Json.size() > 2)
	{
		ERROR_("The condition on \"%s\" has more than one operator.", Name.c_str());
		return std::nullopt;
	}

	auto Entry = Json.begin();
	if(Entry.key() == "attr")
	{
		++Entry;
	}

	const Str& OperatorName = Entry.key();
	const OperatorKey* Op = GetOperatorFromName(OperatorName);
	if(Op == nullptr)
	{
		ERROR_("Attribute \"%s\" uses unknown operator %s.", Name.c_str(), OperatorName.c_str());
		return std::nullopt;
	}

	const nlohmann::json& ValueJson = Entry.value();
	const std::optional<Attribute::Value> Value = ReadAttributeValue(ValueJson);
	if(!Value)
	{
		ERROR_("\"%s\" uses invalid operator %s.", Name.c_str(), ValueJson.dump().c_str());
		return std::nullopt;
	}

	if(!OperationIsAllowed(*Value, Op->Op))
	{
		ERROR_("The condition on \"%s\" can't use \"%s\" with a value of type %s.", Name.c_str(), Op->Key, Attribute::GetTypeName(*Value));
		return std::nullopt;
	}

	return Condition{.Op = Op->Op, .ChildConditions = {}, .Attribute = Name, .Value = *Value};
}
}

std::optional<Condition> CharacterControlOBS::ParseCondition(const nlohmann::json& Json)
{
	return Parse(Json, 1);
}

std::optional<Attribute::Value> CharacterControlOBS::ReadAttributeValue(const nlohmann::json& Json)
{
	if(Json.is_boolean())
	{
		return Json.get<bool>();
	}

	if(Json.is_number_float())
	{
		return Json.get<double>();
	}

	if(Json.is_number_unsigned())
	{
		const auto Unsigned = Json.get<uint64_t>();
		if(Unsigned > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
		{
			return std::nullopt;
		}

		return static_cast<int64_t>(Unsigned);
	}

	if(Json.is_number_integer())
	{
		return Json.get<int64_t>();
	}

	if(Json.is_string())
	{
		return Json.get<Str>();
	}

	return std::nullopt;
}
