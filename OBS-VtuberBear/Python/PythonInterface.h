#pragma once
#include "Characters/ActiveCharacters.h"

namespace CharacterControlScript
{
// Holds the GIL for the current scope. Every call into Python needs one: OBS ticks sources on its
// graphics thread, and OBS's own Python scripting may be using the same interpreter.
class ScopedGil final
{
public:
	ScopedGil() : State(PyGILState_Ensure()) {}

	~ScopedGil()
	{
		PyGILState_Release(State);
	}

	ScopedGil(const ScopedGil&) = delete;
	ScopedGil(ScopedGil&&) = delete;
	ScopedGil& operator=(const ScopedGil&) = delete;
	ScopedGil& operator=(ScopedGil&&) = delete;

private:
	PyGILState_STATE State;
};

// The Python the plugin's scripts run on. The plugin does not link to a particular Python: any Python 3 on the machine
// is found and loaded the first time TryGet() is called (see PythonLocator.h). Python calls, including ScopedGil, must
// only be made once TryGet() has returned non-null.
class PythonInterface final
{
public:
	PythonInterface(const PythonInterface&) = delete;
	PythonInterface(PythonInterface&&) = delete;
	PythonInterface& operator=(const PythonInterface&) = delete;
	PythonInterface& operator=(PythonInterface&&) = delete;

	~PythonInterface()
	{
		// Only shut down an interpreter we started
		if(MainThreadState != nullptr)
		{
			PyEval_RestoreThread(MainThreadState);
			Py_FinalizeEx();
		}
	}

	// Starts the interpreter, or joins the one OBS's own scripting already runs, on the first call. Null if there is no usable
	// Python 3; that is logged once and GetLastError() says what to do about it.
	static PythonInterface* TryGet();

	// True once TryGet() has succeeded. Never starts anything, so it is safe where Python calls would otherwise be made
	// during teardown.
	static bool IsRunning()
	{
		return Singleton != nullptr;
	}

	// What the user should do after TryGet() failed. Empty otherwise.
	static const Str& GetLastError()
	{
		return LastError;
	}

	// Makes modules in the folder importable. Caller must hold the GIL.
	static void AddToSysPath(const std::filesystem::path& Folder, bool Front = false);

private:
	PythonInterface() = default;

	bool Start();

	PyThreadState* MainThreadState = nullptr;

	static std::unique_ptr<PythonInterface> Singleton;
	static Str LastError;
};
}
