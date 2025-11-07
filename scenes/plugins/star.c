
#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/st25tb/st25tb_poller.h>
#include <nfc/protocols/st25tb/st25tb.h>

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"
#include <machine/endian.h>

#define TAG "Metroflip:Scene:Star"

uint8_t st25tb_get_block_byte(const St25tbData* data, size_t block_index, uint8_t byte_index) {
    if(block_index >= st25tb_get_block_count(data->type) || byte_index > 3) return 0;
    uint32_t block = __bswap32(data->blocks[block_index]);
    return (block >> (8 * (3 - byte_index))) & 0xFF;
}


static bool star_parse(FuriString* parsed_data, const St25tbData* data) {
    bool parsed = false;

    do {
        //do some check here later..
        size_t uid_len = 0;
        const uint8_t* uid = st25tb_get_uid(data, &uid_len);
        uint32_t card_number = bit_lib_bytes_to_num_le(uid, 4);
        size_t remaining_trips_block = 5;
        uint8_t r0 = st25tb_get_block_byte(data, remaining_trips_block, 0);
        uint8_t r1 = st25tb_get_block_byte(data, remaining_trips_block, 1);
        uint8_t r2 = st25tb_get_block_byte(data, remaining_trips_block, 2);

        // reverse order: r2 r1 r0
        uint32_t remaining_total = (r2 << 16) | (r1 << 8) | r0;

        furi_string_printf(
            parsed_data,
            "\e#Star\nUID: %lu\nRemaining Trips: %ld",
            card_number,
            remaining_total);
        parsed = true;
    } while(false);

    return parsed;
}


static NfcCommand star_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolSt25tb);

    Metroflip* app = context;
    const St25tbPollerEvent* st25tb_event = event.event_data;

    if(st25tb_event->type == St25tbPollerEventTypeRequestMode) {
        st25tb_event->data->mode_request.mode = St25tbPollerModeRead;
    } else if(st25tb_event->type == St25tbPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolSt25tb, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void star_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "Star loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            St25tbData* st25tb_data = st25tb_alloc();
            st25tb_load(st25tb_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!star_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            st25tb_free(st25tb_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        FURI_LOG_I(TAG, "Star not loaded");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolSt25tb);
        nfc_poller_start(app->poller, star_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool star_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            const St25tbData* st25tb_data = nfc_device_get_data(app->nfc_device, NfcProtocolSt25tb);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!star_parse(parsed_data, st25tb_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
            widget_add_button_element(
                widget, GuiButtonTypeLeft, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Save", metroflip_save_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            metroflip_app_blink_stop(app);
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

static void star_on_exit(Metroflip* app) {
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
static const MetroflipPlugin star_plugin = {
    .card_name = "Star",
    .plugin_on_enter = star_on_enter,
    .plugin_on_event = star_on_event,
    .plugin_on_exit = star_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor star_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &star_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* star_plugin_ep(void) {
    return &star_plugin_descriptor;
}
