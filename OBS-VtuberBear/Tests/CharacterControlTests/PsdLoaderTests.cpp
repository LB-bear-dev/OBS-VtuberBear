// The PSD loader in WatchedImage.cpp, run through WatchedFileImageLibrary like the plugin does. Each test builds a PSD with
// PsdBuilder.h, loads it, and looks at the layers that came out: their names, positions and pixels. The pixels are RGBA with
// the alpha already multiplied in, which is what ObsStubs.h keeps instead of handing them to the GPU.
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <memory>
#include "ObsStubs.h"
#include "PCH.h"
#include "PsdBuilder.h"
#include "TestSupport.h"
#include "WatchedImage.h"

using namespace CharacterControlRender;
using namespace CharacterControlTests;
namespace Psd = PsdBuilder;

namespace
{
using Pixel = std::array<int, 4>;

class PsdLoaderTest : public testing::Test
{
protected:
	void SetUp() override
	{
		ObsStubs::Reset();
	}

	std::unique_ptr<WatchedFileImageLibrary> LoadBytes(const std::vector<uint8_t>& Bytes, const std::string& FileName = "Avatar.psd") const
	{
		const auto Path = Dir.File(FileName);
		WriteBytes(Path, Bytes);
		const QuietStdout Quiet;
		return std::make_unique<WatchedFileImageLibrary>(Path);
	}

	std::unique_ptr<WatchedFileImageLibrary> Load(const Psd::Document& Document) const
	{
		return LoadBytes(Psd::Build(Document));
	}

	static const FileLayer* Find(const WatchedFileImageLibrary& Library, const std::string& Name)
	{
		const FileImageLibrary* Layers = Library.Get();
		if(Layers == nullptr)
		{
			return nullptr;
		}

		const auto Found = Layers->find(Name);
		return Found == Layers->end() ? nullptr : &Found->second;
	}

	static Pixel PixelAt(const FileLayer& Layer, const unsigned X, const unsigned Y)
	{
		const uint8_t* Data = Layer.Image->texture_data + (static_cast<size_t>(Y) * Layer.Image->cx + X) * 4;
		return {Data[0], Data[1], Data[2], Data[3]};
	}

	static std::vector<std::string> NamesOf(const WatchedFileImageLibrary& Library)
	{
		std::vector<std::string> Names = Library.GetImageNames();
		std::ranges::sort(Names);
		return Names;
	}

	TempDir Dir;
};

// A pixel whose red channel says where in the layer it came from, so a crop that picks the wrong pixels shows
Psd::Layer MarkedLayer(const std::string& Name, const Psd::Rect Bounds)
{
	return Psd::PixelLayer(Name,
		Bounds,
		[](const int X, const int Y)
		{
			return Psd::Rgba{static_cast<uint8_t>(16 * Y + X), 0, 0, 255};
		});
}

using Names = std::vector<std::string>;
}

TEST_F(PsdLoaderTest, OpaqueLayer_LoadsAtItsPosition)
{
	Psd::Document Document(16, 16);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(2, 3, 4, 5), 200, 100, 50));

	const auto Library = Load(Document);

	ASSERT_NE(Library->Get(), nullptr);
	const FileLayer* Body = Find(*Library, "Body");
	ASSERT_NE(Body, nullptr);
	EXPECT_EQ(Body->Image->cx, 4u);
	EXPECT_EQ(Body->Image->cy, 5u);
	EXPECT_EQ(Body->Offset.X, 2.0f);
	EXPECT_EQ(Body->Offset.Y, 3.0f);
	EXPECT_EQ(PixelAt(*Body, 0, 0), (Pixel{200, 100, 50, 255}));
	EXPECT_EQ(PixelAt(*Body, 3, 4), (Pixel{200, 100, 50, 255}));
	EXPECT_EQ(ObsStubs::TextureUploads(), 1);
}

TEST_F(PsdLoaderTest, MaxResolution_IsTheCanvas)
{
	Psd::Document Document(24, 12);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(1, 1, 4, 4), 1, 2, 3));

	const auto Library = Load(Document);

	EXPECT_EQ(Library->GetMaxResolution().X, 24.0f);
	EXPECT_EQ(Library->GetMaxResolution().Y, 12.0f);
}

