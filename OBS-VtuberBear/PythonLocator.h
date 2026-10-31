#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace CharacterControlScript
{
// Oldest Python 3 minor version we accept. The project builds against the stable ABI (Py_LIMITED_API in the vcxproj), and
// 3.8 is where every call we make is part of it. Lower this and Py_LIMITED_API together.
constexpr int MIN_PYTHON_MINOR = 8;

// Newest minor version we look for by name. Python names its DLL python3<minor>.dll, so each version has to be tried in turn.
constexpr int MAX_PYTHON_MINOR = 40;

struct PythonBinding
{
	// The python3<minor>.dll Python calls are bound to (an HMODULE)
	void* Module = nullptr;

	int Minor = 0;

	// OBS, or something else in the process, had already loaded this Python
	bool Shared = false;

	// Folder holding the standard library (Lib\ or python3<minor>.zip). Empty when there is none to point Python at.
	std::filesystem::path Home;

	// A line per Python that was found but passed over, and why
	std::vector<std::string> Skipped;
};

// Finds a Python 3 and binds the delay-loaded python3.dll imports to it, in this order:
//   1. a Python already loaded into this process, so we share the interpreter OBS's own scripting uses
//   2. the folders in PreferredFolders (for instance the Python folder OBS's scripting settings name)
//   3. Python installs registered in the registry and Pythons on PATH, newest first
// Returns false when none could be used; Out.Skipped says what was tried. Python calls must not be made unless this returned true.
bool BindPython(const std::vector<std::filesystem::path>& PreferredFolders, PythonBinding& Out);

// Looks up an export of the bound Python. Null if it doesn't have one.
void* FindPythonExport(const PythonBinding& Binding, const char* Name);

// True if Python started with this home can find its standard library. Starting one that can't kills the host process.
bool HasStandardLibrary(const std::filesystem::path& Home, int Minor);
}
