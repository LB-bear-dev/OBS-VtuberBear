// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Discord/DiscordComponent.h"
#include <optional>
#include "Discord/DiscordVoice.h"

namespace
{
std::optional<bool> GetSpeaking(const std::string& Id)
{
	if(const CharacterControlOBS::DiscordVoice* Voice = CharacterControlOBS::DiscordVoice::Get())
	{
		return Voice->GetSpeaking(Id);
	}

	return std::nullopt;
}
}

void CharacterControlOBS::DiscordComponent::DeclareAttributes(AttributeMap& Map)
{
	Map.try_emplace("Talking", std::make_unique<Attribute>(false));
	Map.try_emplace("InCall", std::make_unique<Attribute>(false));
}

void CharacterControlOBS::DiscordComponent::UpdateAttributes(const AttributeMap& Map)
{
	if(const std::optional<bool> Speaking = GetSpeaking(Id))
	{
		SetAttribute(Map, "Talking", *Speaking);
		SetAttribute(Map, "InCall", true);
	} else
	{
		SetAttribute(Map, "InCall", false);
	}
}

bool CharacterControlOBS::IdIsActiveInDiscord(const std::string& Id)
{
	return GetSpeaking(Id).has_value();
}

void CharacterControlOBS::ShutdownDiscord()
{
	DiscordVoice::Shutdown();
}
