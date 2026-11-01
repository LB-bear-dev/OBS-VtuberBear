#pragma once
#include <limits>

namespace CharacterControlOBS
{
template<typename T>
class Attribute
{
public:
	Attribute(T DefaultVal, T MinVal, T MaxVal) : CurrentValue(DefaultVal), MaxValue(MaxVal), MinValue(MinVal) {}

	explicit Attribute(T DefaultVal) : CurrentValue(DefaultVal), MaxValue(std::numeric_limits<T>::max()), MinValue(std::numeric_limits<T>::lowest()) {}

	void SetValue(T Value)
	{
		CurrentValue = std::max(std::min(Value, MaxValue), MinValue);
	}

	[[nodiscard]] T GetValue() const
	{
		return CurrentValue;
	}

private:
	T CurrentValue;
	T MaxValue;
	T MinValue;
};
}
