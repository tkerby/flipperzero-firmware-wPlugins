#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <applications/services/gui/gui.h>
#include <applications/services/dolphin/dolphin.h>
#include <applications/services/dolphin/helpers/dolphin_deed.h>

#include <gui/view.h>
#include <assets_icons.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>

#include <gui/modules/submenu.h>
#include <gui/modules/empty_screen.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
//#include <application/services/dolphin/helpers/dolphin_deed.h>

/* #include <lib/nfc/nfc.h>
#include <lib/nfc/nfc_common.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/nfc_listener.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h> */
#include <nfc/nfc_listener.h>

#include "nfc_eink_screen/nfc_eink_screen.h"
#include "scenes/scenes.h"

#define TAG "NfcEink"

#define NFC_EINK_NAME_SIZE 22
#define NFC_EINK_APP_EXTENSION ".eink"

#define NFC_EINK_APP_FOLDER ANY_PATH(TAG)

///TODO: possibly move this closer to save/load functions
#define NFC_EINK_FORMAT_VERSION (1)
#define NFC_EINK_FILE_HEADER "Flipper NFC Eink screen"
#define NFC_EINK_DEVICE_UID_KEY "UID"
#define NFC_EINK_DEVICE_TYPE_KEY "NFC type"
#define NFC_EINK_SCREEN_TYPE_KEY "Screen type"
#define NFC_EINK_SCREEN_NAME_KEY "Screen name"
#define NFC_EINK_SCREEN_WIDTH_KEY "Width"
#define NFC_EINK_SCREEN_HEIGHT_KEY "Height"
#define NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY "Data block size"
#define NFC_EINK_SCREEN_DATA_TOTAL_KEY "Data total"
#define NFC_EINK_SCREEN_DATA_READ_KEY "Data read"
#define NFC_EINK_SCREEN_BLOCK_DATA_KEY "Block"

typedef enum {
    NfcEinkCustomEventReserved = 100,

    NfcEinkCustomEventTextInputDone,
    NfcEinkCustomEventTimerExpired,

} NfcEinkCustomEvents;

typedef enum {
    NfcEinkViewMenu,
    NfcEinkViewDialogEx,
    NfcEinkViewPopup,
    NfcEinkViewLoading,
    NfcEinkViewTextInput,
    NfcEinkViewByteInput,
    NfcEinkViewTextBox,
    NfcEinkViewWidget,
    NfcEinkViewEmptyScreen,
} NfcEinkView;

typedef struct {
    Storage* storage;
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
    View* view_image;

    Nfc* nfc;

    NfcListener* listener;
    NfcEinkScreen* screen;

    BitBuffer* tx_buf;
    char text_store[50 + 1];
    FuriString* file_path;
    FuriString* file_name;
    bool was_update;
    uint8_t update_cnt;
    FuriTimer* timer;
} NfcEinkApp;

///TODO: Move this to private _i.h file
bool nfc_eink_screen_delete(const char* file_path);
bool nfc_eink_screen_load(const char* file_path, NfcEinkScreen** screen);
bool nfc_eink_screen_save(const NfcEinkScreen* screen, const char* file_path);