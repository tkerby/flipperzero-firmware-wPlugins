#include "../metroflip_i.h"

#include <dolphin/dolphin.h>

#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#define Metroflip_POLLER_MAX_BUFFER_SIZE 1024

#define TAG "Metroflip:Scene:RavKav"

uint8_t apdu_success[] = {0x90, 0x00};
uint8_t apdu_file_not_found[] = {0x6a, 0x82};

// balance
uint8_t select_balance_file[] = {0x94, 0xA4, 0x00, 0x00, 0x02, 0x20, 0x2A, 0x00};
uint8_t read_balance[] = {0x94, 0xb2, 0x01, 0x04, 0x1D};

static NfcCommand metroflip_scene_ravkav_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4b);
    NfcCommand next_command = NfcCommandContinue;
    MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    Metroflip* app = context;

    const Iso14443_4bPollerEvent* iso14443_4b_event = event.event_data;

    Iso14443_4bPoller* iso14443_4b_poller = event.instance;

    BitBuffer* tx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);

    if(iso14443_4b_event->type == Iso14443_4bPollerEventTypeReady) {
        if(stage == MetroflipPollerEventTypeStart) {
            nfc_device_set_data(
                app->nfc_device, NfcProtocolIso14443_4b, nfc_poller_get_data(app->poller));

            Iso14443_4bError error;
            size_t response_length = 0;

            do {
                // Select file of balance
                bit_buffer_append_bytes(
                    tx_buffer, select_balance_file, sizeof(select_balance_file));
                error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
                if(error != Iso14443_4bErrorNone) {
                    FURI_LOG_I(TAG, "Select File: iso14443_4b_poller_send_block error %d", error);
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                // Check the response after selecting file
                response_length = bit_buffer_get_size_bytes(rx_buffer);
                if(bit_buffer_get_byte(rx_buffer, response_length - 2) != apdu_success[0] ||
                   bit_buffer_get_byte(rx_buffer, response_length - 1) != apdu_success[1]) {
                    FURI_LOG_I(
                        TAG,
                        "Select file failed: %02x%02x",
                        bit_buffer_get_byte(rx_buffer, response_length - 2),
                        bit_buffer_get_byte(rx_buffer, response_length - 1));
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFileNotFound);
                    break;
                }

                // Now send the read command
                bit_buffer_reset(tx_buffer);
                bit_buffer_append_bytes(tx_buffer, read_balance, sizeof(read_balance));
                error = iso14443_4b_poller_send_block(iso14443_4b_poller, tx_buffer, rx_buffer);
                if(error != Iso14443_4bErrorNone) {
                    FURI_LOG_I(TAG, "Read File: iso14443_4b_poller_send_block error %d", error);
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                // Check the response after reading the file
                response_length = bit_buffer_get_size_bytes(rx_buffer);
                if(bit_buffer_get_byte(rx_buffer, response_length - 2) != apdu_success[0] ||
                   bit_buffer_get_byte(rx_buffer, response_length - 1) != apdu_success[1]) {
                    FURI_LOG_I(
                        TAG,
                        "Read file failed: %02x%02x",
                        bit_buffer_get_byte(rx_buffer, response_length - 2),
                        bit_buffer_get_byte(rx_buffer, response_length - 1));
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFileNotFound);
                    break;
                }

                // Process the response data
                if(response_length < 3) {
                    FURI_LOG_I(TAG, "Response too short: %d bytes", response_length);
                    stage = MetroflipPollerEventTypeFail;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    break;
                }

                uint32_t value = 0;
                for(uint8_t i = 0; i < 3; i++) {
                    value = (value << 8) | bit_buffer_get_byte(rx_buffer, i);
                }

                float result = value / 100.0f;
                app->value = result;
                strcpy(app->currency, "ILS");
                FURI_LOG_I(TAG, "Value: %.2f %s", (double)app->value, app->currency);

                strncpy(app->card_type, "Rav-Kav", sizeof(app->card_type));

                // Send success event
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MetroflipCustomEventPollerSuccess);

                stage = MetroflipPollerEventTypeSuccess;
                next_command = NfcCommandStop;
            } while(false);

            if(stage != MetroflipPollerEventTypeSuccess) {
                next_command = NfcCommandStop;
            }
        }
    }
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return next_command;
}

void metroflip_scene_ravkav_on_enter(void* context) {
    Metroflip* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
    nfc_scanner_alloc(app->nfc);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4b);
    nfc_poller_start(app->poller, metroflip_scene_ravkav_poller_callback, app);

    metroflip_app_blink_start(app);
}

bool metroflip_scene_ravkav_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            scene_manager_next_scene(app->scene_manager, MetroflipSceneReadSuccess);
            consumed = true;
        } else if(event.event == MetroflipPollerEventTypeCardDetect) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Scanning..", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFileNotFound) {
            Popup* popup = app->popup;
            popup_set_header(popup, "No\nRecord\nFile", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

void metroflip_scene_ravkav_on_exit(void* context) {
    Metroflip* app = context;

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
    metroflip_app_blink_stop(app);

    // Clear view
    popup_reset(app->popup);

    //metroflip_app_blink_stop(app);
}
