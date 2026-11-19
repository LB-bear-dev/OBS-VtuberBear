// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <map>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/CoreTypes.h"
#include "CharacterRig/Attribute.h"
#include "CharacterRig/Condition.h"
#include "CharacterRig/Rig.h"

namespace CharacterControlOBS
{
	struct ComponentEntry
	{
		StrId Type;

		nlohmann::json Settings;

		bool operator==(const ComponentEntry&) const = default;
	};

	struct Look
	{
		StrId Heading;

		StrId Style;

		Condition When;

		bool operator==(const Look&) const = default;
	};

	enum class HotkeyMode : uint8_t
	{
		Set,
		Toggle,
		Clear
	};

	struct Hotkey
	{
		StrId Name;

		StrId Attribute;

		std::optional<Attribute::Value> Value;

		StrId Channel;

		HotkeyMode Mode = HotkeyMode::Set;

		std::optional<double> Seconds;

		bool operator==(const Hotkey&) const = default;
	};

	struct CharacterFile
	{
		Str Name;

		FilePath RigPath;

		std::vector<ComponentEntry> Components;

		std::map<StrId, Attribute::Value> Defaults;

		std::map<StrId, FilePath> Styles;

		std::vector<Look> Looks;

		std::vector<StrId> Channels;

		std::vector<Hotkey> Hotkeys;

		bool operator==(const CharacterFile&) const = default;
	};

	struct LoadedCharacter
	{
		CharacterFile File;

		Rig RigDefinition;
	};

	std::vector<StrId> DefaultChannels();

	std::optional<CharacterFile> ParseCharacterFile(const Str& Text, FilePathRef Folder);

	std::optional<LoadedCharacter> LoadCharacterFile(FilePathRef Folder);

	std::optional<FilePath> FindCharacterFolder(FilePathRef CharactersFolder, StrIdRef DiscordId);
}