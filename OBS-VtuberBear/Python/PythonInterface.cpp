#include "PCH.h"
#include "Python/PythonInterface.h"
#include <mutex>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>
#include <util/platform.h>
#include "CharacterControlCore/Messaging.h"
#include "Python/PythonLocator.h"

using namespace CharacterControlScript;

std::unique_ptr<PythonInterface> PythonInterface::Singleton = nullptr;
Str PythonInterface::LastError;

namespace
{
// Python holds on to the pointer it is given as its home, so it has to outlive the interpreter
std::wstring PythonHomeStorage;

// The Python folder OBS's scripting settings name, if one was set. OBS keeps it in its user or global config depending on version.
std::vector<FilePath> ObsPythonFolders()
{
	std::vector<FilePath> Folders;
	for(const char* ConfigName : {"obs-studio/user.ini", "obs-studio/global.ini"})
	{
		char* ConfigPath = os_get_config_path_ptr(ConfigName);
		if(ConfigPath == nullptr)
		{
			continue;
		}

		config_t* Config = nullptr;
		if(config_open(&Config, ConfigPath, CONFIG_OPEN_EXISTING) == CONFIG_SUCCESS)
		{
			if(const char* Folder = config_get_string(Config, "Python", "Path64bit"); Folder != nullptr && *Folder != '\0')
			{
				Folders.emplace_back(std::u8string(reinterpret_cast<const char8_t*>(Folder)));
			}
			config_close(Config);
		}
		bfree(ConfigPath);
	}
	return Folders;
}

Str MissingPythonMessage()
{
	return "Scripted characters need Python 3." + std::to_string(MIN_PYTHON_MINOR) + " or newer (64-bit), and none was found. Install it from https://www.python.org/downloads/ and restart OBS.";
}
}

PythonInterface* PythonInterface::TryGet()
{
	static std::once_flag Started;
	std::call_once(Started,
		[]
		{
			if(std::unique_ptr<PythonInterface> Interface(new PythonInterface()); Interface->Start())
			{
				Singleton = std::move(Interface);
			}
		});

	return Singleton.get();
}

void PythonInterface::AddToSysPath(const std::filesystem::path& Folder, const bool Front)
{
	PyObject* SysPath = PySys_GetObject("path");
	PyObject* Entry = PyUnicode_FromWideChar(Folder.c_str(), -1);
	if(SysPath != nullptr && Entry != nullptr && PySequence_Contains(SysPath, Entry) == 0)
	{
		if(Front)
		{
			PyList_Insert(SysPath, 0, Entry);
		} else
		{
			PyList_Append(SysPath, Entry);
		}
	}
	Py_XDECREF(Entry);
}

bool PythonInterface::Start()
{
	PythonBinding Binding;
	if(!BindPython(ObsPythonFolders(), Binding))
	{
		LastError = MissingPythonMessage();
		ERROR_("%s", LastError.c_str());
		for(const Str& Reason : Binding.Skipped)
		{
			WARN_("Python skipped: %s", Reason.c_str());
		}
		return false;
	}

	const std::u8string HomeText = Binding.Home.u8string();
	const Str From = Binding.Home.empty() ? Str() : " from " + Str(HomeText.begin(), HomeText.end());
	INFO_("Using Python 3.%d%s%s", Binding.Minor, Binding.Shared ? " (already loaded by OBS)" : "", From.c_str());

	// OBS's Python scripting may have started the interpreter already. Share it rather than starting a second one
	if(Py_IsInitialized())
	{
		const ScopedGil Gil;
		AddToSysPath(CharacterControlOBS::GetGlobalSettingsPath().value_or(""));
		return true;
	}

	// Starting an interpreter that can't find its standard library takes OBS down with it, so refuse to
	if(Binding.Home.empty())
	{
		LastError = "The Python that OBS loaded is not running and its standard library could not be found. Restart OBS, or reinstall Python from https://www.python.org/downloads/.";
		ERROR_("%s", LastError.c_str());
		return false;
	}

	// Point Python at its install rather than relying on its registry entry. Optional: Python deprecated this call in 3.11
	// and plans to drop it, and without it Python finds the standard library itself.
	if(auto* SetPythonHome = reinterpret_cast<void(__cdecl*)(const wchar_t*)>(FindPythonExport(Binding, "Py_SetPythonHome")))
	{
		PythonHomeStorage = Binding.Home.wstring();
		SetPythonHome(PythonHomeStorage.c_str());
	}

	Py_Initialize();

	// Python's own startup sets sys.argv, embedding leaves it unset, and scripts and libraries may read it
	if(PyObject* Argv = PyList_New(1))
	{
		PyList_SetItem(Argv, 0, PyUnicode_FromString(""));
		PySys_SetObject("argv", Argv);
		Py_DECREF(Argv);
	}

	// Scripts sit next to the settings, so they come first on the import path
	if(const auto SettingsPath = CharacterControlOBS::GetGlobalSettingsPath(); SettingsPath.has_value() && !SettingsPath->empty())
	{
		AddToSysPath(*SettingsPath, true);
	}

	/* Setup the __main__ module for us to use */
	auto* const MainModule = PyImport_ImportModule("__main__");
	auto* const MainDict = PyModule_GetDict(MainModule);

	/* Fetch the sys module */
	auto* const SysModule = PyImport_ImportModule("sys");

	/* Attach the sys module into the __main__ namespace */
	PyDict_SetItemString(MainDict, "sys", SysModule);

	// Initialization leaves this thread holding the GIL; release it so ScopedGIL works from any thread
	MainThreadState = PyEval_SaveThread();
	return true;
}
