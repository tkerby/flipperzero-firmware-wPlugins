#include "eink_goodisplay_i.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller_i.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define EINK_SCREEN_ISO14443_POLLER_FWT_FC (60000) //(90000)

typedef NfcCommand (
    *NfcEinkGoodisplayStateHandler)(Iso14443_3aPoller* poller, NfcEinkScreen* screen);

//static NfcEinkScreenGoodisplayPollerState state = SendC2Cmd;

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
        poller, screen->tx_buf, screen->rx_buf, 0 /* EINK_SCREEN_ISO14443_POLLER_FWT_FC */);

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

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0xC2};
        const uint8_t* response = NULL;
        size_t response_len = 0;
        bool result = eink_goodisplay_send_data(
            poller, screen, data, sizeof(data), &response, &response_len);

        if(!result) break;
        if((response_len > 1) || (response[0] != 0xC2)) break;

        ctx->state = SelectNDEFTagApplication;
    } while(false);

    return NfcCommandReset;
}

static NfcCommand
    eink_goodisplay_select_application(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Select NDEF APP");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {
            0x02, 0x00, 0xa4, 0x04, 0x00, 0x07, 0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02 || rx[1] != 0x90 || rx[2] != 0x00) break;

        ctx->state = SelectNDEFFile;
    } while(false);
    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_select_ndef_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Select NDEF File");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0x03, 0x00, 0xa4, 0x00, 0x0c, 0x02, 0xe1, 0x03};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x03 || rx[1] != 0x90 || rx[2] != 0x00) break;

        ctx->state = ReadFIDFileData;
    } while(false);

    return NfcCommandContinue;
}

static NfcCommand eink_goodisplay_read_fid_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Read FID");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
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

        ctx->state = Select0xE104File;
    } while(false);

    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_select_E104_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    FURI_LOG_D(TAG, "Select 0xE104");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0x03, 0x00, 0xa4, 0x00, 0x0c, 0x02, 0xe1, 0x04};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x03 || rx[1] != 0x90 || rx[2] != 0x00) break;

        ctx->state = Read0xE104FileData;
    } while(false);
    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_read_0xE104_file(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Read 0xE104");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t data[] = {0x02, 0x00, 0xb0, 0x00, 0x00, 0x02};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, data, sizeof(data), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02) break;

        for(uint8_t i = 0; i < rx_len; i++) {
            FURI_LOG_D(TAG, "%d = 0x%02X", i, rx[i]);
        }

        ctx->state = DuplicateC2Cmd;
    } while(false);

    return NfcCommandContinue;
}

static NfcCommand eink_goodisplay_duplicate_C2(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    NfcCommand cmd = eink_goodisplay_C2(poller, screen);

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = DuplicateC2Cmd;

    static uint8_t cnt = 0;
    cnt++;
    if(cnt == 2) {
        ctx->state = SendConfigCmd;
        cnt = 0;
    }

    return cmd;
}

static NfcCommand
    eink_goodisplay_send_config_cmd(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    FURI_LOG_D(TAG, "Send config");

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;
    do {
        const uint8_t config1[] = {0x02, 0xf0, 0xdb, 0x02, 0x00, 0x00};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result =
            eink_goodisplay_send_data(poller, screen, config1, sizeof(config1), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02 || rx[1] != 0x90 || rx[2] != 0x00) break;

        const uint8_t config2[] = {
            0x03, 0xf0, 0xdb, 0x00, 0x00, 0x67, 0xa0, 0x06, 0x00, 0x20, 0x00, 0x7a, 0x00, 0xfa,
            0xa4, 0x01, 0x0c, 0xa5, 0x02, 0x00, 0x0a, 0xa4, 0x01, 0x08, 0xa5, 0x02, 0x00, 0x0a,
            0xa4, 0x01, 0x0c, 0xa5, 0x02, 0x00, 0x0a, 0xa4, 0x01, 0x02, 0xa1, 0x01, 0x12, 0xa4,
            0x01, 0x02, 0xa1, 0x04, 0x01, 0x27, 0x01, 0x01, 0xa1, 0x02, 0x11, 0x01, 0xa1, 0x03,
            0x44, 0x00, 0x0f, 0xa1, 0x05, 0x45, 0x27, 0x01, 0x00, 0x00, 0xa1, 0x02, 0x3c, 0x05,
            0xa1, 0x03, 0x21, 0x00, 0x80, 0xa1, 0x02, 0x18, 0x80, 0xa1, 0x02, 0x4e, 0x00, 0xa1,
            0x03, 0x4f, 0x27, 0x01, 0xa3, 0x01, 0x24, 0xa2, 0x02, 0x22, 0xf7, 0xa2, 0x01, 0x20,
            0xa4, 0x01, 0x02, 0xa2, 0x02, 0x10, 0x01, 0xa5, 0x02, 0x00, 0x0a};

        result = eink_goodisplay_send_data(poller, screen, config2, sizeof(config2), &rx, &rx_len);
        if(!result) break;
        if(rx[0] != 0x03 || rx[1] != 0x90 || rx[2] != 0x00) break;

        const uint8_t config3[] = {0x02, 0xf0, 0xda, 0x00, 0x00, 0x03, 0xf0, 0x00, 0x20};

        result = eink_goodisplay_send_data(poller, screen, config3, sizeof(config3), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0x02 || rx[1] != 0x90 || rx[2] != 0x00) break;

        const uint8_t cmdb3[] = {0xb3};

        result = eink_goodisplay_send_data(poller, screen, cmdb3, sizeof(cmdb3), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0xA2) break;

        ctx->state = SendDataCmd;

    } while(false);

    return NfcCommandContinue;
}

