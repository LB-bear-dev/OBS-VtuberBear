// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include "Characters/Character.h"

namespace CharacterControlOBS
{
std::optional<std::filesystem::path> GetGlobalSettingsPath();

class ActiveCharacters
{
public:
	static ActiveCharacters& Get();

	void MakeActiveCharacter(const std::string& Name, Character&& NewCharacter);

	std::shared_ptr<Character> RemoveActiveCharacter(const std::string& Name);

	std::shared_ptr<Character> GetActiveCharacter(const std::string& Name);

private:
	static ActiveCharacters Singleton;

	std::mutex CharactersMutex;
	std::unordered_map<std::string, std::shared_ptr<Character>> Characters;
};
}
