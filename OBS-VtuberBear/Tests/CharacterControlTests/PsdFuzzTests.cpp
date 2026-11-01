// Feeds the PSD loader files with a few bytes changed. PSDs come from artists and the internet, so the loader has to get through
// damaged ones without crashing, whatever it makes of them. The files are valid ones from PsdBuilder.h with bytes flipped,
// set to 0 or 255, or whole 32-bit fields replaced with awkward numbers (sizes, 0, -1, the largest and smallest signed).
//
// The same seed gives the same files, so a failure can be run again. Settings, all optional environment variables:
//   PSD_FUZZ_ITERATIONS  files per starting document, default 300 (a few seconds); 0 skips the test, 1 only loads the unchanged ones
//   PSD_FUZZ_SEED        default 0xC0FFEE
//   PSD_FUZZ_TIMEOUT     seconds a single file may take to load before it counts as a hang, default 30
//   PSD_FUZZ_REPLAY      path of one file to load and check instead, such as one the fuzzer left behind
//
// A file that hangs the loader would hang the test with it, so a watchdog ends the process after PSD_FUZZ_TIMEOUT seconds on one
// file, keeping that file like any other failure.
//
// This only means much in an AddressSanitizer build, which stops on a read or write outside a buffer instead of letting it pass
// unnoticed (see Directory.Build.props for how to make one), and it needs Release, because psd_sdk's asserts end the process
// on corrupt input in Debug. It relies on ThirdParty\patches\psd_sdk.patch, which scripts\setup-deps.ps1 applies: upstream
// psd_sdk crashes or hangs on files like these within the first few hundred.
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <gtest/gtest.h>
#include "ObsStubs.h"
#include "PCH.h"
#include "PsdBuilder.h"
#include "TestSupport.h"
#include "Render/WatchedImage.h"

using namespace CharacterControlRender;
using namespace CharacterControlTests;
namespace Psd = PsdBuilder;

namespace
{
// Unused in Debug builds, which skip the fuzz test
uint64_t EnvNumber(const char* Name, const uint64_t Default)
{
	char* Text = nullptr;
	if(_dupenv_s(&Text, nullptr, Name) != 0 || Text == nullptr)
	{
		return Default;
	}

	const std::unique_ptr<char, decltype(&free)> Owned(Text, &free);
	return std::strtoull(Text, nullptr, 0);
}

std::string EnvText(const char* Name)
{
	char* Text = nullptr;
	if(_dupenv_s(&Text, nullptr, Name) != 0 || Text == nullptr)
	{
		return {};
	}

	const std::unique_ptr<char, decltype(&free)> Owned(Text, &free);
	return Text;
}

WatchedFileImageLibrary LoadQuietly(const std::filesystem::path& Path)
{
	const QuietStdout Quiet;
	return WatchedFileImageLibrary(Path);
}

// Ends the process when one file takes too long, after keeping the file the way a failed check does. The test thread says which
// file it is on with Begin and that it has finished it with End.
class Watchdog
{
public:
	Watchdog(std::filesystem::path Current, const std::chrono::seconds Limit, const uint64_t Seed) : Current(std::move(Current)),
		Limit(Limit),
		Seed(Seed),
		Thread([this]
		{
			Watch();
		}) {}

	Watchdog(const Watchdog&) = delete;
	Watchdog(Watchdog&&) = delete;
	Watchdog& operator=(const Watchdog&) = delete;
	Watchdog& operator=(Watchdog&&) = delete;

	~Watchdog()
	{
		Done = true;
		Thread.join();
	}

	void Begin(const uint64_t NewIteration, const size_t NewStart)
	{
		Iteration = NewIteration;
		Start = NewStart;
		Since = std::chrono::steady_clock::now().time_since_epoch().count();
	}

	void End()
	{
		Since = 0;
	}

private:
	void Watch() const
	{
		while(!Done)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			if(const int64_t Began = Since; Began == 0 || std::chrono::steady_clock::now().time_since_epoch() - std::chrono::steady_clock::duration(Began) < Limit)
			{
				continue;
			}

			const std::filesystem::path Kept = std::filesystem::temp_directory_path() / ("psd-fuzz-failure-" + std::to_string(Seed) + "-" + std::to_string(Iteration) + "-" + std::to_string(Start) +
				".psd");
			std::error_code Error;
			copy_file(Current, Kept, std::filesystem::copy_options::overwrite_existing, Error);
			QuietStdout::Restore();
			printf("PSD fuzz: loading document %zu, file %llu, seed %llu took more than %lld seconds, kept as %s\n",
				Start.load(),
				static_cast<unsigned long long>(Iteration.load()),
				static_cast<unsigned long long>(Seed),
				static_cast<long long>(Limit.count()),
				Kept.string().c_str());
			static_cast<void>(fflush(stdout));
			std::_Exit(3);
		}
	}

