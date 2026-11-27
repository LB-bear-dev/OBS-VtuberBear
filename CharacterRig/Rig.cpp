// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "CharacterRig/Rig.h"
#include <fstream>
#include <glaze/glaze.hpp>
#include <iterator>
#include <util/base.h>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

Rig Rig::Load(FilePathRef Path)
{
	Rig LoadedRig;

	std::ifstream File(Path, std::ios::binary);
	if(!File)
	{
		ERROR_("Cannot open %s.", LogPath(Path).c_str());
		return LoadedRig;
	}

	INFO_("Rig file %s loaded successfully", LogPath(Path).c_str());

	const Str FileContents{std::istreambuf_iterator(File), std::istreambuf_iterator<char>()};

	// Do our best to load the file and get a usable rig out of it. Any partial readable file should give us
	// something to display which is better than nothing.
	if(const auto Error = glz::read<glz::opts{.error_on_unknown_keys = false}>(LoadedRig, FileContents))
	{
		ERROR_("%s is not a valid json file. This will be an empty rig.\n%s", LogPath(Path).c_str(), format_error(Error, FileContents).c_str());
	}

	INFO_("Rig data %s loaded successfully", LogPath(Path).c_str());

	return LoadedRig;
}

bool Rig::Save(FilePathRef Path) const
{
	std::string FileContents;
	if(const auto ErrorString = glz::write<glz::opts{.minified = true}>(*this, FileContents))
	{
		ERROR_("Cannot write rig %s.\n%s", LogPath(Path).c_str(), format_error(ErrorString, FileContents).c_str());
		return false;
	}

	INFO_("Rig data %s created successfully", LogPath(Path).c_str());

	std::ofstream File(Path, std::ios::binary);
	if(!(File << FileContents))
	{
		ERROR_("Cannot save to disk %s.", LogPath(Path).c_str());
		return false;
	}

	INFO_("Rig file %s written successfully", LogPath(Path).c_str());

	return true;
}

SlotValues Rig::EvaluateSlots(const AttributeMap& Attributes) const
{
	SlotValues EvaluatedSlots;

	for(const RigSlot& Slot : Slots)
	{
		if(EvaluatedSlots.contains(Slot.Name))
		{
			continue;
		}

		for(const auto& [ConditionValue, SlotCondition] : Slot.Rules)
		{
			if(SlotCondition.Evaluate(Attributes))
			{
				EvaluatedSlots.emplace(Slot.Name, ConditionValue);
				break;
			}
		}

		if(!Slot.Default.empty())
		{
			EvaluatedSlots.try_emplace(Slot.Name, Slot.Default);
		}
	}

	return EvaluatedSlots;
}
