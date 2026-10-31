// Each scenario runs the probe (Probe.h) in a child process, because a process can only be bound to one Python and the
// scenarios differ in what Python the process can see. The Python they find is the one in PYTHONPATH, the same one
// the plugin is built against, so the tests behave the same on a machine with any Python 3.8 or newer.
#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "Probe.h"

using namespace PythonLocatorTests;

// PYTHON_HOME_DIR is the build's PYTHONPATH, passed as a bare define because quotes don't survive the project's define list
#define WIDE_TEXT_(Text) L## #Text
#define WIDE_TEXT(Text) WIDE_TEXT_(Text)

namespace
{
struct ProbeRun
{
	DWORD ExitCode = static_cast<DWORD>(-1);
	std::string Output;
};

std::wstring SystemFolders()
{
	std::array<wchar_t, MAX_PATH> System = {};
	std::array<wchar_t, MAX_PATH> Windows = {};
	GetSystemDirectoryW(System.data(), MAX_PATH);
	GetWindowsDirectoryW(Windows.data(), MAX_PATH);
	return std::wstring(System.data()) + L";" + Windows.data();
}

// The environment block of this process with PATH replaced
std::wstring EnvironmentWithPath(const std::wstring& Path)
{
	std::wstring Block;
	wchar_t* Strings = GetEnvironmentStringsW();
	for(const wchar_t* Entry = Strings; *Entry != L'\0'; Entry += wcslen(Entry) + 1)
	{
		const bool IsPath = _wcsnicmp(Entry, L"PATH=", 5) == 0;
		Block += IsPath ? L"PATH=" + Path : std::wstring(Entry);
		Block += L'\0';
	}
	FreeEnvironmentStringsW(Strings);
	Block += L'\0';
	return Block;
}

// Runs this exe as the probe, able to see only the folders in Path
ProbeRun RunProbeProcess(const std::vector<std::wstring>& Args, const std::wstring& Path)
{
	ProbeRun Run;

	std::array<wchar_t, MAX_PATH> Self = {};
	GetModuleFileNameW(nullptr, Self.data(), MAX_PATH);
	std::wstring CommandLine = L"\"" + std::wstring(Self.data()) + L"\" " + PROBE_FLAG;
	for(const std::wstring& Arg : Args)
	{
		CommandLine += L" \"" + Arg + L"\"";
	}

	SECURITY_ATTRIBUTES Inheritable = {sizeof(Inheritable), nullptr, TRUE};
	HANDLE Read = nullptr;
	HANDLE Write = nullptr;
	if(!CreatePipe(&Read, &Write, &Inheritable, 0))
	{
		return Run;
	}
	SetHandleInformation(Read, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW Startup = {sizeof(Startup)};
	Startup.dwFlags = STARTF_USESTDHANDLES;
	Startup.hStdOutput = Write;
	Startup.hStdError = Write;
	Startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	std::wstring Environment = EnvironmentWithPath(Path);
	PROCESS_INFORMATION Process = {};
	const BOOL Started = CreateProcessW(Self.data(), CommandLine.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, Environment.data(), nullptr, &Startup, &Process);
	CloseHandle(Write);
	if(!Started)
	{
		CloseHandle(Read);
		return Run;
	}

	std::array<char, 4096> Buffer = {};
	DWORD Count = 0;
	while(ReadFile(Read, Buffer.data(), static_cast<DWORD>(Buffer.size()), &Count, nullptr) && Count > 0)
	{
		Run.Output.append(Buffer.data(), Count);
	}

	WaitForSingleObject(Process.hProcess, 60000);
	GetExitCodeProcess(Process.hProcess, &Run.ExitCode);
	CloseHandle(Process.hProcess);
	CloseHandle(Process.hThread);
	CloseHandle(Read);
	return Run;
}

bool Contains(const std::string& Text, const std::string& Part)
{
	return Text.find(Part) != std::string::npos;
}

class PythonLocatorTest : public testing::Test
{
protected:
	virtual void SetUp() override
	{
		PythonHome = std::filesystem::path(WIDE_TEXT(PYTHON_HOME_DIR)).make_preferred();
		Minor = InstalledMinor(PythonHome);
		ASSERT_NE(Minor, -1) << "No python3<minor>.dll in PYTHONPATH " << PythonHome.string();
		PythonDll = PythonHome / (L"python3" + std::to_wstring(Minor) + L".dll");
	}

	virtual void TearDown() override
	{
		std::error_code Error;
		if(!Broken.empty())
		{
			std::filesystem::remove_all(Broken, Error);
		}
	}

	// A Python install with nothing but its DLL: what is left of one after its Lib folder is deleted, and what starting it would crash on
	std::wstring MakeBrokenInstall()
	{
		Broken = std::filesystem::temp_directory_path() / (L"PythonLocatorTests-" + std::to_wstring(GetCurrentProcessId()));
		std::filesystem::create_directories(Broken);
		std::filesystem::copy_file(PythonDll, Broken / PythonDll.filename(), std::filesystem::copy_options::overwrite_existing);
		return Broken.wstring();
	}

	std::wstring PathWithPython() const
	{
		return SystemFolders() + L";" + PythonHome.wstring();
	}

	std::filesystem::path PythonHome;
	std::filesystem::path PythonDll;
	std::filesystem::path Broken;
	int Minor = -1;
};
}

TEST_F(PythonLocatorTest, NoPythonAnywhere)
{
	const ProbeRun Run = RunProbeProcess({L"noreg"}, SystemFolders());
	EXPECT_EQ(Run.ExitCode, 2u) << Run.Output;
	EXPECT_TRUE(Contains(Run.Output, "RESULT: no usable Python")) << Run.Output;
}

TEST_F(PythonLocatorTest, FindsPythonOnPath)
{
	const ProbeRun Run = RunProbeProcess({L"noreg"}, PathWithPython());
	EXPECT_EQ(Run.ExitCode, 0u) << Run.Output;
	EXPECT_TRUE(Contains(Run.Output, "RESULT: bound Python 3." + std::to_string(Minor))) << Run.Output;
	EXPECT_TRUE(Contains(Run.Output, "PYTHON OK")) << Run.Output;
}

// Starting a Python that can't find its standard library aborts the whole process, so it must be passed over instead
TEST_F(PythonLocatorTest, SkipsPythonWithoutStandardLibrary)
{
	const ProbeRun Run = RunProbeProcess({L"noreg", L"prefer=" + MakeBrokenInstall()}, SystemFolders());
	EXPECT_EQ(Run.ExitCode, 2u) << Run.Output;
	EXPECT_TRUE(Contains(Run.Output, "no standard library found")) << Run.Output;
}
