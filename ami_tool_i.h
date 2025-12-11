#pragma once

#include <stddef.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/scene_manager.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <furi/core/thread.h>

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
    AmiToolEventReadSuccess,
    AmiToolEventReadWrongType,
    AmiToolEventReadError,
} AmiToolCustomEvent;

typedef enum {
    AmiToolReadResultNone,
    AmiToolReadResultSuccess,
    AmiToolReadResultWrongType,
    AmiToolReadResultError,
} AmiToolReadResultType;

typedef struct {
    AmiToolReadResultType type;
    MfUltralightError error;
    MfUltralightType tag_type;
    size_t uid_len;
    uint8_t uid[10];
} AmiToolReadResult;

struct AmiToolApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;

    TextBox* text_box;
    FuriString* text_box_store;

    Nfc* nfc;
    FuriThread* read_thread;
    bool read_scene_active;
    AmiToolReadResult read_result;
    MfUltralightData* tag_data;
    bool tag_data_valid;
};

/* Scene handlers table */
extern const SceneManagerHandlers ami_tool_scene_handlers;

/* Allocation / free */
AmiToolApp* ami_tool_alloc(void);
void ami_tool_free(AmiToolApp* app);
