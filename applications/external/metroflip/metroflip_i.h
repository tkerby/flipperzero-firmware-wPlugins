#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/modules/validators.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <metroflip_icons.h>
#include "api/nfc/mf_classic_key_cache.h"

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include "scenes/metroflip_scene.h"
#include <lib/flipper_format/flipper_format.h>
#include <toolbox/name_generator.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/nfc/helpers/nfc_data_generator.h>
#include <furi_hal_bt.h>
#include <notification/notification_messages.h>

#include <lib/nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <datetime.h>
#include <dolphin/dolphin.h>
#include <locale/locale.h>
#include <stdio.h>
#include <strings.h>
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>

#include "scenes/metroflip_scene.h"

#include "api/calypso/calypso_i.h"

#define KEY_MASK_BIT_CHECK(key_mask_1, key_mask_2) (((key_mask_1) & (key_mask_2)) == (key_mask_1))

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;
    ByteInput* byte_input;
    Popup* popup;
    uint8_t mac_buf[GAP_MAC_ADDR_SIZE];
    FuriString* text_box_store;
    Widget* widget;

    Nfc* nfc;
    NfcPoller* poller;
    NfcScanner* scanner;
    NfcDevice* nfc_device;
    MfClassicKeyCache* mfc_key_cache;

    // card details:
    uint32_t balance_lari;
    uint8_t balance_tetri;
    uint32_t card_number;
    size_t sec_num;
    float value;
    char currency[4];
    char card_type[32];

    // Calypso specific context
    CalypsoContext* calypso_context;
} Metroflip;

enum MetroflipCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    MetroflipCustomEventReserved = 100,

    MetroflipCustomEventViewExit,
    MetroflipCustomEventByteInputDone,
    MetroflipCustomEventTextInputDone,
    MetroflipCustomEventWorkerExit,

    MetroflipCustomEventPollerDetect,
    MetroflipCustomEventPollerSuccess,
    MetroflipCustomEventPollerFail,
    MetroflipCustomEventPollerSelectFailed,
    MetroflipCustomEventPollerFileNotFound,

    MetroflipCustomEventCardLost,
    MetroflipCustomEventCardDetected,
    MetroflipCustomEventWrongCard
};

typedef enum {
    MetroflipPollerEventTypeStart,
    MetroflipPollerEventTypeCardDetect,

    MetroflipPollerEventTypeSuccess,
    MetroflipPollerEventTypeFail,
} MetroflipPollerEventType;

typedef enum {
    MetroflipViewSubmenu,
    MetroflipViewTextInput,
    MetroflipViewByteInput,
    MetroflipViewPopup,
    MetroflipViewMenu,
    MetroflipViewLoading,
    MetroflipViewTextBox,
    MetroflipViewWidget,
    MetroflipViewUart,
} MetroflipView;

typedef enum {
    SUCCESSFUL,
    INCOMPLETE_KEYFILE,
    MISSING_KEYFILE
} KeyfileManager;

KeyfileManager manage_keyfiles(
    char uid_str[],
    const uint8_t* uid,
    size_t uid_len,
    MfClassicKeyCache* instance,
    uint64_t key_mask_a_required,
    uint64_t key_mask_b_required);

void metroflip_app_blink_start(Metroflip* metroflip);
void metroflip_app_blink_stop(Metroflip* metroflip);

#ifdef FW_ORIGIN_Official
#define submenu_add_lockable_item(                                             \
    submenu, label, index, callback, callback_context, locked, locked_message) \
    if(!(locked)) submenu_add_item(submenu, label, index, callback, callback_context)
#endif

void metroflip_exit_widget_callback(GuiButtonType result, InputType type, void* context);

void uid_to_string(const uint8_t* uid, size_t uid_len, char* uid_str, size_t max_len);

void handle_keyfile_case(
    Metroflip* app,
    const char* message_title,
    const char* log_message,
    FuriString* parsed_data,
    char card_type[]);

char* bit_slice(const char* bit_representation, int start, int end);

///////////////////////////////// Calypso / EN1545 /////////////////////////////////

#define Metroflip_POLLER_MAX_BUFFER_SIZE 1024

#define epoch 852073200

void locale_format_datetime_cat(FuriString* out, const DateTime* dt, bool time);

extern uint8_t read_file[5];
extern uint8_t apdu_success[2];
extern uint8_t select_app[8];

void byte_to_binary(uint8_t byte, char* bits);

int binary_to_decimal(const char binary[]);

int bit_slice_to_dec(const char* bit_representation, int start, int end);

void dec_to_bits(char dec_representation, char* bit_representation);