	std::filesystem::path Current;
	std::chrono::seconds Limit;
	uint64_t Seed;
	std::atomic<bool> Done = false;
	std::atomic<int64_t> Since = 0; // steady clock ticks when the current file started, 0 between files
	std::atomic<uint64_t> Iteration = 0;
	std::atomic<size_t> Start = 0;
	std::thread Thread; // last, so the rest exists before it runs
};

// xorshift64, written out because the standard's distributions give different numbers on different compilers
class Random
{
public:
	explicit Random(const uint64_t Seed) : State(Seed != 0 ? Seed : 1) {}

	uint64_t Next()
	{
		State ^= State << 13u;
		State ^= State >> 7u;
		State ^= State << 17u;
		return State;
	}

	size_t Below(const size_t Limit)
	{
		return Next() % Limit;
	}

private:
	uint64_t State;
};

struct StartingFile
{
	std::vector<uint8_t> Bytes;
	size_t MutableEnd; // the flattened image at the end is never read, so changing it tells nothing
};

StartingFile Finish(const Psd::Document& Document)
{
	return {.Bytes = Build(Document), .MutableEnd = Build(Document).size() - (2 + static_cast<size_t>(Document.Width) * Document.Height * 3)};
}

// Documents that between them use groups, masks, opacity, unicode names, every compression, and layers hanging off the canvas
std::vector<StartingFile> StartingFiles()
{
	std::vector<StartingFile> Files;

	Psd::Document Groups(16, 16);
	Groups.Add(SolidLayer("Body", Psd::Box(0, 0, 8, 8), 10, 20, 30));
	Groups.Add(Group("Head", 200, Psd::Group("Mouth", 255, {SolidLayer("Open", Psd::Box(2, 2, 4, 4), 40, 50, 60, 128)})));
	Groups.Add(Psd::Group("Eyes", 255, {SolidLayer("Left", Psd::Box(1, 1, 2, 2), 1, 2, 3), SolidLayer("Right", Psd::Box(5, 1, 2, 2), 4, 5, 6)}));
	Files.push_back(Finish(Groups));

	Psd::Document Masked(8, 8);
	Psd::Layer Gradient = PixelLayer("Gradient",
		Psd::Box(0, 0, 8, 8),
		[](const int X, const int Y)
		{
			return Psd::Rgba{static_cast<uint8_t>(X * 30), static_cast<uint8_t>(Y * 30), 90, 255};
		},
		Psd::Rle);
	Gradient.UnicodeName = {0x53E3, u' ', u'x'};
	Gradient.Opacity = 180;
	Psd::Mask Mask;
	Mask.Bounds = Psd::Box(0, 0, 8, 8);
	Mask.DefaultColor = 255;
	for(int Pixel = 0; Pixel < 64; ++Pixel)
	{
		Mask.Pixels.push_back(Pixel % 3 == 0 ? 0 : 255);
	}
	Gradient.LayerMask = Mask;
	Masked.Add(Gradient);
	Masked.Add(PixelLayer("Hanging",
		Psd::Box(-4, -4, 12, 12),
		[](const int X, const int Y)
		{
			return Psd::Rgba{static_cast<uint8_t>(X * 10), 0, static_cast<uint8_t>(Y * 10), 255};
		},
		Psd::Zip));
	Files.push_back(Finish(Masked));

	Psd::Document Compressed(8, 8);
	Compressed.Add(PixelLayer("Stripes",
		Psd::Box(0, 0, 8, 8),
		[](const int X, int)
		{
			return Psd::Rgba{static_cast<uint8_t>(X < 4 ? 10 : 200), 7, 7, 255};
		},
		Psd::ZipPrediction));
	Compressed.Add(Psd::Group("Faded",
		128,
		{
			PixelLayer("Corner",
				Psd::Box(4, 4, 8, 8),
				[](const int X, const int Y)
				{
					return Psd::Rgba{static_cast<uint8_t>(X * 20), static_cast<uint8_t>(Y * 20), 0, 255};
				})
		}));
	Files.push_back(Finish(Compressed));

	return Files;
}

void Mutate(std::vector<uint8_t>& Bytes, const size_t MutableEnd, Random& Rng)
{
	static constexpr std::array<uint32_t, 15> AWKWARD = {0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE};

	const size_t Changes = 1 + Rng.Below(4);
	for(size_t Change = 0; Change < Changes; ++Change)
	{
		const size_t At = Rng.Below(MutableEnd);
		switch(Rng.Below(5))
		{
			case 0 :
				Bytes[At] = static_cast<uint8_t>(Rng.Next());
				break;
			case 1 :
				Bytes[At] ^= static_cast<uint8_t>(1u << Rng.Below(8));
				break;
			case 2 :
				Bytes[At] = 0;
				break;
			case 3 :
				Bytes[At] = 0xFF;
				break;
			default :
			{
				// Replace a whole big-endian field, a length or a rectangle edge, say
				const size_t Start = std::min(At, Bytes.size() - 4);
				const uint32_t Value = Rng.Below(4) == 0 ? static_cast<uint32_t>(Bytes.size() + Rng.Below(3) - 1) : AWKWARD[Rng.Below(AWKWARD.size())];
				for(size_t Byte = 0; Byte < 4; ++Byte)
				{
					Bytes[Start + Byte] = static_cast<uint8_t>(Value >> (24 - 8 * Byte));
				}
				break;
			}
		}
	}
}

