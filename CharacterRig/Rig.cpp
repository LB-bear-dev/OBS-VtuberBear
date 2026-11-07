#include "CharacterRig/Rig.h"
#include <algorithm>
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
bool HasName(const std::vector<StrId>& Names, StrIdRef Name)
{
	return std::ranges::find(Names, Name) != Names.end();
}

bool HasLayer(const std::vector<RigLayer>& Layers, StrIdRef Name)
{
	return std::ranges::find(Layers, Name, &RigLayer::Name) != Layers.end();
}

bool HasSlot(const std::vector<RigSlot>& Slots, StrIdRef Name)
{
	return std::ranges::find(Slots, Name, &RigSlot::Name) != Slots.end();
}

class RigParser : JsonParser
{
public:
	using JsonParser::JsonParser;
	using JsonParser::ParseObject;

	[[nodiscard]] std::optional<Rig> Parse(const nlohmann::json& Json) const;

private:
	[[nodiscard]] std::optional<std::vector<RigLayer>> ReadLayers(const nlohmann::json& Json) const;

	[[nodiscard]] bool CheckParents(const std::vector<RigLayer>& Layers) const;

	[[nodiscard]] std::optional<std::vector<RigSlot>> ReadSlots(const nlohmann::json& Json) const;

	[[nodiscard]] std::optional<RigSlot> ReadSlot(const nlohmann::json& Json, int Number) const;

	[[nodiscard]] std::optional<SlotRule> ReadRule(const nlohmann::json& Json, int Number, const RigSlot& Slot) const;

	[[nodiscard]] bool CheckLayerSlots(const std::vector<RigLayer>& Layers, const std::vector<RigSlot>& Slots) const;
};

std::optional<Rig> RigParser::Parse(const nlohmann::json& Json) const
{
	std::optional<std::vector<RigLayer>> Layers = ReadLayers(Json);
	if(!Layers || !CheckParents(*Layers))
	{
		return std::nullopt;
	}

	std::optional<std::vector<RigSlot>> Slots = ReadSlots(Json);
	if(!Slots || !CheckLayerSlots(*Layers, *Slots))
	{
		return std::nullopt;
	}

	return Rig{.Layers = std::move(*Layers), .Slots = std::move(*Slots)};
}

std::optional<std::vector<RigLayer>> RigParser::ReadLayers(const nlohmann::json& Json) const
{
	const auto FoundLayers = Json.find("layers");
	if(FoundLayers == Json.end() || !FoundLayers->is_array())
	{
		ERROR_("%s: the rig needs \"layers\", a list of layers.", GetLogName().c_str());
		return std::nullopt;
	}

	std::vector<RigLayer> Layers;
	int Number = 0;
	for(const nlohmann::json& FoundLayer : *FoundLayers)
	{
		++Number;
		if(!FoundLayer.is_object())
		{
			ERROR_("layer %s %d must be a JSON object.", GetLogName().c_str(), Number);
			return std::nullopt;
		}

		std::optional<StrId> Name = ReadName(FoundLayer, "name");
		if(!Name)
		{
			return std::nullopt;
		}

		if(HasLayer(Layers, *Name))
		{
			ERROR_("%s: there are two layers named \"%s\".", GetLogName().c_str(), Name->c_str());
			return std::nullopt;
		}

		RigLayer Layer{.Name = std::move(*Name), .Parent = {}, .Slots = {}};
		if(FoundLayer.contains("parent"))
		{
			std::optional<StrId> Parent = ReadName(FoundLayer, "parent");
			if(!Parent)
			{
				return std::nullopt;
			}

			Layer.Parent = std::move(*Parent);
		}

		if(FoundLayer.contains("slots"))
		{
			std::optional<std::vector<StrId>> LayerSlots = ReadNames(FoundLayer, "slots");
			if(!LayerSlots)
			{
				return std::nullopt;
			}

			Layer.Slots = std::move(*LayerSlots);
		}

		Layers.push_back(std::move(Layer));
	}

	return Layers;
}

bool RigParser::CheckParents(const std::vector<RigLayer>& Layers) const
{
	for(const RigLayer& Layer : Layers)
	{
		if(!Layer.Parent.empty() && HasLayer(Layers, Layer.Parent))
		{
			ERROR_("%s: layer \"%s\" has parent \"%s\", which isn't a layer.", GetLogName().c_str(), Layer.Name.c_str(), Layer.Parent.c_str());
			return false;
		}

		StrId Ancestor = Layer.Parent;
		for(size_t Steps = 0; !Ancestor.empty() && Steps < Layers.size(); ++Steps)
		{
			if(Ancestor == Layer.Name)
			{
				ERROR_("%s: layer \"%s\" is its own ancestor.", GetLogName().c_str(), Layer.Name.c_str());
				return false;
			}

			Ancestor = std::ranges::find(Layers, Ancestor, &RigLayer::Name)->Parent;
		}
	}

	return true;
}

