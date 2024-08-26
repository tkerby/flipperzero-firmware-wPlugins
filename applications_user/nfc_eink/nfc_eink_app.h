#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <applications/services/dolphin/dolphin.h>
#include <applications/services/dolphin/helpers/dolphin_deed.h>

#include <assets_icons.h>

#include <../applications/services/notification/notification_messages.h>

#include <../applications/services/gui/gui.h>
#include <../applications/services/gui/view.h>
#include <../applications/services/gui/view_dispatcher.h>
#include <../applications/services/gui/scene_manager.h>
#include <../applications/services/gui/modules/submenu.h>
#include <../applications/services/gui/modules/empty_screen.h>
#include <../applications/services/gui/modules/dialog_ex.h>
#include <../applications/services/gui/modules/popup.h>
#include <../applications/services/gui/modules/loading.h>
#include <../applications/services/gui/modules/text_input.h>
#include <../applications/services/gui/modules/byte_input.h>
#include <../applications/services/gui/modules/text_box.h>
#include <../applications/services/gui/modules/widget.h>

#include <../applications/services/dialogs/dialogs.h>
#include <../applications/services/storage/storage.h>
//#include <application/services/dolphin/helpers/dolphin_deed.h>

/* #include <lib/nfc/nfc.h>
#include <lib/nfc/nfc_common.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/nfc_listener.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h> */
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>

#include "nfc_eink_screen/nfc_eink_screen.h"
#include "scenes/scenes.h"
#include "nfc_eink_tag.h"

#include "views/eink_progress.h"
//#define TAG "NfcEink"

#define NFC_EINK_NAME_SIZE     22
#define NFC_EINK_APP_EXTENSION ".eink"

#define NFC_EINK_APP_FOLDER ANY_PATH(TAG)

/* ///TODO: possibly move this closer to save/load functions
#define NFC_EINK_FORMAT_VERSION             (1)
#define NFC_EINK_FILE_HEADER                "Flipper NFC Eink screen"
#define NFC_EINK_DEVICE_UID_KEY             "UID"
#define NFC_EINK_DEVICE_TYPE_KEY            "NFC type"
#define NFC_EINK_SCREEN_TYPE_KEY            "Screen type"
#define NFC_EINK_SCREEN_NAME_KEY            "Screen name"
#define NFC_EINK_SCREEN_WIDTH_KEY           "Width"
#define NFC_EINK_SCREEN_HEIGHT_KEY          "Height"
#define NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY "Data block size"
#define NFC_EINK_SCREEN_DATA_TOTAL_KEY      "Data total"
#define NFC_EINK_SCREEN_DATA_READ_KEY       "Data read"
#define NFC_EINK_SCREEN_BLOCK_DATA_KEY      "Block" */

///TODO: remove all this shit below to _i.h file

typedef enum {
    NfcEinkAppCustomEventReserved = 100,

    NfcEinkAppCustomEventTextInputDone,
    NfcEinkAppCustomEventTimerExpired, ///TODO: remove this
    NfcEinkAppCustomEventProcessFinish,
    NfcEinkAppCustomEventTargetDetected,

} NfcEinkAppCustomEvents;

typedef enum {
    NfcEinkViewMenu,
    NfcEinkViewDialogEx,
    NfcEinkViewPopup,
    NfcEinkViewLoading,
    NfcEinkViewTextInput,
    NfcEinkViewByteInput,
    NfcEinkViewTextBox,
    NfcEinkViewWidget,
    NfcEinkViewProgress,
    NfcEinkViewEmptyScreen,
} NfcEinkView;

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // Common Views
    Submenu* submenu;
    DialogEx* dialog_ex;
    Popup* popup;
    Loading* loading;
    TextInput* text_input;
    ByteInput* byte_input;
    TextBox* text_box;
    Widget* widget;
    EmptyScreen* empty_screen;
    EinkProgress* eink_progress;
    View* view_image;

    Nfc* nfc;

    NfcListener* listener;
    NfcPoller* poller;
    NfcEinkScreen* screen;

    BitBuffer* tx_buf;
    char text_store[50 + 1];
    FuriString* file_path;
    FuriString* file_name;
    FuriTimer* timer;
} NfcEinkApp;

bool nfc_eink_load_from_file_select(NfcEinkApp* instance);
void nfc_eink_blink_emulate_start(NfcEinkApp* app);
void nfc_eink_blink_write_start(NfcEinkApp* app);
void nfc_eink_blink_stop(NfcEinkApp* app);