TEST_F(PsdLoaderTest, ZOrder_FollowsTheOrderInTheFile)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Back", Psd::Box(0, 0, 2, 2), 1, 1, 1));
	Document.Add(Psd::SolidLayer("Middle", Psd::Box(0, 0, 2, 2), 2, 2, 2));
	Document.Add(Psd::SolidLayer("Front", Psd::Box(0, 0, 2, 2), 3, 3, 3));

	const auto Library = Load(Document);

	ASSERT_EQ(NamesOf(*Library), (Names{"Back", "Front", "Middle"}));
	EXPECT_EQ(Find(*Library, "Back")->ZOrder, 0);
	EXPECT_EQ(Find(*Library, "Middle")->ZOrder, 1);
	EXPECT_EQ(Find(*Library, "Front")->ZOrder, 2);
}

TEST_F(PsdLoaderTest, PixelAlpha_IsMultipliedIntoTheColor)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Glass", Psd::Box(0, 0, 2, 2), 200, 100, 50, 128));

	const auto Library = Load(Document);

	ASSERT_NE(Find(*Library, "Glass"), nullptr);
	EXPECT_EQ(PixelAt(*Find(*Library, "Glass"), 0, 0), (Pixel{100, 50, 25, 128}));
}

TEST_F(PsdLoaderTest, LayerOpacity_IsBakedIntoAlpha)
{
	Psd::Document Document(8, 8);
	Psd::Layer Faded = Psd::SolidLayer("Faded", Psd::Box(0, 0, 2, 2), 200, 100, 50);
	Faded.Opacity = 128;
	Document.Add(Faded);

	const auto Library = Load(Document);

	ASSERT_NE(Find(*Library, "Faded"), nullptr);
	EXPECT_EQ(PixelAt(*Find(*Library, "Faded"), 0, 0), (Pixel{100, 50, 25, 128}));
}

TEST_F(PsdLoaderTest, GroupOpacity_MultipliesWithTheLayersOwn)
{
	Psd::Document Document(8, 8);
	Psd::Layer Inner = Psd::SolidLayer("Inner", Psd::Box(0, 0, 2, 2), 255, 255, 255);
	Inner.Opacity = 128;
	Document.Add(Psd::Group("Faded", 128, {Inner}));

	const auto Library = Load(Document);

	// 128/255 of 128/255 is 64/255
	ASSERT_NE(Find(*Library, "Faded/Inner"), nullptr);
	EXPECT_EQ(PixelAt(*Find(*Library, "Faded/Inner"), 0, 0), (Pixel{64, 64, 64, 64}));
}

TEST_F(PsdLoaderTest, GroupedLayers_AreNamedByTheirPath)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));
	Document.Add(Psd::Group("Head", 255, Psd::Group("Mouth", 255, {Psd::SolidLayer("Open", Psd::Box(0, 0, 2, 2), 2, 2, 2)})));

	const auto Library = Load(Document);

	// The groups themselves, and their dividers, are not images
	EXPECT_EQ(NamesOf(*Library), (Names{"Body", "Head/Mouth/Open"}));
}

TEST_F(PsdLoaderTest, SameNameInDifferentGroups_StaysDistinct)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::Group("Head", 255, {Psd::SolidLayer("Eyes", Psd::Box(0, 0, 2, 2), 10, 0, 0)}));
	Document.Add(Psd::Group("Doll", 255, {Psd::SolidLayer("Eyes", Psd::Box(0, 0, 2, 2), 20, 0, 0)}));

	const auto Library = Load(Document);

	ASSERT_EQ(NamesOf(*Library), (Names{"Doll/Eyes", "Head/Eyes"}));
	EXPECT_EQ(PixelAt(*Find(*Library, "Head/Eyes"), 0, 0)[0], 10);
	EXPECT_EQ(PixelAt(*Find(*Library, "Doll/Eyes"), 0, 0)[0], 20);
}

TEST_F(PsdLoaderTest, SamePathTwice_KeepsTheFirstAndWarns)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Mouth", Psd::Box(0, 0, 2, 2), 10, 0, 0));
	Document.Add(Psd::SolidLayer("Mouth", Psd::Box(0, 0, 2, 2), 20, 0, 0));

	const auto Library = Load(Document);

	ASSERT_EQ(NamesOf(*Library), (Names{"Mouth"}));
	EXPECT_EQ(PixelAt(*Find(*Library, "Mouth"), 0, 0)[0], 10);
	EXPECT_TRUE(ObsStubs::LogContains("more than one layer is named 'Mouth'"));
}

