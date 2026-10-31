// Tests for PythonLocator, see PythonLocatorTests.cpp. With --probe this exe is the child process the scenarios run in.
#include <gtest/gtest.h>
#include "Probe.h"

int wmain(int Argc, wchar_t** Argv)
{
	if(Argc >= 2 && std::wstring(Argv[1]) == PythonLocatorTests::PROBE_FLAG)
	{
		return PythonLocatorTests::RunProbe(std::vector<std::wstring>(Argv + 2, Argv + Argc));
	}

	testing::InitGoogleTest(&Argc, Argv);
	return RUN_ALL_TESTS();
}