std::optional<std::vector<RigSlot>> RigParser::ReadSlots(const nlohmann::json& Json) const
{
	std::vector<RigSlot> Slots;
	const auto Found = Json.find("slots");
	if(Found == Json.end())
	{
		return Slots;
	}

	if(!Found->is_array())
	{
		ERROR_("%s: \"slots\" must be a list of slots.", GetLogName().c_str());
		return std::nullopt;
	}

	int Number = 0;
	for(const nlohmann::json& Entry : *Found)
	{
		std::optional<RigSlot> Slot = ReadSlot(Entry, ++Number);
		if(!Slot)
		{
			return std::nullopt;
		}

		if(HasSlot(Slots, Slot->Name))
		{
			ERROR_("%s: there are two slots named \"%s\".", GetLogName().c_str(), Slot->Name.c_str());
			return std::nullopt;
		}

		Slots.push_back(std::move(*Slot));
	}

	return Slots;
}

std::optional<RigSlot> RigParser::ReadSlot(const nlohmann::json& Json, const int Number) const
{
	const Str NumberContext = "slot " + std::to_string(Number);
	if(!Json.is_object())
	{
		ERROR_("%s: %s must be a JSON object.", GetLogName().c_str(), NumberContext.c_str());
		return std::nullopt;
	}

	std::optional<StrId> Name = ReadName(Json, "name");
	if(!Name)
	{
		return std::nullopt;
	}

	RigSlot Slot{.Name = std::move(*Name), .Values = {}, .Rules = {}};

	std::optional<std::vector<StrId>> Values = ReadNames(Json, "values");
	if(!Values)
	{
		return std::nullopt;
	}

	if(Values->empty())
	{
		ERROR_("%s: %s needs at least one value.", GetLogName().c_str());
		return std::nullopt;
	}

	for(const StrId& Value : *Values)
	{
		if(Value.find('+') != StrId::npos)
		{
			ERROR_("%s: %s has the value \"%s\", but values can't contain \"+\".", GetLogName().c_str(), Value.c_str());
			return std::nullopt;
		}
	}

	Slot.Values = std::move(*Values);

	const auto Rules = Json.find("rules");
	if(Rules == Json.end())
	{
		return Slot;
	}

	if(!Rules->is_array())
	{
		ERROR_("%s: \"rules\" in %s must be a list of rules.", GetLogName().c_str());
		return std::nullopt;
	}

	int RuleNumber = 0;
	for(const nlohmann::json& Entry : *Rules)
	{
		std::optional<SlotRule> Rule = ReadRule(Entry, ++RuleNumber, Slot);
		if(!Rule)
		{
			return std::nullopt;
		}

		Slot.Rules.push_back(std::move(*Rule));
	}

	return Slot;
}

std::optional<SlotRule> RigParser::ReadRule(const nlohmann::json& Json, const int Number, const RigSlot& Slot) const
{
	const Str Context = "slot \"" + Slot.Name + "\" rule " + std::to_string(Number);
	if(!Json.is_object())
	{
		ERROR_("%s: %s must be a JSON object.", GetLogName().c_str(), Context.c_str());
		return std::nullopt;
	}

	if(!HasOnlyKeys(Json, {"value", "when"}))
	{
		return std::nullopt;
	}

	std::optional<StrId> Value = ReadName(Json, "value");
	if(!Value)
	{
		return std::nullopt;
	}

	if(!HasName(Slot.Values, *Value))
	{
		ERROR_("%s: %s sets \"%s\", which isn't one of the slot's values.", GetLogName().c_str(), Context.c_str(), Value->c_str());
		return std::nullopt;
	}

	SlotRule Rule{.Value = std::move(*Value), .When = {}};
	if(const auto When = Json.find("when"); When != Json.end())
	{
		std::optional<Condition> Parsed = ParseCondition(*When);
		if(!Parsed)
		{
			ERROR_("%s: %s has an invalid condition.", GetLogName().c_str(), Context.c_str());
			return std::nullopt;
		}

		Rule.When = std::move(*Parsed);
	}

	return Rule;
}

bool RigParser::CheckLayerSlots(const std::vector<RigLayer>& Layers, const std::vector<RigSlot>& Slots) const
{
	for(const RigLayer& Layer : Layers)
	{
		for(const StrId& SlotName : Layer.Slots)
		{
			if(!HasSlot(Slots, SlotName))
			{
				ERROR_("%s: layer \"%s\" lists slot \"%s\", which isn't a slot.", GetLogName().c_str(), Layer.Name.c_str(), SlotName.c_str());
				return false;
			}
		}
	}

	return true;
}
}

std::optional<Rig> CharacterControlOBS::ParseRig(const Str& Text, FilePathRef Path)
{
	const RigParser Parser(LogPath(Path));
	const std::optional<nlohmann::json> Json = Parser.ParseObject(Text);
	if(!Json)
	{
		return std::nullopt;
	}

	return Parser.Parse(*Json);
}

std::optional<Rig> CharacterControlOBS::LoadRig(FilePathRef Path)
{
	std::ifstream File(Path, std::ios::binary);
	if(!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return std::nullopt;
	}

	const Str Text{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};
	return ParseRig(Text, Path);
}
