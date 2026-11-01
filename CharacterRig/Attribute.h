#pragma once
#include <cstdint>
#include <variant>
#include "CharacterControlCore/CoreTypes.h"

namespace CharacterControlOBS
{
// Attributes are meant to be set from some sort of provider, usually a component. They control state or preferences that the rig
// can look at to decide its visual state.
class Attribute
{
public:
	// Holds some value that takes up 8 bits
	using Value = std::variant<bool, int64_t, double, Str>;

	explicit Attribute(Value Default);

	// Maps this attribute's type with its string name
	[[nodiscard]] const char* GetTypeName() const;

	// Gets the value, but does not resolve the type
	[[nodiscard]] const Value& Get() const;

	// Gets the value when you do know the type
	template<typename T>
	[[nodiscard]] const T* Get() const
	{
		return std::get_if<T>(&Current);
	}

	// Sets the value
	[[nodiscard]] bool Set(const Value& NewValue);

	// Maps the actual type with its string name
	[[nodiscard]] static const char* GetTypeName(const Value& Val);

private:
	Value Current;
};
}
