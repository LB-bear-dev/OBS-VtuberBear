/*
OBS-VtuberBear
Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "PCH.h"
#include <obs-module.h>
#include "Discord/DiscordComponent.h"
#include "Discord/DiscordDock.h"
#include "CharacterControlCore/Messaging.h"
#include "plugin-macros.h"

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

extern obs_source_info SimpleDiscordCharacterSourceInfo;
extern obs_source_info ScriptedDiscordCharacterSourceInfo;

namespace
{
// The plugin was CharacterControlOBS.dll before it was renamed, and OBS names a plugin's config folder after its DLL, so the Discord
// Voice settings and StreamKit's sign-in from those versions are in plugin_config\CharacterControlOBS. Moves them over once, before
// anything reads them.
void MoveConfigFromOldName()
{
	char* RawPath = obs_module_config_path("");

	if(RawPath == nullptr)
	{
		ERROR_("Could not find the plugin's config folder, so settings from before the rename to OBS-VtuberBear were not moved.");
		return;
	}

	FilePath NewFolder = FilePath(reinterpret_cast<const char8_t*>(RawPath)).lexically_normal();
	bfree(RawPath);

	// OBS hands the folder back with a trailing separator
	if(!NewFolder.has_filename())
	{
		NewFolder = NewFolder.parent_path();
	}

	const FilePath OldFolder = NewFolder.parent_path() / u8"CharacterControlOBS";
	std::error_code Error;

	if(!std::filesystem::is_directory(OldFolder, Error) || std::filesystem::exists(NewFolder, Error))
	{
		return;
	}

	std::filesystem::rename(OldFolder, NewFolder, Error);
	const std::u8string OldText = OldFolder.u8string();
	const std::u8string NewText = NewFolder.u8string();

	if(Error)
	{
		ERROR_("%s: could not move it to %s (%s), so the Discord Voice settings start over.", reinterpret_cast<const char*>(OldText.c_str()),
			reinterpret_cast<const char*>(NewText.c_str()), Error.message().c_str());
		return;
	}

	INFO_("Moved the settings from %s to %s, the plugin's new name.", reinterpret_cast<const char*>(OldText.c_str()),
		reinterpret_cast<const char*>(NewText.c_str()));
}
}

// OBS looks this export up by name.
// ReSharper disable once CppInconsistentNaming
bool obs_module_load()
{
	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	MoveConfigFromOldName();

	obs_register_source(&SimpleDiscordCharacterSourceInfo);
	obs_register_source(&ScriptedDiscordCharacterSourceInfo);
	CharacterControlOBS::AddDiscordDock();

	return true;
}

// OBS looks this export up by name.
// ReSharper disable once CppInconsistentNaming
void obs_module_unload()
{
	CharacterControlOBS::ShutdownDiscord();
	blog(LOG_INFO, "plugin unloaded");
}
