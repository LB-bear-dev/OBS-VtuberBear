// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
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

void ShutdownDiscord();
}
