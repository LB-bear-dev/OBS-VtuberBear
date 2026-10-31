#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace PythonLocatorTests
{
// The child process the tests run each scenario in. A process can only be bound to one Python, and the scenarios differ in
// which Pythons they can see, so the test exe runs itself with --probe and reads what it prints.
//
//   --probe [noreg] [prefer=<folder>]
//     noreg           hide the Pythons registered on this machine
//     prefer=<dir>    pass <dir> as a preferred folder, as the one from OBS's scripting settings is
//
// Prints "skipped: ..." per Python passed over, then either "RESULT: bound Python 3.<minor>", an import of a few standard
// modules and "PYTHON OK", or "RESULT: no usable Python". Exit code 0 when a Python ran, 2 when none was found.
constexpr const wchar_t* PROBE_FLAG = L"--probe";

int RunProbe(const std::vector<std::wstring>& Args);

// The N of the python3N.dll in Folder, or -1 if there is none
int InstalledMinor(const std::filesystem::path& Folder);
}
