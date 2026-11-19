#pragma once
#include <ostream>
enum class LogSeverity : char
{
	Verbose,
	Info,
	NonSevere,
	Severe,
	Critical
};
using LogFnDefinition = void(*)(LogSeverity Severity, const char* Msg);
