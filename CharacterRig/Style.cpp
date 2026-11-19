// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/Style.h"
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/JsonParser.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

namespace
{
	class StyleParser : JsonParser
	{
	public:
		StyleParser(Str InLogName, FilePath InFolder) : JsonParser(std::move(InLogName)), Folder(std::move(InFolder))
		{}

		using JsonParser::ParseObject;

		std::optional<Style> Parse(const nlohmann::json& Json) const;

	private:
		std::optional<StyleLayer> ReadLayer(const nlohmann::json& Json, StrIdRef Name) const;

		std::optional<b2Vec2> ReadPoint(const nlohmann::json& Json, const char* Field, const Str& Where) const;

		std::optional<FilePath> ReadPath(const nlohmann::json& Json, const Str& Where) const;

		std::optional<StyleImage> ReadImage(const nlohmann::json& Json, StrIdRef Combination, const std::optional<FilePath>& LayerFile, const Str& LayerWhere) const;

		FilePath Folder;
	};

	std::optional<Style> StyleParser::Parse(const nlohmann::json& Json) const
	{
		const auto FoundLayers = Json.find("layers");
		if (FoundLayers == Json.end() || !FoundLayers->is_object())
		{
			ERROR_("%s: \"layers\" must be an object.", GetLogName().c_str());
			return std::nullopt;
		}

		Style Result;
		for (const auto& Item : FoundLayers->items())
		{
			std::optional<StyleLayer> Layer = ReadLayer(Item.value(), Item.key());
			if (!Layer)
			{
				return std::nullopt;
			}

			Result.Layers.emplace(Item.key(), std::move(*Layer));
		}

		return Result;
	}

	std::optional<StyleLayer> StyleParser::ReadLayer(const nlohmann::json& Json, StrIdRef Name) const
	{
		StyleLayer Layer;
		if (Json.is_null())
		{
			Layer.Hidden = true;
			return Layer;
		}

		if (!Json.is_object())
		{
			ERROR_("%s: layer \"%s\" must be a JSON object or null.", GetLogName().c_str(), Name.c_str());
			return std::nullopt;
		}

		const Str Where = "layer \"" + Name + "\"";
		std::optional<FilePath> LayerFile;
		if (const auto File = Json.find("file"); File != Json.end())
		{
			LayerFile = ReadPath(*File, Where);
			if (!LayerFile)
			{
				return std::nullopt;
			}
		}

		if (const auto Offset = Json.find("offset"); Offset != Json.end())
		{
			const std::optional<b2Vec2> Point = ReadPoint(*Offset, "offset", Where);
			if (!Point)
			{
				return std::nullopt;
			}

			Layer.Offset = *Point;
		}

		if (const auto Pivot = Json.find("pivot"); Pivot != Json.end())
		{
			Layer.Pivot = ReadPoint(*Pivot, "pivot", Where);
			if (!Layer.Pivot)
			{
				return std::nullopt;
			}
		}

		const auto Images = Json.find("images");
		if (Images == Json.end())
		{
			return Layer;
		}

		if (!Images->is_object())
		{
			ERROR_("%s: \"images\" in %s must be a JSON object.", GetLogName().c_str(), Where.c_str());
			return std::nullopt;
		}

		for (const auto& Item : Images->items())
		{
			std::optional<StyleImage> Image = ReadImage(Item.value(), Item.key(), LayerFile, Where);
			if (!Image)
			{
				return std::nullopt;
			}

			Layer.SlotImages.emplace(Item.key(), std::move(*Image));
		}

		return Layer;
	}

	std::optional<b2Vec2> StyleParser::ReadPoint(const nlohmann::json& Json, const char* Field, const Str& Where) const
	{
		if (!Json.is_array() || Json.size() != 2 || !Json[0].is_number() || !Json[1].is_number())
		{
			ERROR_("%s: \"%s\" in %s must be two numbers.", GetLogName().c_str(), Field, Where.c_str());
			return std::nullopt;
		}

		return b2Vec2{.x = Json[0].get<float>(), .y = Json[1].get<float>()};
	}

	std::optional<FilePath> StyleParser::ReadPath(const nlohmann::json& Json, const Str& Where) const
	{
		if (!Json.is_string())
		{
			ERROR_("%s: \"file\" in %s must be a string.", GetLogName().c_str(), Where.c_str());
			return std::nullopt;
		}

		const auto& Written = Json.get_ref<const Str&>();
		std::optional<FilePath> Resolved = ResolveInsideFolder(Folder, Written);
		if (!Resolved)
		{
			ERROR_("%s: \"file\" in %s is outside the character folder: \"%s\".", GetLogName().c_str(), Where.c_str(), Written.c_str());
		}

		return Resolved;
	}

	std::optional<StyleImage> StyleParser::ReadImage(const nlohmann::json& Json, StrIdRef Combination, const std::optional<FilePath>& LayerFile, const Str& LayerWhere) const
	{
		const Str Where = "image \"" + Combination + "\" in " + LayerWhere;
		if (Json.is_string())
		{
			if (!LayerFile)
			{
				ERROR_("%s: %s needs the layer's \"file\".", GetLogName().c_str(), Where.c_str());
				return std::nullopt;
			}

			return StyleImage{.File = *LayerFile, .Layer = Json.get<StrId>()};
		}

		if (!Json.is_object())
		{
			ERROR_("%s: %s must be a string or a JSON object.", GetLogName().c_str(), Where.c_str());
			return std::nullopt;
		}

		const auto File = Json.find("file");
		if (File == Json.end())
		{
			ERROR_("%s: %s needs a \"file\".", GetLogName().c_str(), Where.c_str());
			return std::nullopt;
		}

		std::optional<FilePath> ImageFile = ReadPath(*File, Where);
		if (!ImageFile)
		{
			return std::nullopt;
		}

		StyleImage Image{.File = std::move(*ImageFile)};
		if (const auto PsdLayer = Json.find("layer"); PsdLayer != Json.end())
		{
			if (!PsdLayer->is_string())
			{
				ERROR_("%s: \"layer\" in %s must be a string.", GetLogName().c_str(), Where.c_str());
				return std::nullopt;
			}

			Image.Layer = PsdLayer->get<StrId>();
		}

		return Image;
	}
}

std::optional<Style> CharacterControlOBS::ParseStyle(const Str& Text, FilePathRef Path, FilePathRef Folder)
{
	const StyleParser Parser(LogPath(Path), Folder);
	const std::optional<nlohmann::json> Json = Parser.ParseObject(Text);
	if (!Json)
	{
		return std::nullopt;
	}

	return Parser.Parse(*Json);
}

std::optional<Style> CharacterControlOBS::LoadStyle(FilePathRef Path, FilePathRef Folder)
{
	std::ifstream File(Path, std::ios::binary);
	if (!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return std::nullopt;
	}

	const Str Text{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};
	return ParseStyle(Text, Path, Folder);
}