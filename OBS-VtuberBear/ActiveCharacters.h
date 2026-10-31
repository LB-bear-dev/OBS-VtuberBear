#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include "Character.h"

namespace CharacterControlOBS
{
std::optional<std::filesystem::path> GetGlobalSettingsPath();

class ActiveCharacters
{
public:
	static ActiveCharacters& Get();

	void MakeActiveCharacter(const std::string& Name, Character&& NewCharacter);

	// Gives back the character it removed (null if there was none). A character's destructor takes the graphics lock, so the
	// caller lets it go with no lock of its own held; ignoring the result does that straight away.
	std::shared_ptr<Character> RemoveActiveCharacter(const std::string& Name);

	// The character stays alive for as long as the caller holds the pointer, even if another thread removes it meanwhile.
	std::shared_ptr<Character> GetActiveCharacter(const std::string& Name);

private:
	static ActiveCharacters Singleton;

	// OBS ticks and renders on the graphics thread, while updating, hiding and destroying a source can happen on others
	std::mutex CharactersMutex;
	std::unordered_map<std::string, std::shared_ptr<Character>> Characters;
};
}
