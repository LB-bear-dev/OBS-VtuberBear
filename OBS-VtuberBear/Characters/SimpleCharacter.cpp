#include "PCH.h"
#include "Characters/CharacterSource.h"

using namespace CharacterControlOBS;
using namespace CharacterControlRender;

namespace
{
// Two images, swapped while the Discord user talks
struct SimpleCharacterSource : CharacterSourceData
{
	static constexpr auto DISPLAY_NAME = "Simple Discord Char";
	static constexpr bool UPDATES_RENDER_STATE = false;

	Str NotSpeakingImg;
	Str SpeakingImg;

	static void AddProperties(obs_properties_t* Properties)
	{
		const std::string SettingsPath = GetGlobalSettingsPath().value_or("").string();

		obs_properties_add_text(Properties, "Name", obs_module_text("Character Name"), OBS_TEXT_DEFAULT);
		obs_properties_add_text(Properties, "ID", obs_module_text("Discord ID"), OBS_TEXT_DEFAULT);
		obs_properties_add_path(Properties, "SpeakingImg", obs_module_text("Image when speaking"), OBS_PATH_FILE, "*.*", SettingsPath.c_str());
		obs_properties_add_path(Properties, "NotSpeakingImg", obs_module_text("Image when not speaking"), OBS_PATH_FILE, "*.*", SettingsPath.c_str());
	}

	void LoadSettings(obs_data_t* Settings)
	{
		NotSpeakingImg = obs_data_get_string(Settings, "NotSpeakingImg");
		SpeakingImg = obs_data_get_string(Settings, "SpeakingImg");
	}

	void SetupImages(ImageLayer& DefaultLayer) const
	{
		DefaultLayer.CreateSlot("SpeakingState");
		DefaultLayer.AddImageLibraryToLayer(SpeakingImg, "TALK");
		DefaultLayer.AddImageLibraryToLayer(NotSpeakingImg, "SILENT");
	}

	// Shown, and the user is in a call
	[[nodiscard]] bool IsLive() const
	{
		return Active && IdIsActiveInDiscord(Id);
	}
};
}

obs_source_info SimpleDiscordCharacterSourceInfo = CharacterSource<SimpleCharacterSource>::MakeInfo("SimpleDiscordCharacter");
