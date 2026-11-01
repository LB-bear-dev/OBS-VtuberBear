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

		if(HRESULT Result = SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, DocumentsFolder.data()); SUCCEEDED(Result))
		{
			return std::make_optional(std::filesystem::path(DocumentsFolder.data()) / "OBSCharacterSettings");
		}

		ERROR_("Could not access personal documents folder. It is required to store your image data in a folder called 'OBSCharacterSettings' in your user folder.");
	}
	catch(...)
	{
		ERROR_("Could not build the path to the 'OBSCharacterSettings' folder.");
	}

	return {};
}

CharacterControlOBS::ActiveCharacters& CharacterControlOBS::ActiveCharacters::Get()
{
	return Singleton;
}

void CharacterControlOBS::ActiveCharacters::MakeActiveCharacter(const std::string& Name, Character&& NewCharacter)
{
	// Built before the lock; if the name is taken the new one is dropped on the way out, after the lock is gone
	auto NewCharacterPtr = std::make_shared<Character>(std::move(NewCharacter));
	std::scoped_lock Lock(CharactersMutex);
	Characters.emplace(Name, std::move(NewCharacterPtr));
}

std::shared_ptr<CharacterControlOBS::Character> CharacterControlOBS::ActiveCharacters::RemoveActiveCharacter(const std::string& Name)
{
	std::scoped_lock Lock(CharactersMutex);
	auto FoundIt = Characters.find(Name);

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
	std::scoped_lock Lock(CharactersMutex);
	auto FoundIt = Characters.find(Name);

	if(FoundIt != Characters.end())
	{
		return FoundIt->second;
	}

	return nullptr;
}

CharacterControlOBS::ActiveCharacters CharacterControlOBS::ActiveCharacters::Singleton;
