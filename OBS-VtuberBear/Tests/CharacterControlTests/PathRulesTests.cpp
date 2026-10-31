// The rule that keeps every path written in a shared file, such as a character's, inside that file's folder
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include "PCH.h"
#include "PathRules.h"

using namespace CharacterControlOBS;

namespace
{
const FilePath FOLDER = FilePath(L"C:\\Characters\\123456789012345678");
}

TEST(ResolveInsideFolder, KeepsPathsInsideTheFolder)
{
	EXPECT_EQ(ResolveInsideFolder(FOLDER, "control.py"), FOLDER / "control.py");
	EXPECT_EQ(ResolveInsideFolder(FOLDER, "styles/default.json"), FOLDER / "styles" / "default.json");
	EXPECT_EQ(ResolveInsideFolder(FOLDER, "styles\\default.json"), FOLDER / "styles" / "default.json");
}

TEST(ResolveInsideFolder, TidiesPathsThatStayInside)
{
	EXPECT_EQ(ResolveInsideFolder(FOLDER, "./control.py"), FOLDER / "control.py");
	EXPECT_EQ(ResolveInsideFolder(FOLDER, "styles/../control.py"), FOLDER / "control.py");
}

TEST(ResolveInsideFolder, RefusesPathsThatLeaveTheFolder)
{
	const std::vector<Str> Paths = {
		"",
		".",
		"styles/..",
		"..",
		"../other/control.py",
		"..\\control.py",
		"styles/../../control.py",
		"C:\\Windows\\win.ini",
		"C:control.py",
		"\\control.py",
		"/control.py",
		"\\\\server\\share\\head.psd",
		"//server/share/head.psd",
		"\\\\?\\C:\\control.py",
	};

	for(const Str& Path : Paths)
	{
		SCOPED_TRACE(Path);
		EXPECT_EQ(ResolveInsideFolder(FOLDER, Path), std::nullopt);
	}
}

// JSON strings are UTF-8, so a non-ASCII name has to survive the trip into a Windows path
TEST(ResolveInsideFolder, ReadsUtf8)
{
	// A u-umlaut followed by ".psd": in UTF-8 bytes as JSON has it, and as the wide character Windows uses
	const Str Utf8 = Str{static_cast<char>(0xC3), static_cast<char>(0xBC)} + ".psd";
	const std::wstring Wide = std::wstring(1, static_cast<wchar_t>(0xFC)) + L".psd";

	EXPECT_EQ(ResolveInsideFolder(FOLDER, Utf8), FOLDER / Wide);
}