TEST_F(PsdLoaderTest, UnicodeLayerName_BecomesUtf8)
{
	Psd::Document Document(8, 8);
	Psd::Layer Named = Psd::SolidLayer("Open", Psd::Box(0, 0, 2, 2), 1, 1, 1);
	// U+53E3 is spelled as a number: a \u escape is easily turned into the character itself on the way into a source file,
	// and then the compiler reads it in the wrong code page
	Named.UnicodeName = {0x53E3, u' ', u'O', u'p', u'e', u'n'};
	Document.Add(Named);

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"\xE5\x8F\xA3 Open"}));
}

TEST_F(PsdLoaderTest, TransparentBorders_AreTrimmed)
{
	Psd::Document Document(16, 16);
	Document.Add(Psd::PixelLayer("Dot",
		Psd::Box(1, 1, 8, 8),
		[](const int X, const int Y)
		{
			const bool Inside = X >= 3 && X < 5 && Y >= 2 && Y < 4;
			return Psd::Rgba{255, 0, 0, static_cast<uint8_t>(Inside ? 255 : 0)};
		}));

	const auto Library = Load(Document);

	const FileLayer* Dot = Find(*Library, "Dot");
	ASSERT_NE(Dot, nullptr);
	EXPECT_EQ(Dot->Image->cx, 2u);
	EXPECT_EQ(Dot->Image->cy, 2u);
	// The layer starts at 1,1 and the visible part 3,2 further in
	EXPECT_EQ(Dot->Offset.X, 4.0f);
	EXPECT_EQ(Dot->Offset.Y, 3.0f);
	EXPECT_EQ(PixelAt(*Dot, 1, 1), (Pixel{255, 0, 0, 255}));
}

TEST_F(PsdLoaderTest, FullyTransparentLayer_IsLeftOut)
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Ghost", Psd::Box(0, 0, 4, 4), 255, 255, 255, 0));
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 4, 4), 255, 255, 255));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, LayerWithoutAlphaChannel_IsOpaque)
{
	Psd::Document Document(8, 8);
	Psd::Layer Flat = Psd::SolidLayer("Flat", Psd::Box(0, 0, 2, 2), 9, 8, 7);
	Flat.Channels.erase(Flat.Channels.begin()); // the alpha channel comes first
	Document.Add(Flat);

	const auto Library = Load(Document);

	ASSERT_NE(Find(*Library, "Flat"), nullptr);
	EXPECT_EQ(PixelAt(*Find(*Library, "Flat"), 0, 0), (Pixel{9, 8, 7, 255}));
}

TEST_F(PsdLoaderTest, LayerWithOnlyAlpha_IsBlack)
{
	Psd::Document Document(8, 8);
	Psd::Layer Shadow = Psd::SolidLayer("Shadow", Psd::Box(0, 0, 2, 2), 9, 8, 7);
	Shadow.Channels.resize(1); // keep the alpha channel
	Document.Add(Shadow);

	const auto Library = Load(Document);

	ASSERT_NE(Find(*Library, "Shadow"), nullptr);
	EXPECT_EQ(PixelAt(*Find(*Library, "Shadow"), 0, 0), (Pixel{0, 0, 0, 255}));
}

TEST_F(PsdLoaderTest, LayerHangingOffTheCanvas_IsCroppedToIt)
{
	Psd::Document Document(4, 4);
	Document.Add(MarkedLayer("Big", Psd::Box(-2, -2, 8, 8)));

	const auto Library = Load(Document);

	const FileLayer* Big = Find(*Library, "Big");
	ASSERT_NE(Big, nullptr);
	EXPECT_EQ(Big->Image->cx, 4u);
	EXPECT_EQ(Big->Image->cy, 4u);
	EXPECT_EQ(Big->Offset.X, 0.0f);
	EXPECT_EQ(Big->Offset.Y, 0.0f);
	// Canvas 0,0 is the layer's 2,2
	EXPECT_EQ(PixelAt(*Big, 0, 0)[0], 16 * 2 + 2);
	EXPECT_EQ(PixelAt(*Big, 3, 1)[0], 16 * 3 + 5);
}

