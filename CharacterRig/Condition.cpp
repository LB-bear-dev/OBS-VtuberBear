// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/Condition.h"
#include <algorithm>

using namespace CharacterControlOBS;

namespace
{
bool Compare(const bool Current, const bool Expected, const BoolOperator Op)
{
	switch(Op)
	{
		case BoolOperator::Equal :
			return Current == Expected;
		case BoolOperator::NotEqual :
			return Current != Expected;
	}

	return false;
}

bool Compare(const double Current, const double Expected, const NumberOperator Op)
{
	switch(Op)
	{
		case NumberOperator::Equal : // Allowing direct comparison to float here as we've only got 1 number type
			return Current == Expected; // NOLINT(clang-diagnostic-float-equal)
		case NumberOperator::NotEqual :
			return Current != Expected; // NOLINT(clang-diagnostic-float-equal)
		case NumberOperator::Less :
			return Current < Expected;
		case NumberOperator::Greater :
			return Current > Expected;
		case NumberOperator::LessEqual :
			return Current <= Expected;
		case NumberOperator::GreaterEqual :
			return Current >= Expected;
	}

	return false;
}

bool Compare(const Str& Current, const Str& Expected, const TextOperator Op)
{
	switch(Op)
	{
		case TextOperator::Equal :
			return Current == Expected;
		case TextOperator::NotEqual :
			return Current != Expected;
		case TextOperator::Contains :
			return Current.find(Expected) != Str::npos;
	}

	return false;
}

template<typename T>
bool AllTrue(const std::vector<T>& Children, const AttributeMap& Attributes)
{
	return std::ranges::all_of(Children,
		[&Attributes](const T& Child)
		{
			return Child.Evaluate(Attributes);
		});
}

template<typename T>
bool AnyTrue(const std::vector<T>& Children, const AttributeMap& Attributes)
{
	return std::ranges::any_of(Children,
		[&Attributes](const T& Child)
		{
			return Child.Evaluate(Attributes);
		});
}
}

template<typename T, typename OperatorType>
bool Comparison<T, OperatorType>::Evaluate(const AttributeMap& Attributes) const
{
	if(const auto Found = Attributes.find(Attribute); Found != Attributes.end() && Found->second != nullptr)
	{
		const T* Current = Found->second->Get<T>();
		return Current != nullptr && Compare(*Current, Value, Op);
	}

	return false;
}

template struct Comparison<bool, BoolOperator>;
template struct Comparison<double, NumberOperator>;
template struct Comparison<Str, TextOperator>;

bool Condition::Evaluate(const AttributeMap& Attributes) const
{
	switch(Op)
	{
		case Operator::All :
			return AllTrue(Bools, Attributes) && AllTrue(Numbers, Attributes) && AllTrue(Texts, Attributes) && AllTrue(Groups, Attributes);
		case Operator::Any :
			return AnyTrue(Bools, Attributes) || AnyTrue(Numbers, Attributes) || AnyTrue(Texts, Attributes) || AnyTrue(Groups, Attributes);
	}

	return false;
}
