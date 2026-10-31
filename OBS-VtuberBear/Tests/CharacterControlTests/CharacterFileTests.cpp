// Reading a character's character.json. The rule that keeps its paths inside the character's folder is tested in PathRulesTests.cpp.
// Problems go to the log through blog(), which ObsStubs.h records.
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include "PCH.h"
#include "CharacterFile.h"
#include "ObsStubs.h"
#include "TestSupport.h"

using namespace CharacterControlOBS;
using namespace CharacterControlTests;

namespace
{
const FilePath FOLDER = FilePath(L"C:\\Characters\\123456789012345678");

class CharacterFileTest : public testing::Test
{
protected:
	void SetUp() override
	{
		ObsStubs::Reset();
	}

	static std::optional<CharacterFile> Parse(const Str& Text)
	{
		return ParseCharacterFile(Text, FOLDER);
	}

	// The text is refused, and the log names the file and says why
	static void ExpectRefused(const Str& Text, const Str& Reason)
	{
		SCOPED_TRACE(Text);
		ObsStubs::Reset();

		EXPECT_EQ(Parse(Text), std::nullopt);
		EXPECT_TRUE(ObsStubs::LogContains("character.json")) << "nothing about character.json was logged";
		EXPECT_TRUE(ObsStubs::LogContains(Reason)) << "expected the log to say: " << Reason;
	}
};
}

TEST_F(CharacterFileTest, ReadsEveryField)
{
	const std::optional<CharacterFile> Character = Parse(R"({"name": "Bear", "script": "control.py", "base": "styles/default.json"})");

	ASSERT_TRUE(Character);
	EXPECT_EQ(Character->Name, "Bear");
	EXPECT_EQ(Character->Script, FOLDER / "control.py");
	EXPECT_EQ(Character->BaseStyle, FOLDER / "styles" / "default.json");
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(CharacterFileTest, NameFallsBackToTheFolderName)
{
	const std::optional<CharacterFile> Character = Parse(R"({"script": "control.py", "base": "default.json"})");

	ASSERT_TRUE(Character);
	EXPECT_EQ(Character->Name, "123456789012345678");
}

// The wardrobe and starting stack come in a later step; a character file that already has them still loads
TEST_F(CharacterFileTest, IgnoresFieldsItDoesNotReadYet)
{
	EXPECT_TRUE(Parse(R"({"script": "control.py", "base": "default.json", "styles": {"hat": "hat.json"}, "applied": ["hat"]})"));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(CharacterFileTest, RefusesWhatIsNotACharacter)
{
	ExpectRefused("{", "Json is malformed.");
	ExpectRefused(R"(["control.py"])", "It must be a JSON object.");
}

TEST_F(CharacterFileTest, SaysWhatIsWrongWithAField)
{
	ExpectRefused(R"({"base": "default.json"})", "\"script\" is missing.");
	ExpectRefused(R"({"script": 5, "base": "default.json"})", "\"script\" must be a string.");
	ExpectRefused(R"({"script": "../control.py", "base": "default.json"})",
		"\"script\" must be a path inside the character's folder, not \"../control.py\".");
	ExpectRefused(R"({"script": "control.py"})", "\"base\" is missing.");
	ExpectRefused(R"({"script": "control.py", "base": "\\\\server\\share\\default.json"})",
		"\"base\" must be a path inside the character's folder, not \"\\\\server\\share\\default.json\".");
	ExpectRefused(R"({"name": 5, "script": "control.py", "base": "default.json"})", "\"name\" must be a string.");
}

TEST_F(CharacterFileTest, LoadsTheFoldersCharacterJson)
{
	const TempDir Dir;
	const FilePath Folder = Dir.File("234567890123456789");
	std::filesystem::create_directories(Folder);
	WriteText(Folder / "character.json", R"({"script": "control.py", "base": "styles/default.json"})");

	const std::optional<CharacterFile> Character = LoadCharacterFile(Folder);

	ASSERT_TRUE(Character);
	EXPECT_EQ(Character->Name, "234567890123456789");
	EXPECT_EQ(Character->Script, Folder / "control.py");
	EXPECT_EQ(Character->BaseStyle, Folder / "styles" / "default.json");
}

TEST_F(CharacterFileTest, SaysWhenThereIsNoCharacterJson)
{
	const TempDir Dir;

	EXPECT_EQ(LoadCharacterFile(Dir.File("empty")), std::nullopt);
	EXPECT_TRUE(ObsStubs::LogContains("Cannot open"));
	EXPECT_TRUE(ObsStubs::LogContains("character.json"));
}
