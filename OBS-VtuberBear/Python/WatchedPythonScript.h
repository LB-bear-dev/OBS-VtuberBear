#pragma once
#include <CharacterControlCore/WatchedFile.h>
#define PY_SSIZE_T_CLEAN
// pyconfig.h switches to the debug Python ABI (Py_DEBUG, python312_d.lib) whenever _DEBUG is set.
// Only the release Python is installed, so include it as if this were a release build.
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

namespace CharacterControlScript
{
class WatchedPythonScript final : public CharacterControlCore::WatchedFile<PyObject*>
{
public:
	explicit WatchedPythonScript(const std::filesystem::path& Filename) : WatchedFile(Filename)
	{
		Update();
	}

	// WatchedFile's destructor can't reach our DisposeInternal override, so release the module here.
	~WatchedPythonScript() override
	{
		WatchedPythonScript::DisposeInternal();
	}

	WatchedPythonScript(const WatchedPythonScript&) = delete;
	WatchedPythonScript(WatchedPythonScript&&) = delete;
	WatchedPythonScript& operator=(const WatchedPythonScript&) = delete;
	WatchedPythonScript& operator=(WatchedPythonScript&&) = delete;

	PyObject* Call(const std::string& FnName, PyObject* Args) const;

protected:
	bool UpdateInternal() override;
	void DisposeInternal() override;
};
}
