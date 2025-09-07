#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/file_browser.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <furi_hal_infrared.h>
#include <firestring_icons.h>
#include <assets_icons.h>

#define TAG "firestring-app"

// scene index
typedef enum {
    FireStringScene_MainMenu,
    FireStringScene_Settings,
    FireStringScene_Generate,
    FireStringScene_GenerateStepTwo,
    FireStringScene_USB,
    FireStringScene_Save,
    FireStringScene_SavedPopup,
    // FireStringScene_AboutPopup,  // TODO
    FireStringScene_Load,
    FireStringScene_count
} FireStringScene;

// view index
typedef enum {
    FireStringView_Menu,
    FireStringView_VariableItemList,
    FireStringView_Widget,
    FireStringView_Popup,
    FireStringView_FileBrowser,
    FireStringView_TextInput
} FireStringView;

// custom event index
typedef enum {
    FireStringEvent_ShowVariableItemList,
    FireStringEvent_ShowStringGeneratorView,
    FireStringEvent_ShowBadUSBPopup,
    FireStringEvent_ShowFileBrowser,
    FireStringEvent_ShowSavedPopup,
    FireStringEvent_Exit
} FireStringEvent;

typedef enum {
    StrType_AlphaNumSymb,
    StrType_AlphaNum,
    StrType_Alpha,
    StrType_Symb,
    StrType_Num,
    StrType_Bin,
    // StrType_Passphrase,  // TODO - randomly pull from words_alpha.txt
} StrType;

// app settings
typedef struct {
    uint32_t str_len;
    uint8_t str_type;
    bool use_ir;
} FireStringSettings;

// app context
typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Menu* menu;
    Widget* widget;
    VariableItemList* variable_item_list;
    FileBrowser* file_browser;
    Popup* popup;
    InfraredWorker* ir_worker;
    FuriString* fire_string;
    FireStringSettings* settings;
} FireString;