// What every layer that did load has to look like, whatever the file said. Adding the bytes up makes sure the whole buffer is
// really readable, which AddressSanitizer watches.
std::string Problem(const WatchedFileImageLibrary& Library)
{
	static volatile unsigned Sink = 0;

	const FileImageLibrary* Layers = Library.Get();
	if(Layers == nullptr)
	{
		return {};
	}

	for(const auto& [Name, Layer] : *Layers)
	{
		const gs_image_file_ex* Image = Layer.Image.get();
		if(Image == nullptr || Image->texture_data == nullptr || Image->cx == 0 || Image->cy == 0)
		{
			return "layer '" + Name + "' has no image";
		}
		if(Image->mem_usage != static_cast<uint64_t>(Image->cx) * Image->cy * 4)
		{
			return "layer '" + Name + "' has a buffer size that doesn't match its dimensions";
		}
		if(Layer.Offset.X < 0 || Layer.Offset.Y < 0)
		{
			return "layer '" + Name + "' starts outside the canvas";
		}
		if(Layer.Offset.X + static_cast<float>(Image->cx) > Library.GetMaxResolution().X || Layer.Offset.Y + static_cast<float>(Image->cy) > Library.GetMaxResolution().Y)
		{
			return "layer '" + Name + "' reaches past the library's size";
		}

		unsigned Sum = 0;
		for(uint64_t Byte = 0; Byte < Image->mem_usage; ++Byte)
		{
			Sum += Image->texture_data[Byte];
		}
		Sink = Sink + Sum;
	}

	return {};
}
}

TEST(PsdFuzz, MutatedFiles_LoadWithoutCrashing)
{
	if(IS_DEBUG_BUILD)
	{
		GTEST_SKIP() << "psd_sdk asserts on corrupt input in Debug builds";
	}

	const uint64_t Seed = EnvNumber("PSD_FUZZ_SEED", 0xC0FFEE);
	const uint64_t Iterations = EnvNumber("PSD_FUZZ_ITERATIONS", 300);
	if(Iterations == 0)
	{
		GTEST_SKIP() << "PSD_FUZZ_ITERATIONS is 0";
	}

	const std::vector<StartingFile> Starts = StartingFiles();

	// The file being loaded is always the same one, so after a crash it is the file that did it
	const TempDir Dir;
	const std::filesystem::path Path = Dir.File("Fuzz.psd");
	printf("PSD fuzz: seed %llu, %llu files for each of %zu documents, the current one is %s\n",
		static_cast<unsigned long long>(Seed),
		static_cast<unsigned long long>(Iterations),
		Starts.size(),
		Path.string().c_str());
	static_cast<void>(fflush(stdout));

	Watchdog Guard(Path, std::chrono::seconds(EnvNumber("PSD_FUZZ_TIMEOUT", 30)), Seed);
	Random Rng(Seed);
	for(uint64_t Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		for(size_t Start = 0; Start < Starts.size(); ++Start)
		{
			std::vector<uint8_t> Bytes = Starts[Start].Bytes;
			if(Iteration > 0)
			{
				Mutate(Bytes, Starts[Start].MutableEnd, Rng);
			}

			WriteBytes(Path, Bytes);
			ObsStubs::Reset();
			Guard.Begin(Iteration, Start);
			const WatchedFileImageLibrary Library = LoadQuietly(Path);

			// The untouched files go first, and have to load
			if(Iteration == 0)
			{
				EXPECT_NE(Library.Get(), nullptr) << "document " << Start << " does not load unchanged";
			}

			const std::string Found = Problem(Library);
			Guard.End();
			if(!Found.empty())
			{
				const std::filesystem::path Kept = std::filesystem::temp_directory_path() / ("psd-fuzz-failure-" + std::to_string(Seed) + "-" + std::to_string(Iteration) + "-" + std::to_string(Start)
					+ ".psd");
				WriteBytes(Kept, Bytes);
				ADD_FAILURE() << Found << "; document " << Start << ", file " << Iteration << ", seed " << Seed << ", kept as " << Kept.string();
			}
		}
	}
}

// Loads the file named by PSD_FUZZ_REPLAY, to look at one the fuzzer found
TEST(PsdFuzz, ReplayFile)
{
	const std::string Replay = EnvText("PSD_FUZZ_REPLAY");
	if(Replay.empty())
	{
		GTEST_SKIP() << "set PSD_FUZZ_REPLAY to the path of a file to load";
	}

	const WatchedFileImageLibrary Library{std::filesystem::path(Replay)};

	EXPECT_EQ(Problem(Library), "");
}
