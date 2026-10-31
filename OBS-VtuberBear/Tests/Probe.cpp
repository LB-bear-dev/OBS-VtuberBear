#include "Probe.h"
#include <array>
#include <cstdio>
#include <memory>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// pyconfig.h switches to the debug Python ABI (python312_d.lib) whenever _DEBUG is set, and only the release Python is installed
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif
#include "../PythonLocator.h"

using namespace CharacterControlScript;

namespace
{
// Gives the process an empty HKCU and HKLM, so the Pythons registered on this machine are not seen
class EmptyRegistry
{
public:
	EmptyRegistry()
	{
		Name = L"Software\\BearApps\\PythonLocatorTests\\" + std::to_wstring(GetCurrentProcessId());
		for(int Index = 0; Index < 2; ++Index)
		{
			const std::wstring Path = Name + (Index == 0 ? L"\\User" : L"\\Machine");
			RegCreateKeyExW(HKEY_CURRENT_USER, Path.c_str(), 0, nullptr, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, &Keys[Index], nullptr);
		}
		RegOverridePredefKey(HKEY_CURRENT_USER, Keys[0]);
		RegOverridePredefKey(HKEY_LOCAL_MACHINE, Keys[1]);
	}

	~EmptyRegistry()
	{
		RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
		RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr);
		for(HKEY Key : Keys)
		{
			RegCloseKey(Key);
		}
		RegDeleteTreeW(HKEY_CURRENT_USER, Name.c_str());
		RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\BearApps\\PythonLocatorTests");
	}

	EmptyRegistry(const EmptyRegistry&) = delete;
	EmptyRegistry& operator=(const EmptyRegistry&) = delete;

private:
	std::wstring Name;
	std::array<HKEY, 2> Keys = {nullptr, nullptr};
};

// Start Python the way PythonInterface::Start does
void StartInterpreter(const PythonBinding& Binding)
{
	static std::wstring Home;
	auto* SetPythonHome = reinterpret_cast<void(__cdecl*)(const wchar_t*)>(FindPythonExport(Binding, "Py_SetPythonHome"));
	if(SetPythonHome != nullptr && !Binding.Home.empty())
	{
		Home = Binding.Home.wstring();
		SetPythonHome(Home.c_str());
	}
	Py_Initialize();
}
}

int PythonLocatorTests::InstalledMinor(const std::filesystem::path& Folder)
{
	for(int Minor = MAX_PYTHON_MINOR; Minor >= MIN_PYTHON_MINOR; --Minor)
	{
		std::error_code Error;
		if(std::filesystem::exists(Folder / (L"python3" + std::to_wstring(Minor) + L".dll"), Error))
		{
			return Minor;
		}
	}
	return -1;
}

int PythonLocatorTests::RunProbe(const std::vector<std::wstring>& Args)
{
	// A crash must not take the lines before it along
	setvbuf(stdout, nullptr, _IONBF, 0);

	std::unique_ptr<EmptyRegistry> Registry;
	std::vector<std::filesystem::path> Preferred;
	for(const std::wstring& Arg : Args)
	{
		if(Arg == L"noreg")
		{
			Registry = std::make_unique<EmptyRegistry>();
		} else if(Arg.starts_with(L"prefer="))
		{
			Preferred.emplace_back(Arg.substr(7));
		}
	}

	PythonBinding Binding;
	const bool Bound = BindPython(Preferred, Binding);
	for(const std::string& Reason : Binding.Skipped)
	{
		printf("skipped: %s\n", Reason.c_str());
	}
	if(!Bound)
	{
		printf("RESULT: no usable Python\n");
		return 2;
	}
	printf("RESULT: bound Python 3.%d\n", Binding.Minor);

	StartInterpreter(Binding);

	// Pure Python and extension modules. Not socket: Winsock keeps its configuration in HKLM, which noreg hides
	for(const char* Module : {"json", "ctypes", "decimal"})
	{
		PyObject* Imported = PyImport_ImportModule(Module);
		if(Imported == nullptr)
		{
			printf("import %s: FAILED\n", Module);
			PyErr_Print();
			return 3;
		}
		Py_DECREF(Imported);
	}
	printf("PYTHON OK\n");
	return 0;
}