TEST_F(PsdLoaderTest, LayerPastTheRightAndBottomEdge_KeepsTheVisiblePart)
{
	Psd::Document Document(4, 4);
	Document.Add(MarkedLayer("Corner", Psd::Box(2, 2, 8, 8)));

	const auto Library = Load(Document);

	const FileLayer* Corner = Find(*Library, "Corner");
	ASSERT_NE(Corner, nullptr);
	EXPECT_EQ(Corner->Image->cx, 2u);
	EXPECT_EQ(Corner->Image->cy, 2u);
	EXPECT_EQ(Corner->Offset.X, 2.0f);
	EXPECT_EQ(Corner->Offset.Y, 2.0f);
	EXPECT_EQ(PixelAt(*Corner, 0, 0)[0], 0);
	EXPECT_EQ(PixelAt(*Corner, 1, 1)[0], 16 + 1);
}

TEST_F(PsdLoaderTest, LayerEntirelyOffTheCanvas_IsLeftOut)
{
	Psd::Document Document(4, 4);
	Document.Add(Psd::SolidLayer("Right", Psd::Box(10, 0, 4, 4), 1, 1, 1));
	Document.Add(Psd::SolidLayer("Above", Psd::Box(0, -10, 4, 4), 1, 1, 1));
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 4, 4), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, LayerMask_HidesWhatItCovers)
{
	Psd::Document Document(8, 8);
	Psd::Layer Masked = Psd::SolidLayer("Masked", Psd::Box(0, 0, 4, 4), 255, 255, 255);
	Psd::Mask Mask;
	Mask.Bounds = Psd::Box(0, 0, 4, 4);
	for(int Y = 0; Y < 4; ++Y)
	{
		for(int X = 0; X < 4; ++X)
		{
			Mask.Pixels.push_back(X < 2 ? 0 : 255);
		}
	}
	Masked.LayerMask = Mask;
	Document.Add(Masked);

	const auto Library = Load(Document);

	// The hidden left half is trimmed away
	const FileLayer* Found = Find(*Library, "Masked");
	ASSERT_NE(Found, nullptr);
	EXPECT_EQ(Found->Image->cx, 2u);
	EXPECT_EQ(Found->Image->cy, 4u);
	EXPECT_EQ(Found->Offset.X, 2.0f);
	EXPECT_EQ(PixelAt(*Found, 0, 0), (Pixel{255, 255, 255, 255}));
}

TEST_F(PsdLoaderTest, LayerMask_HidesEverythingOutsideItsRectangleWhenItsDefaultIsBlack)
{
	Psd::Document Document(8, 8);
	Psd::Layer Masked = Psd::SolidLayer("Masked", Psd::Box(0, 0, 4, 4), 255, 255, 255);
	Psd::Mask Mask;
	Mask.Bounds = Psd::Box(2, 0, 2, 4);
	Mask.DefaultColor = 0;
	Mask.Pixels.assign(8, 255); // 2 x 4
	Masked.LayerMask = Mask;
	Document.Add(Masked);

	const auto Library = Load(Document);

	const FileLayer* Found = Find(*Library, "Masked");
	ASSERT_NE(Found, nullptr);
	EXPECT_EQ(Found->Image->cx, 2u);
	EXPECT_EQ(Found->Offset.X, 2.0f);
}

TEST_F(PsdLoaderTest, LayerMask_ShowsEverythingOutsideItsRectangleWhenItsDefaultIsWhite)
{
	Psd::Document Document(8, 8);
	Psd::Layer Masked = Psd::SolidLayer("Masked", Psd::Box(0, 0, 4, 4), 255, 255, 255);
	Psd::Mask Mask;
	Mask.Bounds = Psd::Box(0, 0, 2, 4);
	Mask.DefaultColor = 255;
	Mask.Pixels.assign(8, 0); // 2 x 4
	Masked.LayerMask = Mask;
	Document.Add(Masked);

	const auto Library = Load(Document);

	const FileLayer* Found = Find(*Library, "Masked");
	ASSERT_NE(Found, nullptr);
	EXPECT_EQ(Found->Image->cx, 2u);
	EXPECT_EQ(Found->Offset.X, 2.0f);
}

class PsdCompressionTest : public PsdLoaderTest, public testing::WithParamInterface<Psd::Compression> {};

