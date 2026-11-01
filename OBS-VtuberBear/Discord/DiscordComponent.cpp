#include "PCH.h"
#include "Discord/DiscordComponent.h"
#include <optional>
#include "Discord/DiscordVoice.h"

namespace
{
// Whether the user is speaking, empty while they aren't in the call or Discord has shut down
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
	Map.try_emplace("Talking", std::make_unique<Attribute<int>>(0, 0, 1));
	Map.try_emplace("InCall", std::make_unique<Attribute<int>>(0, 0, 1));
}

void CharacterControlOBS::DiscordComponent::UpdateAttributes(const AttributeMap& Map)
{
	if(const std::optional<bool> Speaking = GetSpeaking(Id))
	{
		if(const auto Talking = Map.find("Talking"); Talking != Map.end())
		{
			Talking->second->SetValue(*Speaking);
		}

		if(const auto InCall = Map.find("InCall"); InCall != Map.end())
		{
			InCall->second->SetValue(true);
		}
	} else
	{
		if(const auto InCall = Map.find("InCall"); InCall != Map.end())
		{
			InCall->second->SetValue(false);
		}
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
