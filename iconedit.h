#pragma once

#include <gui/gui.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include "icon.h"

#define TAG     "IE"
#define VERSION "0.2"

typedef struct {
    InputEvent input;
} IconEditEvent;

// Panel determines what should process input
typedef enum {
    // main view components
    Panel_TabBar, // File, Tools, View, Anim
    // Panel_Tab,
    Panel_Canvas,

    Panel_File,
    Panel_Tools,
    Panel_Settings,
    Panel_About,

    // modal panels
    Panel_Playback,
    Panel_SaveAs, // PNG, XBM, .C
    Panel_New, // dimension prompt
    Panel_FPS, // select FPS
    Panel_SendUSB,
    Panel_SendBT, // can this be the same as USB?
    Panel_Dialog,
} PanelType;

typedef void (*IconEditUpdateCallback)(void* context);

typedef enum {
    Setting_START,
    Setting_Include = Setting_START,
    Setting_Delete_Brace,
    Setting_COUNT,
    Setting_NONE,
} SettingType;
typedef struct {
    bool include_icon_header;
    bool delete_auto_brace;
} IESettings;

typedef struct {
    IEIcon* icon;

    PanelType panel; // the active panel currently reacting to input
    IESettings settings;

    FuriMutex* mutex;
    Storage* storage;
    NotificationApp* notify;
    FuriString* temp_str;
    char temp_cstr[64];
    bool running;
    bool dirty;
} IconEdit;