TEST_P(PsdCompressionTest, DecodesTheSamePixels)
{
	Psd::Document Document(8, 4);
	Document.Add(Psd::PixelLayer("Stripes",
		Psd::Box(0, 0, 8, 4),
		[](const int X, const int Y)
		{
			return Psd::Rgba{static_cast<uint8_t>(X < 5 ? 10 : 200 + Y), static_cast<uint8_t>(X * 10), 7, 255};
		},
		GetParam()));

	const auto Library = Load(Document);

	const FileLayer* Stripes = Find(*Library, "Stripes");
	ASSERT_NE(Stripes, nullptr);
	for(unsigned Y = 0; Y < 4; ++Y)
	{
		for(unsigned X = 0; X < 8; ++X)
		{
			EXPECT_EQ(PixelAt(*Stripes, X, Y), (Pixel{static_cast<int>(X < 5 ? 10 : 200 + Y), static_cast<int>(X * 10), 7, 255})) << "at " << X << "," << Y;
		}
	}
}

const char* CompressionName(const testing::TestParamInfo<Psd::Compression>& Info)
{
	switch(Info.param)
	{
		case Psd::Raw :
			return "Raw";
		case Psd::Rle :
			return "Rle";
		case Psd::Zip :
			return "Zip";
		default :
			return "ZipPrediction";
	}
}

INSTANTIATE_TEST_SUITE_P(Codecs, PsdCompressionTest, testing::Values(Psd::Raw, Psd::Rle, Psd::Zip, Psd::ZipPrediction), CompressionName);

// Files the loader turns away as a whole

TEST_F(PsdLoaderTest, MissingFile_LoadsNothing)
{
	const WatchedFileImageLibrary Library(Dir.File("Missing.psd"));

	EXPECT_EQ(Library.Get(), nullptr);
	EXPECT_TRUE(Library.GetImageNames().empty());
}

TEST_F(PsdLoaderTest, NotAPsd_LoadsNothing)
{
	const auto Library = LoadBytes({'n', 'o', 't', ' ', 'a', ' ', 'p', 's', 'd'});

	EXPECT_EQ(Library->Get(), nullptr);
	EXPECT_TRUE(ObsStubs::LogContains("Cannot create document"));
}

TEST_F(PsdLoaderTest, EmptyFile_LoadsNothing)
{
	const auto Library = LoadBytes({});

	EXPECT_EQ(Library->Get(), nullptr);
}

TEST_F(PsdLoaderTest, SixteenBitDocument_IsRefused)
{
	Psd::Document Document(8, 8);
	Document.Depth = 16;
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(Library->Get(), nullptr);
	EXPECT_TRUE(ObsStubs::LogContains("only 8-bit RGB PSDs are supported"));
}

TEST_F(PsdLoaderTest, CmykDocument_IsRefused)
{
	Psd::Document Document(8, 8);
	Document.ColorMode = 4;
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(Library->Get(), nullptr);
	EXPECT_TRUE(ObsStubs::LogContains("only 8-bit RGB PSDs are supported"));
}

TEST_F(PsdLoaderTest, CanvasBeyondTheLimit_IsRefused)
{
	Psd::Document Document(16385, 8);
	Document.CompositeImage = false;

	const auto Library = Load(Document);

	EXPECT_EQ(Library->Get(), nullptr);
	EXPECT_TRUE(ObsStubs::LogContains("the largest supported is 16384x16384"));
}

TEST_F(PsdLoaderTest, CanvasAtTheLimit_IsAccepted)
{
	Psd::Document Document(16384, 1);
	Document.CompositeImage = false;
	Document.Add(Psd::SolidLayer("Strip", Psd::Box(0, 0, 16384, 1), 1, 2, 3));

	const auto Library = Load(Document);

	ASSERT_NE(Library->Get(), nullptr);
	EXPECT_EQ(NamesOf(*Library), (Names{"Strip"}));
}

TEST_F(PsdLoaderTest, EmptyCanvas_IsRefused)
{
	Psd::Document Document(0, 8);
	Document.CompositeImage = false;

	const auto Library = Load(Document);

	EXPECT_EQ(Library->Get(), nullptr);
}

namespace
{
// The flattened image the builder puts after the layers: a compression word and three bytes per pixel of the 8 x 8 canvas
constexpr size_t COMPOSITE_IMAGE_SIZE = 2 + 8 * 8 * 3;

std::vector<uint8_t> TwoLayerFile()
{
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 4, 4), 1, 2, 3));
	Document.Add(Psd::Group("Head", 255, {Psd::SolidLayer("Eyes", Psd::Box(0, 0, 2, 2), 4, 5, 6)}));
	return Psd::Build(Document);
}
}

