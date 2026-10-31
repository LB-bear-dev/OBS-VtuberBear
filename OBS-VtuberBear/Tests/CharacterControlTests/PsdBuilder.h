#pragma once
// Builds PSD files in memory for the loader tests, so each test says what the file holds instead of pointing at a binary
// fixture. Only what the loader reads is written: 8-bit RGB documents with layers, groups, layer masks, and the four channel
// compressions. Fields are public and unchecked on purpose, so a test can describe a file that lies about its own sizes.
//
// Layers go in bottom to top, like the file. A group is its layers between a section divider (first) and the folder
// record (last), which is what Group() produces.

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace PsdBuilder
{
struct Rect
{
	int32_t Left = 0;
	int32_t Top = 0;
	int32_t Right = 0;
	int32_t Bottom = 0;

	[[nodiscard]] int32_t Width() const
	{
		return Right - Left;
	}

	[[nodiscard]] int32_t Height() const
	{
		return Bottom - Top;
	}
};

inline Rect Box(const int32_t Left, const int32_t Top, const int32_t Width, const int32_t Height)
{
	return {Left, Top, Left + Width, Top + Height};
}

using Rgba = std::array<uint8_t, 4>;

// The compression words of the PSD format
enum Compression : uint16_t
{
	Raw = 0,
	Rle = 1,
	Zip = 2,
	ZipPrediction = 3
};

enum SectionType : uint32_t
{
	NormalLayer = 0,
	OpenFolder = 1,
	ClosedFolder = 2,
	SectionDivider = 3
};

enum ChannelId : int16_t
{
	Red = 0,
	Green = 1,
	Blue = 2,
	Alpha = -1,
	LayerMaskData = -2
};

struct Channel
{
	int16_t Id = 0;
	uint16_t CompressionWord = Raw; // the wire value, so a test can write one the SDK doesn't know
	std::vector<uint8_t> Data; // already encoded, without the compression word
	std::optional<uint32_t> DeclaredSize; // replaces the size in the layer record, which is the compression word plus Data
};

struct Mask
{
	Rect Bounds;
	uint8_t DefaultColor = 0; // what the mask is outside its rectangle
	std::vector<uint8_t> Pixels; // raw, one byte per pixel of Bounds
};

struct Layer
{
	std::string Name;
	std::u16string UnicodeName; // written as a 'luni' block when not empty
	Rect Bounds;
	uint8_t Opacity = 255;
	uint32_t Section = NormalLayer;
	std::vector<Channel> Channels;
	std::optional<Mask> LayerMask;
};

using Layers = std::vector<Layer>;

struct Document
{
	uint32_t Width = 0;
	uint32_t Height = 0;
	uint16_t Depth = 8;
	uint16_t ColorMode = 3; // RGB
	bool CompositeImage = true; // the flattened image at the end; the loader never reads it, a huge canvas can leave it out

	Layers Contents;

	Document(const uint32_t Width, const uint32_t Height) : Width(Width), Height(Height) {}

	void Add(Layer NewLayer)
	{
		Contents.push_back(std::move(NewLayer));
	}

	void Add(Layers NewLayers)
	{
		Contents.insert(Contents.end(), std::make_move_iterator(NewLayers.begin()), std::make_move_iterator(NewLayers.end()));
	}
};

// The bytes of a channel's pixels, one per pixel, row after row
inline Channel MakeChannel(const int16_t Id, const std::vector<uint8_t>& Pixels, const int Width, const Compression Codec = Raw)
{
	Channel Result;
	Result.Id = Id;
	Result.CompressionWord = Codec;

	if(Codec == Raw)
	{
		Result.Data = Pixels;
		return Result;
	}

	const int Height = Width > 0 ? static_cast<int>(Pixels.size()) / Width : 0;

	if(Codec == Rle)
	{
		// PackBits per row, after a table of the encoded row sizes
		std::vector<std::vector<uint8_t>> Rows;
		for(int Y = 0; Y < Height; ++Y)
		{
			const uint8_t* Row = Pixels.data() + static_cast<size_t>(Y) * Width;
			std::vector<uint8_t> Encoded;
			int X = 0;
			while(X < Width)
			{
				int Run = 1;
				while(X + Run < Width && Run < 128 && Row[X + Run] == Row[X])
				{
					++Run;
				}

				if(Run >= 3)
				{
					Encoded.push_back(static_cast<uint8_t>(1 - Run));
					Encoded.push_back(Row[X]);
					X += Run;
					continue;
				}

				const int Start = X;
				while(X < Width && X - Start < 128 && !(X + 2 < Width && Row[X] == Row[X + 1] && Row[X] == Row[X + 2]))
				{
					++X;
				}
				Encoded.push_back(static_cast<uint8_t>(X - Start - 1));
				Encoded.insert(Encoded.end(), Row + Start, Row + X);
			}
			Rows.push_back(std::move(Encoded));
		}

		for(const std::vector<uint8_t>& Row : Rows)
		{
			Result.Data.push_back(static_cast<uint8_t>(Row.size() >> 8));
			Result.Data.push_back(static_cast<uint8_t>(Row.size()));
		}
		for(const std::vector<uint8_t>& Row : Rows)
		{
			Result.Data.insert(Result.Data.end(), Row.begin(), Row.end());
		}
		return Result;
	}

	// Zip: a zlib stream of stored (uncompressed) blocks, which any inflater reads; prediction stores each pixel as the
	// difference from the one before it in the row
	std::vector<uint8_t> Plain = Pixels;
	if(Codec == ZipPrediction)
	{
		for(int Y = 0; Y < Height; ++Y)
		{
			uint8_t* Row = Plain.data() + static_cast<size_t>(Y) * Width;
			for(int X = Width - 1; X > 0; --X)
			{
				Row[X] = static_cast<uint8_t>(Row[X] - Row[X - 1]);
			}
		}
	}

	Result.Data = {0x78, 0x01};
	size_t Offset = 0;
	do
	{
		const size_t Length = std::min<size_t>(Plain.size() - Offset, 0xFFFF);
		const bool Final = Offset + Length == Plain.size();
		Result.Data.push_back(Final ? 1 : 0);
		Result.Data.push_back(static_cast<uint8_t>(Length));
		Result.Data.push_back(static_cast<uint8_t>(Length >> 8));
		Result.Data.push_back(static_cast<uint8_t>(~Length));
		Result.Data.push_back(static_cast<uint8_t>(~Length >> 8));
		Result.Data.insert(Result.Data.end(), Plain.begin() + static_cast<std::ptrdiff_t>(Offset), Plain.begin() + static_cast<std::ptrdiff_t>(Offset + Length));
		Offset += Length;
	} while(Offset < Plain.size());

	uint32_t A = 1;
	uint32_t B = 0;
	for(const uint8_t Byte : Plain)
	{
		A = (A + Byte) % 65521;
		B = (B + A) % 65521;
	}
	for(const uint32_t Shift : {24u, 16u, 8u, 0u})
	{
		Result.Data.push_back(static_cast<uint8_t>(((B << 16 | A) >> Shift) & 0xFF));
	}
	return Result;
}

using PixelFn = std::function<Rgba(int X, int Y)>; // X and Y are inside the layer, 0,0 being its top-left pixel

// A layer whose pixels come from a function, with alpha, in the order Photoshop writes the channels
inline Layer PixelLayer(std::string Name, const Rect Bounds, const PixelFn& Pixel, const Compression Codec = Raw)
{
	const int Width = Bounds.Width();
	const int Height = Bounds.Height();
	std::array<std::vector<uint8_t>, 4> Planes;
	for(int Y = 0; Y < Height; ++Y)
	{
		for(int X = 0; X < Width; ++X)
		{
			const Rgba Value = Pixel(X, Y);
			for(int Plane = 0; Plane < 4; ++Plane)
			{
				Planes[Plane].push_back(Value[Plane]);
			}
		}
	}

	Layer Result;
	Result.Name = std::move(Name);
	Result.Bounds = Bounds;
	Result.Channels = {MakeChannel(Alpha, Planes[3], Width, Codec), MakeChannel(Red, Planes[0], Width, Codec), MakeChannel(Green, Planes[1], Width, Codec), MakeChannel(Blue, Planes[2], Width, Codec)};
	return Result;
}

inline Layer SolidLayer(std::string Name, const Rect Bounds, const uint8_t R, const uint8_t G, const uint8_t B, const uint8_t A = 255)
{
	return PixelLayer(std::move(Name),
		Bounds,
		[=](int, int)
		{
			return Rgba{R, G, B, A};
		});
}

// What Photoshop writes for a group: a divider layer, the contents, then the folder layer carrying the group's name and opacity
inline Layers Group(std::string Name, const uint8_t Opacity, Layers Contents)
{
	const auto Empty = [](std::string LayerName, const SectionType Section, const uint8_t LayerOpacity)
	{
		Layer Result;
		Result.Name = std::move(LayerName);
		Result.Section = Section;
		Result.Opacity = LayerOpacity;
		Result.Channels = {Channel{Alpha}, Channel{Red}, Channel{Green}, Channel{Blue}};
		return Result;
	};

	Layers Result;
	Result.push_back(Empty("</Layer group>", SectionDivider, 255));
	Result.insert(Result.end(), std::make_move_iterator(Contents.begin()), std::make_move_iterator(Contents.end()));
	Result.push_back(Empty(std::move(Name), OpenFolder, Opacity));
	return Result;
}

namespace Detail
{
	class Writer
	{
	public:
		void U8(const uint32_t Value)
		{
			Bytes.push_back(static_cast<uint8_t>(Value));
		}

		void U16(const uint32_t Value)
		{
			U8(Value >> 8);
			U8(Value);
		}

		void U32(const uint32_t Value)
		{
			U16(Value >> 16);
			U16(Value);
		}

		void I16(const int16_t Value)
		{
			U16(static_cast<uint16_t>(Value));
		}

		void I32(const int32_t Value)
		{
			U32(static_cast<uint32_t>(Value));
		}

		void Text(const std::string& Value)
		{
			Bytes.insert(Bytes.end(), Value.begin(), Value.end());
		}

		void Append(const std::vector<uint8_t>& Value)
		{
			Bytes.insert(Bytes.end(), Value.begin(), Value.end());
		}

		void Zeros(const size_t Count)
		{
			Bytes.insert(Bytes.end(), Count, 0);
		}

		void PadTo(const size_t Multiple)
		{
			while(Bytes.size() % Multiple != 0)
			{
				Bytes.push_back(0);
			}
		}

		[[nodiscard]] size_t Size() const
		{
			return Bytes.size();
		}

		[[nodiscard]] const std::vector<uint8_t>& Data() const
		{
			return Bytes;
		}

	private:
		std::vector<uint8_t> Bytes;
	};

	inline std::vector<Channel> ChannelsOf(const Layer& Source)
	{
		std::vector<Channel> Result = Source.Channels;
		if(Source.LayerMask)
		{
			Channel MaskChannel;
			MaskChannel.Id = LayerMaskData;
			MaskChannel.Data = Source.LayerMask->Pixels;
			Result.push_back(std::move(MaskChannel));
		}
		return Result;
	}

	// The layer's own record: rectangle, channel sizes, blend settings, then the "extra data" with mask, name and blocks
	inline void WriteRecord(Writer& Out, const Layer& Source, const std::vector<Channel>& Channels)
	{
		Out.I32(Source.Bounds.Top);
		Out.I32(Source.Bounds.Left);
		Out.I32(Source.Bounds.Bottom);
		Out.I32(Source.Bounds.Right);
		Out.U16(static_cast<uint32_t>(Channels.size()));
		for(const Channel& Item : Channels)
		{
			Out.I16(Item.Id);
			Out.U32(Item.DeclaredSize.value_or(static_cast<uint32_t>(2 + Item.Data.size())));
		}

		Out.Text("8BIM");
		Out.Text("norm");
		Out.U8(Source.Opacity);
		Out.U8(0); // clipping
		Out.U8(0); // flags
		Out.U8(0); // filler

		Writer Extra;
		if(Source.LayerMask)
		{
			Extra.U32(20);
			Extra.I32(Source.LayerMask->Bounds.Top);
			Extra.I32(Source.LayerMask->Bounds.Left);
			Extra.I32(Source.LayerMask->Bounds.Bottom);
			Extra.I32(Source.LayerMask->Bounds.Right);
			Extra.U8(Source.LayerMask->DefaultColor);
			Extra.U8(0); // flags: a layer mask, not a vector mask, with no parameters
			Extra.U16(0); // padding
		} else
		{
			Extra.U32(0);
		}

		Extra.U32(0); // blending ranges

		// Pascal string, padded so that the length byte and the text come to a multiple of 4. The extra data so far is a
		// multiple of 4 too (8 or 28 bytes), so padding the whole of it does that.
		const std::string Name = Source.Name.substr(0, 255);
		Extra.U8(static_cast<uint32_t>(Name.size()));
		Extra.Text(Name);
		Extra.PadTo(4);

		const auto Block = [&Extra](const char* Key, const Writer& Payload)
		{
			Extra.Text("8BIM");
			Extra.Text(Key);
			Extra.U32(static_cast<uint32_t>(Payload.Size()));
			Extra.Append(Payload.Data());
		};

		if(!Source.UnicodeName.empty())
		{
			Writer Payload;
			Payload.U32(static_cast<uint32_t>(Source.UnicodeName.size()));
			for(const char16_t Unit : Source.UnicodeName)
			{
				Payload.U16(Unit);
			}
			Block("luni", Payload);
		}

		if(Source.Section != NormalLayer)
		{
			Writer Payload;
			Payload.U32(Source.Section);
			Block("lsct", Payload);
		}

		Out.U32(static_cast<uint32_t>(Extra.Size()));
		Out.Append(Extra.Data());
	}
}

inline std::vector<uint8_t> Build(const Document& Source)
{
	using namespace Detail;

	Writer Out;
	Out.Text("8BPS");
	Out.U16(1); // version
	Out.Zeros(6);
	Out.U16(3); // channels in the document
	Out.U32(Source.Height);
	Out.U32(Source.Width);
	Out.U16(Source.Depth);
	Out.U16(Source.ColorMode);
	Out.U32(0); // color mode data
	Out.U32(0); // image resources

	// Layer records, then every layer's channel data in the same order
	Writer LayerInfo;
	LayerInfo.I16(static_cast<int16_t>(Source.Contents.size()));
	for(const Layer& Item : Source.Contents)
	{
		WriteRecord(LayerInfo, Item, ChannelsOf(Item));
	}
	for(const Layer& Item : Source.Contents)
	{
		for(const Channel& Piece : ChannelsOf(Item))
		{
			LayerInfo.U16(Piece.CompressionWord);
			LayerInfo.Append(Piece.Data);
		}
	}
	LayerInfo.PadTo(2);

	Out.U32(static_cast<uint32_t>(4 + LayerInfo.Size() + 4)); // layer and mask section: layer info, then the empty global mask info
	Out.U32(static_cast<uint32_t>(LayerInfo.Size()));
	Out.Append(LayerInfo.Data());
	Out.U32(0);

	if(Source.CompositeImage)
	{
		Out.U16(Raw);
		Out.Zeros(static_cast<size_t>(Source.Width) * Source.Height * 3);
	}

	return Out.Data();
}
}
