#pragma once
#include "Characters/Character.h"

namespace CharacterControlOBS
{
class DiscordComponent final : public Component
{
public:
	explicit DiscordComponent(std::string Id) : Id(std::move(Id)) {}

	void DeclareAttributes(AttributeMap& Map) override;
	void UpdateAttributes(const AttributeMap& Map) override;

private:
	std::string Id;
};

bool IdIsActiveInDiscord(const std::string& Id);

// Must run from obs_module_unload, at process exit the WebView's thread is already gone and stopping would wait on it forever.
void ShutdownDiscord();
}