// A file cut off anywhere before the end of its layers is turned away whole. psd_sdk would answer a layer record it can't read by
// returning the layers it reached with the rest uninitialised, and then index its fixed-size group stack with their garbage,
// which crashed OBS in Release for one cut length and trips an assert in Debug for nearly all of them. This is also what a file
// looks like while Photoshop is still saving it.
TEST_F(PsdLoaderTest, FileCutOffInsideTheLayers_LoadsNothing)
{
	const std::vector<uint8_t> Whole = TwoLayerFile();
	const size_t LayersEnd = Whole.size() - COMPOSITE_IMAGE_SIZE;

	for(size_t Length = 0; Length < LayersEnd; ++Length)
	{
		const auto Library = LoadBytes(std::vector<uint8_t>(Whole.begin(), Whole.begin() + static_cast<std::ptrdiff_t>(Length)));

		EXPECT_EQ(Library->Get(), nullptr) << "cut at " << Length;
	}
}

// The loader never reads the flattened image, so a file that is only missing part of that is as good as whole
TEST_F(PsdLoaderTest, FileCutOffAfterTheLayers_StillLoadsThem)
{
	const std::vector<uint8_t> Whole = TwoLayerFile();

	for(size_t Length = Whole.size() - COMPOSITE_IMAGE_SIZE; Length < Whole.size(); ++Length)
	{
		const auto Library = LoadBytes(std::vector<uint8_t>(Whole.begin(), Whole.begin() + static_cast<std::ptrdiff_t>(Length)));

		EXPECT_EQ(NamesOf(*Library), (Names{"Body", "Head/Eyes"})) << "cut at " << Length;
	}
}

// Files whose layers claim more than the file holds. The loader skips those layers and keeps the rest.

TEST_F(PsdLoaderTest, LayerBiggerThanItsData_IsSkipped)
{
	Psd::Document Document(8, 8);
	Psd::Layer Liar;
	Liar.Name = "Liar";
	Liar.Bounds = Psd::Box(0, 0, 1000, 1000);
	for(const int16_t Id : {Psd::Alpha, Psd::Red, Psd::Green, Psd::Blue})
	{
		Liar.Channels.push_back(Psd::MakeChannel(Id, std::vector<uint8_t>(10, 255), 10));
	}
	Document.Add(Liar);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
	EXPECT_TRUE(ObsStubs::LogContains("its size does not fit the file"));
}

TEST_F(PsdLoaderTest, LayerBiggerThanTheLimit_IsSkippedEvenWithDataToMatch)
{
	// 20000 x 20000 pixels is more than the loader will allocate, and 400000 bytes per channel makes it pass the compression check
	Psd::Document Document(8, 8);
	Psd::Layer Huge;
	Huge.Name = "Huge";
	Huge.Bounds = Psd::Box(0, 0, 20000, 20000);
	for(const int16_t Id : {Psd::Alpha, Psd::Red, Psd::Green, Psd::Blue})
	{
		Huge.Channels.push_back(Psd::MakeChannel(Id, std::vector<uint8_t>(400000, 0), 20000));
	}
	Document.Add(Huge);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
	EXPECT_TRUE(ObsStubs::LogContains("its size does not fit the file"));
}

TEST_F(PsdLoaderTest, UpsideDownRectangle_IsSkipped)
{
	Psd::Document Document(8, 8);
	Psd::Layer Flipped = Psd::SolidLayer("Flipped", Psd::Box(0, 0, 4, 4), 1, 1, 1);
	Flipped.Bounds = {5, 5, 1, 1};
	Document.Add(Flipped);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, RectangleSpanningBillionsOfPixels_IsSkipped)
{
	Psd::Document Document(8, 8);
	Psd::Layer Vast = Psd::SolidLayer("Vast", Psd::Box(0, 0, 4, 4), 1, 1, 1);
	Vast.Bounds = {-1'000'000'000, -1'000'000'000, 1'000'000'000, 1'000'000'000};
	Document.Add(Vast);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, MaskBiggerThanItsData_IsSkipped)
{
	Psd::Document Document(8, 8);
	Psd::Layer Masked = Psd::SolidLayer("Masked", Psd::Box(0, 0, 2, 2), 1, 1, 1);
	Psd::Mask Mask;
	Mask.Bounds = Psd::Box(0, 0, 100000, 100000);
	Masked.LayerMask = Mask;
	Document.Add(Masked);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, ChannelShorterThanItsCompressionWord_IsSkipped)
{
	// A channel's size counts its 2-byte compression word, so psd_sdk subtracts 2 from it to size the compressed data, which wraps
	// around for a size of 0 or 1. The liar goes last so that the size it declares doesn't shift the data of any layer after it.
	for(const uint32_t DeclaredSize : {0u, 1u})
	{
		SCOPED_TRACE(DeclaredSize);
		ObsStubs::Reset();

		Psd::Document Document(8, 8);
		Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));
		Psd::Layer Liar = Psd::PixelLayer("Liar",
			Psd::Box(0, 0, 2, 2),
			[](int, int)
			{
				return Psd::Rgba{9, 9, 9, 255};
			},
			Psd::Zip);
		Liar.Channels.back().DeclaredSize = DeclaredSize;
		Document.Add(Liar);

		const auto Library = Load(Document);

		EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
		EXPECT_TRUE(ObsStubs::LogContains("its size does not fit the file"));
	}
}

