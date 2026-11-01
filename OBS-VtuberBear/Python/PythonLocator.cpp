#include "PCH.h"
#include "Python/PythonLocator.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cwchar>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <delayimp.h>

using namespace CharacterControlScript;

namespace
{
// Where the delay-loaded python3.dll imports resolve to. They are resolved against this module rather than against a
// python3.dll, which every Python provides but only next to its own python3<minor>.dll, and which exports the same names.
HMODULE BoundModule = nullptr;

FARPROC WINAPI DelayLoadHook(const unsigned Notification, DelayLoadInfo* const Info)
{
	if(Notification == dliNotePreLoadLibrary && BoundModule != nullptr && _stricmp(Info->szDll, "python3.dll") == 0)
	{
		return reinterpret_cast<FARPROC>(BoundModule);
	}

	return nullptr;
}

struct Candidate
{
	// Lower ranks are tried first
	int Rank = 0;
	int Minor = 0;
	std::filesystem::path Dll;
	std::filesystem::path Home;
	std::string Source;

	// Set when the DLL is already in the process
	HMODULE Loaded = nullptr;
};

std::wstring DllName(const int Minor)
{
	return L"python3" + std::to_wstring(Minor) + L".dll";
}

std::string Narrow(const std::filesystem::path& Path)
{
	const std::u8string Utf8 = Path.u8string();
	return {Utf8.begin(), Utf8.end()};
}

bool Exists(const std::filesystem::path& Path)
{
	std::error_code Error;
	return exists(Path, Error);
}

std::filesystem::path SearchOnPath(const std::wstring& Name)
{
	std::array<wchar_t, MAX_PATH> Buffer = {};
	if(const DWORD Length = SearchPathW(nullptr, Name.c_str(), nullptr, MAX_PATH, Buffer.data(), nullptr); Length == 0 || Length >= MAX_PATH)
	{
		return {};
	}

	// SearchPath also looks in the current directory; don't load a DLL just because OBS was started from next to one
	std::filesystem::path Found(Buffer.data());
	if(std::error_code Error; equivalent(Found.parent_path(), std::filesystem::current_path(Error), Error))
	{
		Found.clear();
	}

	return Found;
}

std::vector<std::wstring> SubKeys(const HKEY Key)
{
	std::vector<std::wstring> Names;
	std::array<wchar_t, 256> Name = {};
	for(DWORD Index = 0;; ++Index)
	{
		auto Length = static_cast<DWORD>(std::size(Name));
		if(RegEnumKeyExW(Key, Index, Name.data(), &Length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
		{
			break;
		}

		Names.emplace_back(Name.data(), Length);
	}
	return Names;
}

std::wstring ReadInstallPath(const HKEY Tag)
{
	constexpr DWORD Flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
	DWORD Size = 0;
	if(RegGetValueW(Tag, L"InstallPath", nullptr, Flags, nullptr, nullptr, &Size) != ERROR_SUCCESS || Size < sizeof(wchar_t))
	{
		return {};
	}

	std::wstring Value(Size / sizeof(wchar_t), L'\0');
	if(RegGetValueW(Tag, L"InstallPath", nullptr, Flags, nullptr, Value.data(), &Size) != ERROR_SUCCESS)
	{
		Value.clear();
	} else
	{
		// The size includes the terminator
		Value.resize(wcsnlen(Value.c_str(), Value.size()));
	}

	return Value;
}

// Registry tags look like "3.12" or "3.14-64"; other architectures can't be loaded into 64-bit OBS
int MinorFromTag(const std::wstring& Tag)
{
	if(!Tag.starts_with(L"3."))
	{
		return -1;
	}

	const wchar_t* Digits = Tag.c_str() + 2;
	wchar_t* End = nullptr;
	const long Minor = std::wcstol(Digits, &End, 10);
	if(End == Digits || (*End != L'\0' && _wcsicmp(End, L"-64") != 0))
	{
		return -1;
	}

	return static_cast<int>(Minor);
}

// python.org's installer, the Python install manager, Anaconda and others register as HKCU or HKLM\Software\Python\<company>\<tag>
void AddRegistryInstalls(const HKEY Root, const char* RootName, std::vector<Candidate>& Out)
{
	HKEY Python = nullptr;
	if(RegOpenKeyExW(Root, L"Software\\Python", 0, KEY_READ | KEY_WOW64_64KEY, &Python) != ERROR_SUCCESS)
	{
		return;
	}

	for(const std::wstring& Company : SubKeys(Python))
	{
		HKEY CompanyKey = nullptr;
		if(RegOpenKeyExW(Python, Company.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &CompanyKey) != ERROR_SUCCESS)
		{
			continue;
		}

		for(const std::wstring& Tag : SubKeys(CompanyKey))
		{
			const int Minor = MinorFromTag(Tag);
			if(Minor < MIN_PYTHON_MINOR || Minor > MAX_PYTHON_MINOR)
			{
				continue;
			}

			HKEY TagKey = nullptr;
			if(RegOpenKeyExW(CompanyKey, Tag.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &TagKey) != ERROR_SUCCESS)
			{
				continue;
			}

			const std::wstring InstallPath = ReadInstallPath(TagKey);
			RegCloseKey(TagKey);
			if(InstallPath.empty())
			{
				continue;
			}

			Candidate Item;
			Item.Rank = 2;
			Item.Minor = Minor;
			Item.Home = InstallPath;
			Item.Dll = Item.Home / DllName(Minor);

			// Some all-users installs keep the DLL in System32 and only the standard library in the install folder
			if(!Exists(Item.Dll))
			{
				Item.Dll = SearchOnPath(DllName(Minor));
			}
			Item.Source = std::string("registry ") + RootName + "\\Software\\Python\\" + Narrow(Company) + "\\" + Narrow(Tag);
			Out.push_back(std::move(Item));
		}
		RegCloseKey(CompanyKey);
	}
	RegCloseKey(Python);
}

std::vector<Candidate> FindCandidates(const std::vector<std::filesystem::path>& PreferredFolders)
{
	std::vector<Candidate> Found;

	for(int Minor = MAX_PYTHON_MINOR; Minor >= MIN_PYTHON_MINOR; --Minor)
	{
		const std::wstring Name = DllName(Minor);

		if(HMODULE Loaded = GetModuleHandleW(Name.c_str()))
		{
			std::array<wchar_t, MAX_PATH> Buffer = {};
			const DWORD Length = GetModuleFileNameW(Loaded, Buffer.data(), MAX_PATH);
			Candidate Item;
			Item.Rank = 0;
			Item.Minor = Minor;
			Item.Dll = std::filesystem::path(std::wstring(Buffer.data(), Length));
			Item.Home = Item.Dll.parent_path();
			Item.Source = "already loaded into OBS";
			Item.Loaded = Loaded;
			Found.push_back(std::move(Item));
		}

		for(const std::filesystem::path& Folder : PreferredFolders)
		{
			if(!Exists(Folder / Name))
			{
				continue;
			}

			Candidate Item;
			Item.Rank = 1;
			Item.Minor = Minor;
			Item.Dll = Folder / Name;
			Item.Home = Folder;
			Item.Source = "OBS's Python setting";
			Found.push_back(std::move(Item));
		}

		if(const std::filesystem::path OnPath = SearchOnPath(Name); !OnPath.empty())
		{
			Candidate Item;
			Item.Rank = 2;
			Item.Minor = Minor;
			Item.Dll = OnPath;
			Item.Home = OnPath.parent_path();
			Item.Source = "PATH";
			Found.push_back(std::move(Item));
		}
	}

	AddRegistryInstalls(HKEY_CURRENT_USER, "HKCU", Found);
	AddRegistryInstalls(HKEY_LOCAL_MACHINE, "HKLM", Found);

	std::ranges::stable_sort(Found,
		[](const Candidate& Left, const Candidate& Right)
		{
			return Left.Rank != Right.Rank ? Left.Rank < Right.Rank : Left.Minor > Right.Minor;
		});

	// The same DLL is often reachable several ways (registry and PATH, say); keep the best ranked
	std::vector<Candidate> Unique;
	for(Candidate& Item : Found)
	{
		if(const bool Seen = std::ranges::any_of(Unique,
			[&](const Candidate& Other)
			{
				return _wcsicmp(Other.Dll.c_str(), Item.Dll.c_str()) == 0;
			}); !Seen)
		{
			Unique.push_back(std::move(Item));
		}
	}
	return Unique;
}

enum class TryResult
{
	Skipped,
	Bound,
	Failed
};

TryResult TryCandidate(const Candidate& Item, PythonBinding& Out)
{
	const std::string Label = Narrow(Item.Dll) + " (" + Item.Source + ")";
	const bool WasLoaded = Item.Loaded != nullptr;

	HMODULE Module = Item.Loaded;
	if(Module == nullptr)
	{
		if(!Exists(Item.Dll))
		{
			Out.Skipped.push_back(Label + ": file not found");
			return TryResult::Skipped;
		}

		// Altered search path so the DLL's own dependencies, such as the Visual C++ runtime, are found beside it
		Module = LoadLibraryExW(Item.Dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
		if(Module == nullptr)
		{
			Out.Skipped.push_back(Label + ": could not be loaded (error " + std::to_string(GetLastError()) + "; 32-bit Python, or the Visual C++ runtime is missing)");
			return TryResult::Skipped;
		}
	}

	if(GetProcAddress(Module, "Py_InitializeEx") == nullptr)
	{
		Out.Skipped.push_back(Label + ": not a CPython runtime");
		if(!WasLoaded)
		{
			FreeLibrary(Module);
		}
		return TryResult::Skipped;
	}

	// A Python that isn't running yet has to be started, and one that can't find its standard library aborts the whole process
	const bool HasLibrary = HasStandardLibrary(Item.Home, Item.Minor);
	if(!WasLoaded && !HasLibrary)
	{
		Out.Skipped.push_back(Label + ": no standard library found in " + Narrow(Item.Home));
		FreeLibrary(Module);
		return TryResult::Skipped;
	}

	BoundModule = Module;
	if(const HRESULT Result = __HrLoadAllImportsForDll("python3.dll"); FAILED(Result))
	{
		// Some imports may already be bound to this one, so no other Python can be used after this
		Out.Skipped.push_back(Label + ": lacks functions the plugin needs (error " + std::to_string(Result) + ")");
		BoundModule = nullptr;
		return TryResult::Failed;
	}

	Out.Module = Module;
	Out.Minor = Item.Minor;
	Out.Shared = WasLoaded;
	Out.Home = HasLibrary ? Item.Home : std::filesystem::path();
	return TryResult::Bound;
}
}

// The delay-load helper looks this up by name
// NOLINTNEXTLINE(bugprone-reserved-identifier)
extern "C" const PfnDliHook __pfnDliNotifyHook2 = DelayLoadHook;

bool CharacterControlScript::BindPython(const std::vector<std::filesystem::path>& PreferredFolders, PythonBinding& Out)
{
	for(const Candidate& Item : FindCandidates(PreferredFolders))
	{
		switch(TryCandidate(Item, Out))
		{
			case TryResult::Bound :
				return true;
			case TryResult::Failed :
				return false;
			case TryResult::Skipped :
				break;
		}
	}
	return false;
}

void* CharacterControlScript::FindPythonExport(const PythonBinding& Binding, const char* Name)
{
	return Binding.Module == nullptr ? nullptr : reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(Binding.Module), Name));
}

bool CharacterControlScript::HasStandardLibrary(const std::filesystem::path& Home, const int Minor)
{
	if(Home.empty())
	{
		return false;
	}

	return Exists(Home / L"Lib" / L"os.py") || Exists(Home / (L"python3" + std::to_wstring(Minor) + L".zip"));
}
