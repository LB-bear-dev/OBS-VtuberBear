// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
#include <glaze/forward.hpp>

#include "CharacterRig/Attribute.h"

namespace CharacterControlOBS
{
enum class BoolOperator : uint8_t
{
	Equal,
	NotEqual
};

enum class NumberOperator : uint8_t
{
	Equal,
	NotEqual,
	Less,
	Greater,
	LessEqual,
	GreaterEqual
};

enum class TextOperator : uint8_t
{
	Equal,
	NotEqual,
	Contains
};

// One simple condition based on a single attribute. X == Y, Name contains "airplane"
template <typename T, typename OperatorType>
struct Comparison
{
	// Which kind of basic logical operator is it?
	OperatorType Op {};

	// What attribute are we comparing
	StrId Attribute;

	// What are we comparing the incoming attribute to?
	T Value {};

	bool Evaluate(const AttributeMap& Attributes) const;

	bool operator==(const Comparison&) const = default;
};

using BoolComparison = Comparison<bool, BoolOperator>;
using NumberComparison = Comparison<double, NumberOperator>;
using TextComparison = Comparison<Str, TextOperator>;

// Holds meta conditions, which are aggregations of other conditons
// to produce a final result.
struct Condition
{
	enum class Operator : uint8_t
	{
		All,
		Any
	} Op = Operator::All;

	// Collections of aggregated results from individual comparisons
	std::vector<BoolComparison> Bools;
	std::vector<NumberComparison> Numbers;
	std::vector<TextComparison> Texts;
	std::vector<Condition> Groups;

	bool Evaluate(const AttributeMap& Attributes) const;

	bool operator==(const Condition&) const = default;
};
}


// Custom serialization for enum values
template <>
struct glz::meta<CharacterControlOBS::BoolOperator>
{
	using enum CharacterControlOBS::BoolOperator;
	static constexpr std::array<std::string_view, 2> keys {"eq", "ne"};
	static constexpr std::array value {Equal, NotEqual};
};

template <>
struct glz::meta<CharacterControlOBS::NumberOperator>
{
	using enum CharacterControlOBS::NumberOperator;
	static constexpr std::array<std::string_view, 6> keys {"eq", "ne", "lt", "gt", "le", "ge"};
	static constexpr std::array value {Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual};
};

template <>
struct glz::meta<CharacterControlOBS::TextOperator>
{
	using enum CharacterControlOBS::TextOperator;
	static constexpr std::array<std::string_view, 3> keys {"eq", "ne", "contains"};
	static constexpr std::array value {Equal, NotEqual, Contains};
};

template <>
struct glz::meta<CharacterControlOBS::Condition::Operator>
{
	using enum CharacterControlOBS::Condition::Operator;
	static constexpr std::array<std::string_view, 2> keys {"all", "any"};
	static constexpr std::array value {All, Any};
};