TEST_F(PsdLoaderTest, LayerMaskChannelWithoutAMask_IsSkipped)
{
	// LAYER_MASK is the channel type for a layer mask when there is also a vector mask. Without a layer mask to hold it, psd_sdk
	// would look up the mask's size through a null pointer.
	Psd::Document Document(8, 8);
	Psd::Layer Orphan = Psd::SolidLayer("Orphan", Psd::Box(0, 0, 2, 2), 9, 9, 9);
	Orphan.Channels.push_back(Psd::MakeChannel(-3, std::vector<uint8_t>(4, 255), 2));
	Document.Add(Orphan);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
	EXPECT_TRUE(ObsStubs::LogContains("its size does not fit the file"));
}

TEST_F(PsdLoaderTest, MaskChannelWithoutAMask_IsNotTakenForPicture)
{
	if(IS_DEBUG_BUILD)
	{
		GTEST_SKIP() << "psd_sdk asserts on a mask channel with no mask in Debug builds; the loader's handling of it is for Release";
	}

	// The channel is left in the layer where it was, sized as a mask when no mask exists, and was then copied in as the alpha
	Psd::Document Document(8, 8);
	Psd::Layer Stray = Psd::SolidLayer("Stray", Psd::Box(0, 0, 2, 2), 1, 2, 3);
	Stray.Channels.push_back(Psd::MakeChannel(Psd::LayerMaskData, std::vector<uint8_t>(4, 0), 2));
	Document.Add(Stray);

	const auto Library = Load(Document);

	const FileLayer* Found = Find(*Library, "Stray");
	ASSERT_NE(Found, nullptr);
	EXPECT_EQ(PixelAt(*Found, 1, 1), (Pixel{1, 2, 3, 255}));
}

TEST_F(PsdLoaderTest, ChannelOfAnUnknownKind_IsNotTakenForPicture)
{
	// Types below the masks aren't defined, and aren't the alpha either: this one would have replaced it with its zeros
	Psd::Document Document(8, 8);
	Psd::Layer Stray = Psd::SolidLayer("Stray", Psd::Box(0, 0, 2, 2), 1, 2, 3);
	Stray.Channels.push_back(Psd::MakeChannel(-4, std::vector<uint8_t>(4, 0), 2));
	Document.Add(Stray);

	const auto Library = Load(Document);

	const FileLayer* Found = Find(*Library, "Stray");
	ASSERT_NE(Found, nullptr);
	EXPECT_EQ(PixelAt(*Found, 1, 1), (Pixel{1, 2, 3, 255}));
}

TEST_F(PsdLoaderTest, UnknownCompression_SkipsTheLayer)
{
	if(IS_DEBUG_BUILD)
	{
		GTEST_SKIP() << "psd_sdk asserts on a compression type it doesn't know in Debug builds; the loader's check is for Release";
	}

	Psd::Document Document(8, 8);
	Psd::Layer Strange = Psd::SolidLayer("Strange", Psd::Box(0, 0, 2, 2), 1, 1, 1);
	for(Psd::Channel& Channel : Strange.Channels)
	{
		Channel.CompressionWord = 7;
	}
	Document.Add(Strange);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));

	const auto Library = Load(Document);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
	EXPECT_TRUE(ObsStubs::LogContains("one of its channels could not be decoded"));
}

// Bugs in psd_sdk itself that the loader cannot check for before handing it the file, found by PsdFuzzTests. Upstream still has
// them; ThirdParty\patches\psd_sdk.patch fixes them, and scripts\setup-deps.ps1 applies it. Without the patch each of these
// crashes, hangs or corrupts memory (only AddressSanitizer sees the corruption).

