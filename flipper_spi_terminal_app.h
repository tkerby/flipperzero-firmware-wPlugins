#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    DialogEx* mainScreen;
    VariableItemList* configScreen;
    TextBox* terminalScreen;
} FlipperSPITerminalApp;
