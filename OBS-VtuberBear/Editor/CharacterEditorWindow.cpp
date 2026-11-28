// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Editor/CharacterEditorWindow.h"
#include <QtWidgets/QWidget>

using namespace CharacterControlEditor;

CharacterEditorWindow::CharacterEditorWindow(QWidget* Parent) : QMainWindow(Parent)
{
	setWindowTitle("Character Editor");
	resize(1100, 700);
	setCentralWidget(new QWidget(this));
}
