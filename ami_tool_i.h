#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/scene_manager.h>

typedef struct AmiToolApp AmiToolApp;

/* View IDs for ViewDispatcher */
typedef enum {
    AmiToolViewMenu,
    AmiToolViewTextBox,
} AmiToolView;

/* Scene IDs */
typedef enum {
    AmiToolSceneMainMenu,
    AmiToolSceneRead,
    AmiToolSceneGenerate,
    AmiToolSceneSaved,
    AmiToolSceneCount,
} AmiToolScene;

/* Custom events from main menu */
typedef enum {
    AmiToolEventMainMenuRead,
    AmiToolEventMainMenuGenerate,
    AmiToolEventMainMenuSaved,
    AmiToolEventMainMenuExit,
} AmiToolCustomEvent;

struct AmiToolApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;

    TextBox* text_box;
    FuriString* text_box_store;
};

/* Scene handlers table */
extern const SceneManagerHandlers ami_tool_scene_handlers;

/* Allocation / free */
AmiToolApp* ami_tool_alloc(void);
void ami_tool_free(AmiToolApp* app);
