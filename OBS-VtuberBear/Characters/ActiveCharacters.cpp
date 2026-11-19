// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Characters/ActiveCharacters.h"
#include <array>
#include <shlobj_core.h>
#include "CharacterControlCore/Messaging.h"

std::optional<std::filesystem::path> CharacterControlOBS::GetGlobalSettingsPath()
{
	try
	{
		std::array<char, MAX_PATH> DocumentsFolder = {};

		if(const HRESULT Result = SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, DocumentsFolder.data()); SUCCEEDED(Result))
		{
			return std::make_optional(std::filesystem::path(DocumentsFolder.data()) / "OBSCharacterSettings");
		}

		ERROR_("Cannot find Documents folder.");
	}
	catch(...)
	{
		ERROR_("Cannot build OBSCharacterSettings path.");
	}

	return {};
}

CharacterControlOBS::ActiveCharacters& CharacterControlOBS::ActiveCharacters::Get()
{
	return Singleton;
}

void CharacterControlOBS::ActiveCharacters::MakeActiveCharacter(const std::string& Name, Character&& NewCharacter)
{
	auto NewCharacterPtr = std::make_shared<Character>(std::move(NewCharacter));
	const std::scoped_lock Lock(CharactersMutex);
	Characters.emplace(Name, std::move(NewCharacterPtr));
}

std::shared_ptr<CharacterControlOBS::Character> CharacterControlOBS::ActiveCharacters::RemoveActiveCharacter(const std::string& Name)
{
	const std::scoped_lock Lock(CharactersMutex);
	const auto FoundIt = Characters.find(Name);

	if(FoundIt == Characters.end())
	{
		return nullptr;
	}

	std::shared_ptr<Character> Removed = std::move(FoundIt->second);
	Characters.erase(FoundIt);
	return Removed;
}

std::shared_ptr<CharacterControlOBS::Character> CharacterControlOBS::ActiveCharacters::GetActiveCharacter(const std::string& Name)
{
	const std::scoped_lock Lock(CharactersMutex);

	if(const auto FoundIt = Characters.find(Name); FoundIt != Characters.end())
	{
		return FoundIt->second;
	}

	return nullptr;
}

CharacterControlOBS::ActiveCharacters CharacterControlOBS::ActiveCharacters::Singleton;
