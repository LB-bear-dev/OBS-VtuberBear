// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include <fstream>
#include <optional>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"
#include "Characters/CharacterSource.h"
#include "Characters/CharacterSourceInfos.h"

using namespace CharacterControlOBS;
using namespace CharacterControlRender;

namespace
{
void LoadRigImages(FilePathRef RigPath, ImageLayer& Layer)
{
	std::ifstream RigFile(RigPath);
	if(!RigFile)
	{
		ERROR_("Cannot open rig file %s.", RigPath.string().c_str());
		return;
	}

	nlohmann::json Rig = nlohmann::json::parse(RigFile, nullptr, false);
	if(Rig.is_discarded())
	{
		ERROR_("Rig file %s is not valid JSON.", RigPath.string().c_str());
		return;
	}

	auto Images = Rig.find("images");
	if(Images == Rig.end() || !Images->is_array())
	{
		WARN_("Rig file %s has no \"images\" list.", RigPath.string().c_str());
		return;
	}

	for(const nlohmann::json& Image : *Images)
	{
		auto File = Image.find("file");
		if(File == Image.end() || !File->is_string())
		{
			WARN_("Rig file %s: an image entry has no \"file\".", RigPath.string().c_str());
			continue;
		}

		b2Vec2 Offset{.x = 0, .y = 0};
		if(const auto OffsetJson = Image.find("offset"); OffsetJson != Image.end() && OffsetJson->is_array() && OffsetJson->size() == 2 && (*OffsetJson)[0].is_number() && (*OffsetJson)[1].is_number())
		{
			Offset = {.x = (*OffsetJson)[0].get<float>(), .y = (*OffsetJson)[1].get<float>()};
		}

		const auto& FileName = File->get_ref<const std::string&>();
		const std::optional<std::filesystem::path> ImagePath = ResolveInsideFolder(RigPath.parent_path(), FileName);
		if(!ImagePath)
		{
			WARN_("Rig file %s: image \"%s\" is outside the rig folder.", RigPath.string().c_str(), FileName.c_str());
			continue;
		}

		Layer.AddLayersFromFile(*ImagePath, Offset);
	}
}

struct ScriptedCharacterSource : CharacterSourceData
{
	static constexpr auto DISPLAY_NAME = "Scripted Discord Char";

	struct Settings : CharacterSettings
	{
		Str RigFile;

		void Load(obs_data_t* Data)
		{
			RigFile = obs_data_get_string(Data, "RigFile");
		}

		void SetupImages(ImageLayer& DefaultLayer) const
		{
			if(!RigFile.empty())
			{
				LoadRigImages(RigFile, DefaultLayer);
			}
		}
	};

	Settings Current;

	static void AddProperties(obs_properties_t* Properties)
	{
		obs_properties_add_text(Properties, "Name", obs_module_text("Name"), OBS_TEXT_DEFAULT);
		obs_properties_add_text(Properties, "ID", obs_module_text("Discord ID"), OBS_TEXT_DEFAULT);
		obs_properties_add_path(Properties, "RigFile", obs_module_text("Rig File"), OBS_PATH_FILE, "*.json", GetGlobalSettingsPath().value_or("").string().c_str());
	}

	static bool IsLive()
	{
		return true;
	}
};
}

obs_source_info ScriptedDiscordCharacterSourceInfo = CharacterSource<ScriptedCharacterSource>::MakeInfo("ScriptedDiscordCharacter");
