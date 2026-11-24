// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Characters/CharacterSource.h"
#include "Characters/CharacterSourceInfos.h"

using namespace CharacterControlOBS;
using namespace CharacterControlRender;

namespace
{
struct SimpleCharacterSource : CharacterSourceData
{
	static constexpr auto DISPLAY_NAME = "Simple Discord Char";

	struct Settings : CharacterSettings
	{
		Utf8Str NotSpeakingImg;
		Utf8Str SpeakingImg;

		void Load(obs_data_t* Data)
		{
			NotSpeakingImg = obs_data_get_string(Data, "NotSpeakingImg");
			SpeakingImg = obs_data_get_string(Data, "SpeakingImg");
		}

		void SetupImages(ImageLayer& DefaultLayer) const
		{
			DefaultLayer.CreateSlot("SpeakingState");
			DefaultLayer.AddImageLibraryToLayer(SpeakingImg, "TALK");
			DefaultLayer.AddImageLibraryToLayer(NotSpeakingImg, "SILENT");
		}
	};

	Settings Current;

	static void AddProperties(obs_properties_t* Properties)
	{
		const std::string SettingsPath = GetGlobalSettingsPath().value_or("").string();

		obs_properties_add_text(Properties, "Name", obs_module_text("Character Name"), OBS_TEXT_DEFAULT);
		obs_properties_add_text(Properties, "ID", obs_module_text("Discord ID"), OBS_TEXT_DEFAULT);
		obs_properties_add_path(Properties, "SpeakingImg", obs_module_text("Image when speaking"), OBS_PATH_FILE, "*.*", SettingsPath.c_str());
		obs_properties_add_path(Properties, "NotSpeakingImg", obs_module_text("Image when not speaking"), OBS_PATH_FILE, "*.*", SettingsPath.c_str());
	}

	bool IsLive() const
	{
		return Active && IdIsActiveInDiscord(Current.Id);
	}
};
}

obs_source_info SimpleDiscordCharacterSourceInfo = CharacterSource<SimpleCharacterSource>::MakeInfo("SimpleDiscordCharacter");
