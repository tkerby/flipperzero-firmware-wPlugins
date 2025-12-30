#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/number_input.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <nfc/nfc.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>

#define TONUINO_CARD_SIZE 16
#define TONUINO_BOX_ID_0 0x13
#define TONUINO_BOX_ID_1 0x37
#define TONUINO_BOX_ID_2 0xB3
#define TONUINO_BOX_ID_3 0x47
#define TONUINO_VERSION 0x02

#define APP_VERSION "1.0.0"
#define APP_BUILD_NUMBER 41

typedef enum {
    ModeHoerspiel = 1,
    ModeAlbum = 2,
    ModeParty = 3,
    ModeEinzel = 4,
    ModeHoerbuch = 5,
    ModeAdmin = 6,
    ModeHoerspielVB = 7,
    ModeAlbumVB = 8,
    ModePartyVB = 9,
    ModeHoerbuch1 = 10,
    ModeRepeatLast = 11,
} TonuinoMode;

typedef struct {
    uint8_t box_id[4];
    uint8_t version;
    uint8_t folder;
    uint8_t mode;
    uint8_t special1;
    uint8_t special2;
    uint8_t reserved[7];
} TonuinoCard;

typedef enum {
    TonuinoInputTypeFolder,
    TonuinoInputTypeSpecial1,
    TonuinoInputTypeSpecial2,
} TonuinoInputType;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    NumberInput* number_input;
    Widget* widget;
    TextBox* text_box;
    NotificationApp* notifications;
    
    TonuinoCard card_data;
    bool card_ready;
    TonuinoInputType current_input_type;
    char text_buffer[32];
    char read_text_buffer[512];
    bool rapid_write_mode_active;
    bool rapid_write_selected_folder; // true = folder selected, false = mode selected
    char rapid_write_status_message[32]; // Temporary status message (Success/Error)
    uint32_t rapid_write_status_timeout; // Timestamp when to clear the message
} TonuinoApp;

TonuinoApp* tonuino_app_alloc();
void tonuino_app_free(TonuinoApp* app);
void tonuino_build_card_data(TonuinoApp* app);
bool tonuino_write_card(TonuinoApp* app);
bool tonuino_read_card(TonuinoApp* app);

