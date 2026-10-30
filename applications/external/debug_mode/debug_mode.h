#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>

#include <storage/storage.h>
#include <nfc/nfc.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "scenes/debug_mode_scene.h"
#include "helpers/case_store.h"

typedef enum {
    DebugModeViewIdSplash,
    DebugModeViewIdSubmenu,
    DebugModeViewIdTextInput,
    DebugModeViewIdWidget,
    DebugModeViewIdDialogEx,
} DebugModeViewId;

typedef enum {
    DebugModeCustomEventSplashStart = 0x1000,
    DebugModeCustomEventSplashTick,
    DebugModeCustomEventNfcFound,
    DebugModeCustomEventNfcTimeout,
    DebugModeCustomEventNfcScanTick,
} DebugModeCustomEvent;

typedef enum {
    DebugModeCaseListSortUpdated,
    DebugModeCaseListSortPriority,
    DebugModeCaseListSortCategory,
    DebugModeCaseListSortTitle,
} DebugModeCaseListSort;

typedef enum {
    DebugModeCaseActionNone,
    DebugModeCaseActionArchive,
    DebugModeCaseActionDelete,
} DebugModeCaseAction;

typedef enum {
    DebugModeNfcModeOpen,
    DebugModeNfcModeLink,
} DebugModeNfcMode;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Storage* storage;
    Nfc* nfc;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    DialogEx* dialog_ex;
    View* splash_view;
    FuriTimer* splash_timer;
    FuriTimer* nfc_scan_timer;
    void* nfc_worker_context;

    char message_title[40];
    char message_body[DEBUG_MODE_MAX_TEXT_LEN + 1];
    DebugModeScene message_return_scene;

    uint32_t current_case_id;
    char current_case_title[DEBUG_MODE_MAX_TITLE_LEN + 1];
    char current_case_category[16];
    char current_case_status[16];
    char current_case_priority[16];

    char pending_entry_type[16];
    char pending_entry_text[DEBUG_MODE_MAX_TEXT_LEN + 1];
    char pending_entry_outcome[16];
    char pending_entry_note[DEBUG_MODE_MAX_TEXT_LEN + 1];
    char pending_root_cause[24];
    uint32_t pending_template_id;
    DebugModeCaseAction pending_case_action;
    DebugModeNfcMode nfc_mode;
    volatile bool nfc_scan_active;
    bool nfc_scan_found;
    uint8_t nfc_scan_frame;
    char nfc_uid[32];

    bool case_list_show_resolved;
    bool case_list_show_archived;
    DebugModeCaseListSort case_list_sort;
    uint32_t case_list_ids[DEBUG_MODE_MAX_CASES];
    uint32_t case_list_count;
} DebugMode;

void debug_mode_save_pending_entry_and_return(DebugMode* app);

View* debug_mode_splash_view_alloc(void* context);
void debug_mode_splash_view_free(View* view);
void debug_mode_splash_view_tick(View* view);
