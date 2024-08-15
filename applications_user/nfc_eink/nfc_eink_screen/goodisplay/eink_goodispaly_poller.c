#include "eink_goodisplay_i.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller_i.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define EINK_SCREEN_ISO14443_POLLER_FWT_FC (60000)

typedef NfcCommand (
    *NfcEinkGoodisplayStateHandler)(Iso14443_3aPoller* poller, NfcEinkScreen* screen);

typedef enum {
    //Idle,
    SendC2Cmd,
    SelectNDEFTagApplication,
    SelectNDEFFile,
    ReadFIDFileData,

    NfcEinkScreenGoodisplayPollerStateError,
    NfcEinkScreenGoodisplayPollerStateNum
} NfcEinkScreenGoodisplayPollerState;

static NfcEinkScreenGoodisplayPollerState state = SendC2Cmd;

/* static NfcCommand eink_goodisplay_idle(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Idle");
    //state =

    return NfcCommandContinue;
} */

static bool eink_goodisplay_send_data(
    Iso14443_3aPoller* poller,
    const NfcEinkScreen* screen,
    const uint8_t* data,
    const size_t data_len,
    const uint8_t** response,
    size_t* response_len) {
    furi_assert(poller);
    furi_assert(screen);
    furi_assert(data);
    furi_assert(data_len > 0);
    furi_assert(response);
    furi_assert(response_len);

    bit_buffer_reset(screen->tx_buf);
    bit_buffer_reset(screen->rx_buf);

    bit_buffer_append_bytes(screen->tx_buf, data, data_len);

    Iso14443_3aError error = iso14443_3a_poller_send_standard_frame(
        poller, screen->tx_buf, screen->rx_buf, EINK_SCREEN_ISO14443_POLLER_FWT_FC);

    bool result = false;
    if(error == Iso14443_3aErrorNone) {
        *response_len = bit_buffer_get_size_bytes(screen->rx_buf);
        *response = bit_buffer_get_data(screen->rx_buf);

        result = true;
    } else {
        *response = NULL;
        *response_len = 0;
        FURI_LOG_E(TAG, "Iso14443_3aError: %02X", error);
    }
    return result;
}

static NfcCommand eink_goodisplay_C2(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    FURI_LOG_D(TAG, "Send 0xC2");

    state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0xC2};
        const uint8_t* response = NULL;
        size_t response_len = 0;
        bool result = eink_goodisplay_send_data(
            poller, screen, data, sizeof(data), &response, &response_len);

        if(!result) break;
        if((response_len > 1) || (response[0] != 0xC2)) break;

        state = SelectNDEFTagApplication;
    } while(false);

    return NfcCommandReset;
}

static NfcCommand
    eink_goodisplay_select_application(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Select NDEF APP");

    state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {
            0x02, 0x00, 0xa4, 0x04, 0x00, 0x07, 0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02 || rx[1] != 0x90 || rx[2] != 0x00) break;

        state = SelectNDEFFile;
    } while(false);
    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_select_ndef_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Select NDEF File");

    state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0x03, 0x00, 0xa4, 0x00, 0x0c, 0x02, 0xe1, 0x03};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x03 || rx[1] != 0x90 || rx[2] != 0x00) break;

        state = ReadFIDFileData;
    } while(false);

    return NfcCommandContinue;
}

static NfcCommand eink_goodisplay_read_fid_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Read FID");
    state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0x02, 0x00, 0xb0, 0x00, 0x00, 0x0f};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02) break;

        for(uint8_t i = 0; i < rx_len; i++) {
            FURI_LOG_D(TAG, "%d = 0x%02X", i, rx[i]);
        }
        state = NfcEinkScreenGoodisplayPollerStateError;
    } while(false);

    return NfcCommandContinue;
}

static NfcCommand eink_goodisplay_error_handler(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_E(TAG, "ERROR!");
    return NfcCommandStop;
}

static NfcEinkGoodisplayStateHandler handlers[NfcEinkScreenGoodisplayPollerStateNum] = {
    // [Idle] = eink_goodisplay_idle,
    [SendC2Cmd] = eink_goodisplay_C2,
    [SelectNDEFTagApplication] = eink_goodisplay_select_application,
    [SelectNDEFFile] = eink_goodisplay_select_ndef_file,
    [ReadFIDFileData] = eink_goodisplay_read_fid_file,
    [NfcEinkScreenGoodisplayPollerStateError] = eink_goodisplay_error_handler,
};

NfcCommand eink_goodisplay_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    NfcEinkScreen* screen = context;

    NfcCommand command = NfcCommandContinue;

    Iso14443_4aPollerEvent* Iso14443_4a_event = event.event_data;
    Iso14443_4aPoller* poller = event.instance;

    if(Iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        bit_buffer_reset(screen->tx_buf);
        bit_buffer_reset(screen->rx_buf);

        command = handlers[state](poller->iso14443_3a_poller, screen);
    }
    if(command == NfcCommandReset) iso14443_4a_poller_halt(poller);
    return command;
}
