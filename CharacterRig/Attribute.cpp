#include "CharacterRig/Attribute.h"
#include "CharacterControlCore/Overloaded.h"

using namespace CharacterControlOBS;

Attribute::Attribute(Value Default) : Current(std::move(Default)) {}

const char* Attribute::GetTypeName() const
{
	return GetTypeName(Current);
}

const Attribute::Value& Attribute::Get() const
{
	return Current;
}

bool Attribute::Set(const Value& NewValue)
{
	if(NewValue.index() != Current.index())
	{
		return false;
	}

	Current = NewValue;
	return true;
}

const char* Attribute::GetTypeName(const Value& Val)
{
	// @formatter:off
	return std::visit(Overloaded{
		[](bool) { return "bool"; },
		[](int64_t) { return "int"; },
		[](double) { return "float"; },
		[](const Str&) { return "string"; },
		[]<typename T>(const T&) { static_assert(ALWAYS_FALSE<T>); },
	}, Val);
	// @formatter:on
}