namespace
{
// Where the layer count is: after the 26-byte header, the empty color mode data and image resources (4 bytes each) and the two
// section lengths
constexpr size_t LAYER_COUNT_OFFSET = 26 + 4 + 4 + 4 + 4;
}

TEST_F(PsdLoaderTest, SdkLayerCountOfMinus32768_IsRefused)
{
	if(IS_DEBUG_BUILD)
	{
		GTEST_SKIP() << "psd_sdk asserts on the layer data it then misreads in Debug builds; the fix is for Release";
	}

	// A negative count means "the first layer holds the alpha for the merged image" and is negated; -32768 has no positive 16-bit
	// value, so psd_sdk goes on to allocate for over 2^63 layers
	std::vector<uint8_t> Bytes = Psd::Build([]
	{
		Psd::Document Document(8, 8);
		Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));
		return Document;
	}());
	Bytes[LAYER_COUNT_OFFSET] = 0x80;
	Bytes[LAYER_COUNT_OFFSET + 1] = 0x00;

	const auto Library = LoadBytes(Bytes);

	EXPECT_TRUE(NamesOf(*Library).empty());
}

TEST_F(PsdLoaderTest, SdkUnicodeNameLongerThanItsBlock_IsRefused)
{
	// The count of characters in a 'luni' block is not checked against the block, so the name loop spins through billions of
	// characters read from beyond it. A file of a kilobyte or so keeps the loader busy for minutes.
	Psd::Document Document(8, 8);
	Psd::Layer Named = Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1);
	Named.UnicodeName = u"Body";
	Document.Add(Named);
	std::vector<uint8_t> Bytes = Psd::Build(Document);

	const std::string Key = "luni";
	const auto Block = std::search(Bytes.begin(), Bytes.end(), Key.begin(), Key.end());
	ASSERT_NE(Block, Bytes.end());
	// The count follows the key and the block's length
	const std::array<uint8_t, 4> HugeCount = {0xFF, 0x00, 0x00, 0x03};
	std::ranges::copy(HugeCount, Block + 8);

	const auto Library = LoadBytes(Bytes);

	EXPECT_EQ(NamesOf(*Library), (Names{"Body"}));
}

TEST_F(PsdLoaderTest, SdkGroupsNestedDeeperThanItsStack_IsRefused)
{
	// The hierarchy is built with a fixed stack of 256 entries, which only an assert guards, so a file with 300 folders that
	// are never closed writes past the end of it. A divider with no folder to close pops below the start of it just the same.
	Psd::Document Document(8, 8);
	Document.Add(Psd::SolidLayer("Body", Psd::Box(0, 0, 2, 2), 1, 1, 1));
	for(int Index = 0; Index < 300; ++Index)
	{
		Psd::Layer Folder;
		Folder.Name = "Folder";
		Folder.Section = Psd::OpenFolder;
		Folder.Channels = {Psd::Channel{Psd::Alpha}, Psd::Channel{Psd::Red}, Psd::Channel{Psd::Green}, Psd::Channel{Psd::Blue}};
		Document.Add(Folder);
	}

	// Which layers come out of a file like this is anyone's guess; what matters is that nothing outside the stack is touched
	const auto Library = LoadBytes(Psd::Build(Document));

	EXPECT_NE(Library->Get(), nullptr);
}

TEST_F(PsdLoaderTest, SdkRunLongerThanItsRow_StopsAtTheEndOfTheRow)
{
	// An RLE run is copied for its whole stated length whatever the row has room for, so a run of 128 bytes in a row of 2 writes
	// 126 bytes past the end of the channel's buffer. Only AddressSanitizer notices. The run is cut off at the end of the row, so
	// the layer loads; what its pixels hold is of no interest.
	Psd::Document Document(8, 8);
	Psd::Layer Overrun = Psd::SolidLayer("Overrun", Psd::Box(0, 0, 2, 2), 1, 1, 1);
	Psd::Channel& Alpha = Overrun.Channels.front();
	Alpha.CompressionWord = Psd::Rle;
	Alpha.Data = {0, 2, 0, 2, 0x81, 0xAA, 0x81, 0xAA}; // two rows of two bytes, each a run of 128
	Document.Add(Overrun);

	const auto Library = LoadBytes(Psd::Build(Document));

	EXPECT_NE(Find(*Library, "Overrun"), nullptr);
}
