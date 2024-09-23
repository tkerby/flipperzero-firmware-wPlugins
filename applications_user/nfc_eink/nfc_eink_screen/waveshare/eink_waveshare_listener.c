#include "eink_waveshare_i.h"

#define TAG "WSH_Listener"

NfcCommand eink_waveshare_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkScreen* instance = context;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;

    NfcEinkWaveshareSpecificContext* ctx = instance->device->screen_context;
    uint8_t* blocks = ctx->buf;

    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedData) {
        FURI_LOG_D(TAG, "ReceivedData");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeFieldOff) {
        FURI_LOG_D(TAG, "FieldOff");
        if(ctx->listener_state == NfcEinkWaveshareListenerStateUpdatedSuccefully)
            eink_waveshare_on_done(instance);
        else if(ctx->listener_state != NfcEinkWaveshareListenerStateIdle)
            eink_waveshare_on_target_lost(instance);
        command = NfcCommandStop;
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeHalted) {
        FURI_LOG_D(TAG, "Halted");
        if(ctx->listener_state == NfcEinkWaveshareListenerStateUpdatedSuccefully)
            eink_waveshare_on_done(instance);
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;

        const uint8_t* data = bit_buffer_get_data(buffer);
        FURI_LOG_D(TAG, "0x%02X, 0x%02X", data[0], data[1]);

        ///TODO: move this to process function
        bit_buffer_reset(instance->tx_buf);
        NfcEinkScreenData* const screen_data = instance->data;
        NfcEinkScreenDevice* const screen_device = instance->device;
        if(data[0] == 0xFF && data[1] == 0xFE) {
            bit_buffer_append_byte(instance->tx_buf, 0xEE);
            bit_buffer_append_byte(instance->tx_buf, 0xFF);
            iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        } else if(data[0] == 0x30) {
            uint8_t index = data[1];
            bit_buffer_append_bytes(instance->tx_buf, &blocks[index * 4], 4 * 4);
            iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        } else if(data[0] == 0xA2) {
            uint8_t index = data[1];
            memcpy(&blocks[index * 4], &data[2], 4);
            bit_buffer_append_byte(instance->tx_buf, 0x0A);
            iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        } else if(data[0] == 0xCD) {
            if(data[1] == 0x00) {
                eink_waveshare_parse_config(instance, data + 2, 1);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
            } else if(data[1] == 0x04) {
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                ctx->listener_state = NfcEinkWaveshareListenerStateUpdatedSuccefully;
            } else if(data[1] == 0x0D) {
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                //bit_buffer_append_byte(instance->tx_buf, 0x02); //Some kind of busy status
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                ctx->listener_state = NfcEinkWaveshareListenerStateWaitingForConfig;
                eink_waveshare_on_target_detected(instance);
            } else if(data[1] == 0x08) {
                memcpy(screen_data->image_data + screen_device->received_data, &data[3], data[2]);
                screen_device->received_data += data[2];

                uint16_t last = screen_device->received_data - 1;
                screen_data->image_data[last] &= 0xC0;

                bit_buffer_append_byte(instance->tx_buf, 0x00);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                ctx->listener_state = NfcEinkWaveshareListenerStateReadingBlocks;
            } else if(data[1] == 0x0A) {
                bit_buffer_append_byte(instance->tx_buf, 0xFF);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
            } else {
                bit_buffer_append_byte(instance->tx_buf, 0x00);
                bit_buffer_append_byte(instance->tx_buf, 0x00);
            }

            iso14443_crc_append(Iso14443CrcTypeA, instance->tx_buf);
        }
        if(bit_buffer_get_size_bytes(instance->tx_buf) > 0)
            nfc_listener_tx(event.instance, instance->tx_buf);
    }

    return command;
}