static bool eink_goodisplay_send_image_data_p1(
    Iso14443_3aPoller* poller,
    const NfcEinkScreen* screen,
    const uint8_t block_number,
    const uint8_t* data,
    const size_t data_len) {
    furi_assert(poller);
    furi_assert(screen);

    bit_buffer_reset(screen->tx_buf);
    bit_buffer_reset(screen->rx_buf);

    uint8_t header[] = {0x13, 0xf0, 0xd2, 0x00, 0x00, 0xFA};
    header[4] = block_number;
    bit_buffer_append_bytes(screen->tx_buf, header, sizeof(header));
    bit_buffer_append_bytes(screen->tx_buf, data, data_len);

    Iso14443_3aError error =
        iso14443_3a_poller_send_standard_frame(poller, screen->tx_buf, screen->rx_buf, 0);

    bool result = false;
    if(error == Iso14443_3aErrorNone) {
        const uint8_t* response = bit_buffer_get_data(screen->rx_buf);
        result = response[0] == 0xA3;
    } else {
        FURI_LOG_E(TAG, "Iso14443_3aError: %02X", error);
    }
    return result;
}

static bool eink_goodisplay_send_image_data_p2(
    Iso14443_3aPoller* poller,
    const NfcEinkScreen* screen,
    const uint8_t* data,
    const size_t data_len) {
    furi_assert(poller);
    furi_assert(screen);

    bit_buffer_reset(screen->tx_buf);
    bit_buffer_reset(screen->rx_buf);

    const uint8_t header[] = {0x02};
    bit_buffer_append_bytes(screen->tx_buf, header, sizeof(header));
    bit_buffer_append_bytes(screen->tx_buf, data, data_len);

    Iso14443_3aError error =
        iso14443_3a_poller_send_standard_frame(poller, screen->tx_buf, screen->rx_buf, 0);

    bool result = false;
    if(error == Iso14443_3aErrorNone) {
        const uint8_t* rx = bit_buffer_get_data(screen->rx_buf);
        if(rx[0] != 0x02 || rx[1] != 0x90 || rx[2] != 0x00)
            result = false;
        else
            result = true;
    } else {
        FURI_LOG_E(TAG, "Iso14443_3aError: %02X", error);
    }
    return result;
}

static NfcCommand
    eink_goodisplay_send_image_data(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    FURI_LOG_D(TAG, "Send data");

    const NfcEinkScreenData* data = screen->data;
    bool status = true;
    uint8_t block_number = 0;
    for(size_t i = 0; i < data->received_data; block_number++) {
        status &= eink_goodisplay_send_image_data_p1(
            poller, screen, block_number, &data->image_data[i], data->base.data_block_size - 2);
        i += data->base.data_block_size - 2;

        if(!status) break;

        status &= eink_goodisplay_send_image_data_p2(poller, screen, &data->image_data[i], 2);
        i += 2;

        if(!status) break;
    }

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = status ? UpdateDisplay : NfcEinkScreenGoodisplayPollerStateError;
    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_update_display(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
    ctx->state = NfcEinkScreenGoodisplayPollerStateError;

    const uint8_t apply[] = {0x03, 0xf0, 0xd4, 0x05, 0x80, 0x00};
    const uint8_t* rx = NULL;
    size_t rx_len = 0;
    bool result = eink_goodisplay_send_data(poller, screen, apply, sizeof(apply), &rx, &rx_len);

    if(result) {
        do {
            furi_delay_ms(1000);

            if(rx[0] == 0xf2 && rx[1] == 0x01) {
                FURI_LOG_D(TAG, "Updating...");
            } else if(rx[0] == 0x03 && rx[1] == 0x90 && rx[2] == 0x00) {
                furi_delay_ms(1000);
                //furi_delay_ms(500);
                break;
            }

            const uint8_t update[] = {0xF2, 0x01};
            result =
                eink_goodisplay_send_data(poller, screen, update, sizeof(update), &rx, &rx_len);
            if(!result) break;
        } while(true);
    }
    ctx->state = result ? SendDataDone : NfcEinkScreenGoodisplayPollerStateError;
    return NfcCommandContinue;
}

static NfcCommand
    eink_goodisplay_finish_handler(Iso14443_3aPoller* poller, NfcEinkScreen* screen) {
    UNUSED(poller);
    UNUSED(screen);
    uint8_t cnt = 0;
    do {
        const uint8_t b2[] = {0xb2};
        const uint8_t* rx = NULL;
        size_t rx_len = 0;
        bool result = eink_goodisplay_send_data(poller, screen, b2, sizeof(b2), &rx, &rx_len);

        if(!result) break;
        if(rx[0] != 0xA3) break;
        cnt++;
    } while(cnt < 20);
    FURI_LOG_D(TAG, "Done!");
    eink_goodisplay_on_done(screen);
    iso14443_3a_poller_halt(poller);
    return NfcCommandStop;
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
    [Select0xE104File] = eink_goodisplay_select_E104_file,
    [Read0xE104FileData] = eink_goodisplay_read_0xE104_file,
    [DuplicateC2Cmd] = eink_goodisplay_duplicate_C2,
    [SendConfigCmd] = eink_goodisplay_send_config_cmd,
    [SendDataCmd] = eink_goodisplay_send_image_data,
    [UpdateDisplay] = eink_goodisplay_update_display,
    [SendDataDone] = eink_goodisplay_finish_handler,

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

        NfcEinkScreenSpecificGoodisplayContext* ctx = screen->data->screen_context;
        command = handlers[ctx->state](poller->iso14443_3a_poller, screen);
    }
    if(command == NfcCommandReset) iso14443_4a_poller_halt(poller);
    return command;
}
