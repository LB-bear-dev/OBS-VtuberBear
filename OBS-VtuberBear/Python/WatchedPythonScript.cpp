#include "PCH.h"
#include "Python/WatchedPythonScript.h"
#include "Python/PythonInterface.h"

PyObject* CharacterControlScript::WatchedPythonScript::Call(const std::string& FnName, PyObject* Args) const
{
	if(!PythonInterface::IsRunning())
	{
		return nullptr;
	}

	const ScopedGil Gil;
	PyObject* PVal = nullptr;
	if(GetMutable() != nullptr)
	{
		if(PyObject* PyFile = *GetMutable(); PyFile != nullptr)
		{
			PyObject* PFunc = PyObject_GetAttrString(PyFile, FnName.c_str());
			if(PFunc != nullptr && PyCallable_Check(PFunc))
			{
				PVal = PyObject_CallObject(PFunc, Args);
			}

			// A missing function or an exception in the script leaves an error set; report it so it isn't left pending
			if(PyErr_Occurred())
			{
				PyErr_Print();
			}

			// GetAttrString returns null when the function doesn't exist
			Py_XDECREF(PFunc);
		}
	}
	return PVal;
}

bool CharacterControlScript::WatchedPythonScript::UpdateInternal()
{
	if(PythonInterface::TryGet() == nullptr)
	{
		return false;
	}

	const ScopedGil Gil;

	// Import by file name from the script's own folder, wherever that is
	const std::filesystem::path& Path = GetPath();
	PythonInterface::AddToSysPath(Path.parent_path());

	PyObject* PName = PyUnicode_FromWideChar(Path.stem().c_str(), -1);
	if(PName == nullptr)
	{
		PyErr_Print();
		return false;
	}

	// WatchedFile hands us a fresh, empty slot on every reload, so the module always has to be stored again
	if(PyObject* PyFile = PyImport_GetModule(PName); PyFile == nullptr)
	{
		*GetMutable() = PyImport_Import(PName);
	} else
	{
		*GetMutable() = PyImport_ReloadModule(PyFile);
		Py_DECREF(PyFile);
	}
	Py_DECREF(PName);

	// Python reports failures (including syntax errors in the script) as a null module, not C++ exceptions
	if(*GetMutable() == nullptr)
	{
		PyErr_Print();
		return false;
	}

	return true;
}

void CharacterControlScript::WatchedPythonScript::DisposeInternal()
{
	if(!PythonInterface::IsRunning())
	{
		return;
	}

	const ScopedGil Gil;
	if(GetMutable() != nullptr)
	{
		if(PyObject* PyFile = *GetMutable(); PyFile != nullptr)
		{
			Py_DECREF(PyFile);
		}
	}
}
