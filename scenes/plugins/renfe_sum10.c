#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:RenfeSum10"
static bool renfe_sum10_is_history_entry(const uint8_t* block_data);
static void renfe_sum10_parse_history_entry(FuriString* parsed_data, const uint8_t* block_data, int entry_num);
static void renfe_sum10_parse_travel_history(FuriString* parsed_data, const MfClassicData* data);
static uint32_t renfe_sum10_extract_timestamp(const uint8_t* block_data);
static void renfe_sum10_sort_history_entries(HistoryEntry* entries, int count);
// Keys for RENFE Suma 10 cards - specific keys found in real dumps
const MfClassicKeyPair renfe_sum10_keys[16] = {
    {.a = 0xA8844B0BCA06, .b = 0xffffffffffff}, // Sector 0 - RENFE specific key
    {.a = 0xCB5ED0E57B08, .b = 0xffffffffffff}, // Sector 1 - RENFE specific key 
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 2
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 3
    {.a = 0xC0C1C2C3C4C5, .b = 0xffffffffffff}, // Sector 4 - Alternative RENFE key
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 5 - Value blocks
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 6 - Value blocks
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 7
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 8
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 9
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 10
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 11
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 12
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 13
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 14
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, // Sector 15
};

// Alternative common keys for RENFE Suma 10
const MfClassicKeyPair renfe_sum10_alt_keys[16] = {
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 0
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 1
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 2
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 3
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 4
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 5 - Value blocks
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 6 - Value blocks
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 7
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 8
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 9
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 10
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 11
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 12
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 13
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 14
    {.a = 0xa0a1a2a3a4a5, .b = 0xa0a1a2a3a4a5}, // Sector 15
};

typedef struct {
    uint8_t data_sector;
    const MfClassicKeyPair* keys;
} RenfeSum10CardConfig;

static bool renfe_sum10_get_card_config(RenfeSum10CardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 5; // Primary data sector for trips
        config->keys = renfe_sum10_keys;
    } else if(type == MfClassicType4k) {
        // Mifare Plus 2K/4K configured as 1K for RENFE Suma 10 compatibility
        // Treat it exactly like a 1K card - only use the first 1K sectors
        config->data_sector = 5; // Same as 1K - RENFE only uses first 1K sectors
        config->keys = renfe_sum10_keys;
        FURI_LOG_I(TAG, "RENFE Suma 10: Treating Mifare Plus as 1K card for compatibility");
    } else {
        success = false; 
    }

    return success;
}

static bool renfe_sum10_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        // Verify card type - accepts both 1K and Plus (configured as 1K)
        RenfeSum10CardConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        if(!renfe_sum10_get_card_config(&cfg, data->type)) {
            FURI_LOG_E(TAG, "RENFE Suma 10: Unsupported card type %d", data->type);
            break;
        }

        furi_string_cat_printf(parsed_data, "\e#RENFE Suma 10\n");
        
        // Show card type info
        if(data->type == MfClassicType1k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Classic 1K\n");
        } else if(data->type == MfClassicType4k) {
            furi_string_cat_printf(parsed_data, "Type: Mifare Plus 2K (as 1K)\n");
        } else {
            furi_string_cat_printf(parsed_data, "Type: Unknown (%d)\n", data->type);
        }
        
        // Extract UID
        const uint8_t* uid = data->iso14443_3a_data->uid;
        size_t uid_len = data->iso14443_3a_data->uid_len;
        
        furi_string_cat_printf(parsed_data, "UID: ");
        for(size_t i = 0; i < uid_len; i++) {
            furi_string_cat_printf(parsed_data, "%02X", uid[i]);
            if(i < uid_len - 1) furi_string_cat_printf(parsed_data, ":");
        }
        furi_string_cat_printf(parsed_data, "\n");
        
        // Extract trips from Block 5 (based on real dump analysis)
        const uint8_t* block5 = data->block[5].data;
        if(block5[0] == 0x01 && block5[1] == 0x00 && block5[2] == 0x00 && block5[3] == 0x00) {
            // Extract trip count from byte 4
            int viajes = (int)block5[4];
            furi_string_cat_printf(parsed_data, "Trips: %d\n", viajes);
        } else {
            furi_string_cat_printf(parsed_data, "Trips: Not available\n");
        }
        
        parsed = true;
        
    } while(false);

    return parsed;
}

static bool renfe_sum10_checked = false;

static NfcCommand renfe_sum10_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        command = NfcCommandContinue;
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        app->sec_num = 0;
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;

    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicKey key = {0};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        bit_lib_num_to_bytes_be(renfe_sum10_keys[app->sec_num].a, COUNT_OF(key.data), key.data);
          if(!renfe_sum10_checked) {
            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            app->sec_num++;
            renfe_sum10_checked = true;
        }
        
        nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        
        if(mfc_data->type == MfClassicType1k) {
            bit_lib_num_to_bytes_be(renfe_sum10_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            if(app->sec_num == 16) {
                mfc_event->data->read_sector_request_data.key_provided = false;
                app->sec_num = 0;
            }
            app->sec_num++;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        FuriString* parsed_data = furi_string_alloc();
        Widget* widget = app->widget;
        
        if(!renfe_sum10_parse(parsed_data, mfc_data)) {
            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
        widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        metroflip_app_blink_stop(app);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void renfe_sum10_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!renfe_sum10_parse(parsed_data, mfc_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            mf_classic_free(mfc_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, renfe_sum10_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool renfe_sum10_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void renfe_sum10_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin renfe_sum10_plugin = {
    .card_name = "RENFE Suma 10",
    .plugin_on_enter = renfe_sum10_on_enter,
    .plugin_on_event = renfe_sum10_on_event,
    .plugin_on_exit = renfe_sum10_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor renfe_sum10_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &renfe_sum10_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* renfe_sum10_plugin_ep(void) {
    return &renfe_sum10_plugin_descriptor;
}
