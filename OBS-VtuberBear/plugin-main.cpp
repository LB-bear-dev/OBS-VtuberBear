// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include <obs-module.h>
#include "plugin-macros.h"
#include "Editor/CharacterEditor.h"

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// ReSharper disable once CppInconsistentNaming
bool obs_module_load()
{
	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	CharacterControlEditor::RegisterCharacterEditor();

	return true;
}

// ReSharper disable once CppInconsistentNaming
void obs_module_unload()
{
	CharacterControlEditor::UnregisterCharacterEditor();

	blog(LOG_INFO, "plugin unloaded");
}
