// WatchedFile reloads a file when it changes on disk, but looks at the disk at most every 10 seconds. That interval is fixed,
// so the tests that need a reload wait it out; they are in WatchedFileSlow and can be left out with
// --gtest_filter=-WatchedFileSlow.*
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <CharacterControlCore/WatchedFile.h>
#include "TestSupport.h"

using namespace CharacterControlTests;

namespace
{
struct Payload
{
	std::string Text;
};

// Loads the file's text, and keeps count of what WatchedFile asked of it
class TextFile : public CharacterControlCore::WatchedFile<Payload>
{
public:
	using WatchedFile::WatchedFile;

	int Loads = 0;
	bool FailLoads = false;

	// The text of the old file each time one was disposed of, so a dispose with nothing loaded yet isn't counted
	std::vector<std::string> Disposed;

protected:
	virtual bool UpdateInternal() override
	{
		++Loads;
		if(FailLoads)
		{
			return false;
		}

		GetMutable()->Text = ReadText(GetPath());
		return true;
	}

	virtual void DisposeInternal() override
	{
		if(GetMutable() != nullptr)
		{
			Disposed.push_back(GetMutable()->Text);
		}
	}
};

class WatchedFileTest : public testing::Test
{
protected:
	TempDir Dir;
};

// File times only move forward when a test says so, so a rewrite right after a write can't land on the same timestamp
void Touch(const std::filesystem::path& Path)
{
	std::filesystem::last_write_time(Path, std::filesystem::last_write_time(Path) + 2s);
}

// Longer than WatchedFile's 10 second interval
void WaitOutInterval()
{
	std::this_thread::sleep_for(10s + 200ms);
}
}

TEST_F(WatchedFileTest, GetPath_ReturnsTheWatchedPath)
{
	const auto Path = Dir.File("Rig.json");
	TextFile File(Path);

	EXPECT_EQ(File.GetPath(), Path);
}

TEST_F(WatchedFileTest, NothingIsLoadedBeforeTheFirstUpdate)
{
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "one");
	TextFile File(Path);

	EXPECT_EQ(File.Get(), nullptr);
	EXPECT_EQ(File.Loads, 0);
}

TEST_F(WatchedFileTest, FirstUpdate_LoadsTheFile)
{
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "one");
	TextFile File(Path);

	EXPECT_TRUE(File.Update());

	ASSERT_NE(File.Get(), nullptr);
	EXPECT_EQ(File.Get()->Text, "one");
	EXPECT_EQ(File.Loads, 1);
	EXPECT_TRUE(File.Disposed.empty());
}

TEST_F(WatchedFileTest, MissingFile_LoadsNothing)
{
	TextFile File(Dir.File("Missing.json"));

	EXPECT_FALSE(File.Update());

	EXPECT_EQ(File.Get(), nullptr);
	EXPECT_EQ(File.Loads, 0);
}

TEST_F(WatchedFileTest, FailedLoad_LeavesNoFile)
{
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "one");
	TextFile File(Path);
	File.FailLoads = true;

	EXPECT_FALSE(File.Update());

	EXPECT_EQ(File.Get(), nullptr);
	EXPECT_EQ(File.Loads, 1);
}

TEST_F(WatchedFileTest, UpdateWithinTheInterval_DoesNotLookAtTheFileAgain)
{
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "one");
	TextFile File(Path);
	File.Update();
	const Payload* Loaded = File.Get();

	WriteText(Path, "two");
	Touch(Path);
	File.Update();

	EXPECT_EQ(File.Loads, 1);
	EXPECT_EQ(File.Get(), Loaded);
	EXPECT_EQ(File.Get()->Text, "one");
}

TEST_F(WatchedFileTest, MoveConstruct_TakesTheLoadedFileAndPath)
{
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "one");
	TextFile File(Path);
	File.Update();
	const Payload* Loaded = File.Get();

	TextFile Moved(std::move(File));

	EXPECT_EQ(Moved.GetPath(), Path);
	EXPECT_EQ(Moved.Get(), Loaded);
	// NOLINTNEXTLINE(bugprone-use-after-move): the moved-from file is what is being checked
	EXPECT_EQ(File.Get(), nullptr);
}

TEST(WatchedFileSlow, AfterTheInterval_ReloadsAChangedFileAndLeavesAnUnchangedOneAlone)
{
	TempDir Dir;
	const auto ChangedPath = Dir.File("Changed.json");
	const auto UnchangedPath = Dir.File("Unchanged.json");
	WriteText(ChangedPath, "old");
	WriteText(UnchangedPath, "same");
	TextFile Changed(ChangedPath);
	TextFile Unchanged(UnchangedPath);
	Changed.Update();
	Unchanged.Update();
	const Payload* UnchangedLoaded = Unchanged.Get();

	WriteText(ChangedPath, "new");
	Touch(ChangedPath);
	WaitOutInterval();
	Changed.Update();
	Unchanged.Update();

	EXPECT_EQ(Changed.Loads, 2);
	ASSERT_NE(Changed.Get(), nullptr);
	EXPECT_EQ(Changed.Get()->Text, "new");
	// The old contents are still there when DisposeInternal runs, so it can release what they point at
	EXPECT_EQ(Changed.Disposed, std::vector<std::string>{"old"});

	EXPECT_EQ(Unchanged.Loads, 1);
	EXPECT_EQ(Unchanged.Get(), UnchangedLoaded);
}

TEST(WatchedFileSlow, AfterTheInterval_AFailedReloadDropsTheOldFile)
{
	TempDir Dir;
	const auto Path = Dir.File("Rig.json");
	WriteText(Path, "old");
	TextFile File(Path);
	File.Update();

	WriteText(Path, "broken");
	Touch(Path);
	File.FailLoads = true;
	WaitOutInterval();

	EXPECT_FALSE(File.Update());

	EXPECT_EQ(File.Get(), nullptr);
	EXPECT_EQ(File.Disposed, std::vector<std::string>{"old"});
}
