// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Editor/CharacterEditor.h"
#include <QtCore/QPointer>
#include <obs-frontend-api.h>
#include "Editor/CharacterEditorWindow.h"

using namespace CharacterControlEditor;

namespace
{
QPointer<CharacterEditorWindow> EditorWindow;

void DestroyEditorWindow()
{
	if(!EditorWindow.isNull())
	{
		delete EditorWindow.data();
	}
}

void OpenEditorWindow(void*)
{
	// If we don't already have a window, make one parented to the main OBS window
	if(EditorWindow.isNull())
	{
		const auto MainOBSWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
		EditorWindow = new CharacterEditorWindow(MainOBSWindow);
	}

	EditorWindow->show();
	EditorWindow->raise();
	EditorWindow->activateWindow();
}

void OnFrontendEvent(obs_frontend_event Event, void*)
{
	if(Event == OBS_FRONTEND_EVENT_EXIT)
	{
		DestroyEditorWindow();
	}
}
}

void CharacterControlEditor::RegisterCharacterEditor()
{
	obs_frontend_add_tools_menu_item("Character Editor", OpenEditorWindow, nullptr);
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
}

void CharacterControlEditor::UnregisterCharacterEditor()
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	DestroyEditorWindow();
}
