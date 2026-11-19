// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include <obs-module.h>
#include "plugin-macros.h"
#include "Characters/CharacterSourceInfos.h"
#include "Discord/DiscordComponent.h"
#include "Discord/DiscordDock.h"

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// ReSharper disable once CppInconsistentNaming
bool obs_module_load()
{
	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	obs_register_source(&SimpleDiscordCharacterSourceInfo);
	obs_register_source(&ScriptedDiscordCharacterSourceInfo);
	CharacterControlOBS::AddDiscordDock();

	return true;
}

// ReSharper disable once CppInconsistentNaming
void obs_module_unload()
{
	CharacterControlOBS::ShutdownDiscord();
	blog(LOG_INFO, "plugin unloaded");
}
