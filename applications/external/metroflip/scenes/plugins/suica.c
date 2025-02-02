/*
 * Suica Scene
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../metroflip_i.h"
#include <flipper_application.h>

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>
#include <lib/nfc/protocols/felica/felica_poller_i.h>
#include <lib/nfc/helpers/felica_crc.h>
#include <lib/bit_lib/bit_lib.h>

#include <locale/locale.h>
#include <datetime/datetime.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define SERVICE_CODE_HISTORY_IN_LE  (0x090FU)
#define SERVICE_CODE_TAPS_LOG_IN_LE (0x108FU)
#define BLOCK_COUNT                 1
#define TAG                         "Metroflip:Scene:Suica"

static NfcCommand suica_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolFelica);
    NfcCommand command = NfcCommandContinue;
    MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    Metroflip* app = context;
    FuriString* parsed_data = furi_string_alloc();
    furi_string_reset(app->text_box_store);
    Widget* widget = app->widget;

    const uint16_t service_code[2] = {SERVICE_CODE_HISTORY_IN_LE, SERVICE_CODE_TAPS_LOG_IN_LE};

    const FelicaPollerEvent* felica_event = event.event_data;
    FelicaPollerReadCommandResponse* rx_resp;
    rx_resp->SF1 = 0;
    rx_resp->SF2 = 0;
    uint8_t blocks[1] = {0x00};
    FelicaPoller* felica_poller = event.instance;

    BitBuffer* tx_buffer = bit_buffer_alloc(FELICA_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(FELICA_POLLER_MAX_BUFFER_SIZE);
    if(felica_event->type == FelicaPollerEventTypeRequestAuthContext) {
        if(stage == MetroflipPollerEventTypeStart) {
            nfc_device_set_data(
                app->nfc_device, NfcProtocolFelica, nfc_poller_get_data(app->poller));
            furi_string_printf(parsed_data, "\e#Suica\n");

            FelicaError error;
            // Authenticate with the card
            // Iterate through the two services
            for(int service_code_index = 0; service_code_index < 2; service_code_index++) {
                rx_resp->SF1 = 0;
                rx_resp->SF2 = 0;
                blocks[0] = 0;
                furi_string_cat_printf(
                    parsed_data, "Service Code: %04X\n", service_code[service_code_index]);

                while((rx_resp->SF1 + rx_resp->SF2) == 0) {
                    error = felica_poller_read_blocks(
                        felica_poller, 1, blocks, service_code[service_code_index], &rx_resp);
                    blocks[0]++;
                    furi_string_cat_printf(parsed_data, "Block %02X\n", blocks[0]);
                    // FURI_LOG_I(TAG, "Received Response: %s", bit_buffer_get_data(rx_buffer));
                    for(size_t i = 0; i < FELICA_DATA_BLOCK_SIZE; i++) {
                        // FURI_LOG_I(TAG, "%02X", bit_buffer_get_byte((const BitBuffer*)rx_resp->data, i));
                        furi_string_cat_printf(parsed_data, "%02X", rx_resp->data[i]);
                    }
                    furi_string_cat_printf(parsed_data, "\n");
                    if(error != FelicaErrorNone) {
                        stage = MetroflipPollerEventTypeFail;
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, MetroflipCustomEventPollerFail);
                        break;
                    }
                }
            }
            metroflip_app_blink_stop(app);
            stage = (error == FelicaErrorNone) ? MetroflipPollerEventTypeSuccess :
                                                 MetroflipPollerEventTypeFail;

            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            furi_string_free(parsed_data);
        }
    }
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
    command = NfcCommandStop;
    return command;
}

static void suica_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
    nfc_scanner_alloc(app->nfc);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
    nfc_poller_start(app->poller, suica_poller_callback, app);

    metroflip_app_blink_start(app);
}

static bool suica_on_event(Metroflip* app, SceneManagerEvent event) {
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

static void suica_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    metroflip_app_blink_stop(app);
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin suica_plugin = {
    .card_name = "Suica",
    .plugin_on_enter = suica_on_enter,
    .plugin_on_event = suica_on_event,
    .plugin_on_exit = suica_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor suica_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &suica_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* suica_plugin_ep(void) {
    return &suica_plugin_descriptor;
}
