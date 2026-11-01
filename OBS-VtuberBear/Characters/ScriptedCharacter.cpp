#include "PCH.h"
#include <fstream>
#include <optional>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"
#include "Characters/CharacterSource.h"
#include "Characters/CharacterSourceInfos.h"
#include "Python/PythonInterface.h"

using namespace CharacterControlOBS;
using namespace CharacterControlRender;

namespace
{
// Loads the rig file's "images" list into the layer. Image paths are relative to the rig file.
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

		Coord Offset{.X = 0, .Y = 0};
		if(const auto OffsetJson = Image.find("offset"); OffsetJson != Image.end() && OffsetJson->is_array() && OffsetJson->size() == 2 && (*OffsetJson)[0].is_number() && (*OffsetJson)[1].is_number())
		{
			Offset = {.X = (*OffsetJson)[0].get<float>(), .Y = (*OffsetJson)[1].get<float>()};
		}

		const auto& FileName = File->get_ref<const std::string&>();
		// Image paths must stay inside the rig file's folder
		const std::optional<std::filesystem::path> ImagePath = ResolveInsideFolder(RigPath.parent_path(), FileName);
		if(!ImagePath)
		{
			WARN_("Rig file %s: image \"%s\" is not inside the rig file's folder, skipping it.", RigPath.string().c_str(), FileName.c_str());
			continue;
		}

		Layer.AddLayersFromFile(*ImagePath, Offset);
	}
}

// Layers come from a rig file that lists the images making up the character
struct ScriptedCharacterSource : CharacterSourceData
{
	static constexpr auto DISPLAY_NAME = "Scripted Discord Char";
	static constexpr bool UPDATES_RENDER_STATE = true;

	Str RigFile;

	static void AddProperties(obs_properties_t* Properties)
	{
		obs_properties_add_text(Properties, "Name", obs_module_text("Name"), OBS_TEXT_DEFAULT);
		obs_properties_add_text(Properties, "ID", obs_module_text("Discord ID"), OBS_TEXT_DEFAULT);
		obs_properties_add_path(Properties, "RigFile", obs_module_text("Rig File"), OBS_PATH_FILE, "*.json", GetGlobalSettingsPath().value_or("").string().c_str());

		// Set once a script tried to start Python and no Python 3 was found
		const Str& PythonError = CharacterControlScript::PythonInterface::GetLastError();
		if(!PythonError.empty())
		{
			obs_property_t* Status = obs_properties_add_text(Properties, "PythonStatus", PythonError.c_str(), OBS_TEXT_INFO);
			obs_property_text_set_info_type(Status, OBS_TEXT_INFO_ERROR);
		}
	}

	void LoadSettings(obs_data_t* Settings)
	{
		RigFile = obs_data_get_string(Settings, "RigFile");
	}

	void SetupImages(ImageLayer& DefaultLayer) const
	{
		if(!RigFile.empty())
		{
			LoadRigImages(RigFile, DefaultLayer);
		}
	}

	[[nodiscard]] static bool IsLive()
	{
		return true;
	}
};
}

obs_source_info ScriptedDiscordCharacterSourceInfo = CharacterSource<ScriptedCharacterSource>::MakeInfo("ScriptedDiscordCharacter");
