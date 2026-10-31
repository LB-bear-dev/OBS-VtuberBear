#pragma once
// Shared setup for the CharacterControlTests: a scratch folder per test, and helpers to put files in it.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <iterator>
#include <process.h>
#include <share.h>
#include <string>
#include <vector>

namespace CharacterControlTests
{
// psd_sdk asserts on corrupt input in Debug builds, so the tests that feed it some skip there. A constant rather than #ifdef
// keeps their bodies compiled in every build.
#ifdef NDEBUG
constexpr bool IS_DEBUG_BUILD = false;
#else
constexpr bool IS_DEBUG_BUILD = true;
#endif

// A folder of its own under the system temp folder, removed again when the test ends
class TempDir
{
public:
	TempDir()
	{
		static int Counter = 0;
		Root = std::filesystem::temp_directory_path() / ("CharacterControlTests-" + std::to_string(_getpid()) + "-" + std::to_string(Counter++));
		std::filesystem::create_directories(Root);
	}

	~TempDir()
	{
		std::error_code Ignored;
		std::filesystem::remove_all(Root, Ignored);
	}

	TempDir(const TempDir&) = delete;
	TempDir& operator=(const TempDir&) = delete;

	[[nodiscard]] std::filesystem::path File(const std::string& Name) const
	{
		return Root / Name;
	}

private:
	std::filesystem::path Root;
};

// Sends what the process prints to stdout nowhere until it goes out of scope. psd_sdk prints a line for every read of a damaged file
// that can't finish, hundreds of them over a run, and the loader reports the failure itself anyway. Restore is public so the fuzz
// test's watchdog can get its message out while a load is still going.
class QuietStdout
{
public:
	QuietStdout()
	{
		fflush(stdout);
		const int Real = _dup(_fileno(stdout));
		int Null = -1;
		bool Redirected = false;
		if(Real >= 0 && _sopen_s(&Null, "NUL", _O_WRONLY, _SH_DENYNO, 0) == 0)
		{
			Redirected = _dup2(Null, _fileno(stdout)) == 0;
			_close(Null);
		}

		if(Redirected)
		{
			Saved = Real;
			return;
		}
		if(Real >= 0)
		{
			_close(Real);
		}
	}

	QuietStdout(const QuietStdout&) = delete;
	QuietStdout& operator=(const QuietStdout&) = delete;

	~QuietStdout()
	{
		Restore();
	}

	static void Restore()
	{
		const int Real = Saved.exchange(-1);
		if(Real < 0)
		{
			return;
		}

		// Whatever is still buffered is the noise, so it goes before stdout is pointed back
		fflush(stdout);
		_dup2(Real, _fileno(stdout));
		_close(Real);
	}

private:
	static inline std::atomic<int> Saved = -1;
};

inline void WriteBytes(const std::filesystem::path& Path, const std::vector<uint8_t>& Bytes)
{
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	Out.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
}

inline void WriteText(const std::filesystem::path& Path, const std::string& Text)
{
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	Out << Text;
}

inline std::string ReadText(const std::filesystem::path& Path)
{
	std::ifstream In(Path, std::ios::binary);
	return {std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>()};
}
}
