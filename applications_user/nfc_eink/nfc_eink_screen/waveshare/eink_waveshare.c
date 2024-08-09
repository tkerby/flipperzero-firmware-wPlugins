#include "eink_waveshare.h"
#include "eink_waveshare_i.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>

/// TODO: maybe this can be separated for each eink manufacturer
/// and moved to a dedicated file. After that we can use this array
/// inside of .init function callback in order to fully initialize
/// screen data instance. Also think of protecting this as protected so there will be no opened callback functions
static const NfcEinkScreenDescriptor waveshare_screens[NfcEinkScreenTypeWaveshareNum] = {
    [NfcEinkScreenTypeWaveshareUnknown] =
        {
            .name = "Waveshare Unknown",
            .width = 0,
            .height = 0,
            .screen_manufacturer = NfcEinkManufacturerWaveshare,
            .screen_type = NfcEinkScreenTypeWaveshareUnknown,
            .data_block_size = 0,
        },
    [NfcEinkScreenTypeWaveshare2n13inch] =
        {
            .name = "Waveshare 2.13 inch",
            .width = 250,
            .height = 122,
            .screen_manufacturer = NfcEinkManufacturerWaveshare,
            .screen_type = NfcEinkScreenTypeWaveshare2n13inch,
            .data_block_size = 16,
        },
    [NfcEinkScreenTypeWaveshare2n9inch] =
        {
            .name = "Waveshare 2.9 inch",
            .width = 296,
            .height = 128,
            .screen_manufacturer = NfcEinkManufacturerWaveshare,
            .screen_type = NfcEinkScreenTypeWaveshare2n9inch,
            .data_block_size = 16,
        },

};

static NfcDevice* eink_waveshare_alloc() {
    const uint8_t uid[] = {0x57, 0x53, 0x44, 0x5A, 0x31, 0x30, 0x6D};
    const uint8_t atqa[] = {0x44, 0x00};

    Iso14443_3aData* iso14443_3a_edit_data = iso14443_3a_alloc();
    iso14443_3a_set_uid(iso14443_3a_edit_data, uid, sizeof(uid));
    iso14443_3a_set_sak(iso14443_3a_edit_data, 0);
    iso14443_3a_set_atqa(iso14443_3a_edit_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_3a, iso14443_3a_edit_data);

    iso14443_3a_free(iso14443_3a_edit_data);
    return nfc_device;
}

static void eink_waveshare_free(NfcDevice* instance) {
    furi_assert(instance);
    nfc_device_free(instance);
}

static void eink_waveshare_init(NfcEinkScreenDescriptor* descriptor, NfcEinkType generic_type) {
    furi_assert(descriptor);
    NfcEinkScreenTypeWaveshare waveshare_type = (NfcEinkScreenTypeWaveshare)generic_type;
    furi_assert(waveshare_type != NfcEinkScreenTypeWaveshareUnknown);
    furi_assert(waveshare_type < NfcEinkScreenTypeWaveshareNum);

    *descriptor = waveshare_screens[waveshare_type];
}

static void eink_waveshare_on_done(NfcEinkScreen* instance) {
    furi_assert(instance);
    if(instance->internal_event_callback != NULL)
        instance->internal_event_callback(NfcEinkScreenEventTypeDone, instance);
}

static void eink_waveshare_on_config_received(NfcEinkScreen* instance) {
    furi_assert(instance);
    if(instance->internal_event_callback != NULL)
        instance->internal_event_callback(NfcEinkScreenEventTypeConfigurationReceived, instance);
}

static void
    eink_waveshare_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length) {
    UNUSED(screen);
    UNUSED(data_length);
    NfcEinkScreenTypeWaveshare waveshare_type = NfcEinkScreenTypeWaveshareUnknown;
    uint8_t protocol_type = data[0];
    if(protocol_type == 4) {
        waveshare_type = NfcEinkScreenTypeWaveshare2n13inch;
    } else if(protocol_type == 7) {
        waveshare_type = NfcEinkScreenTypeWaveshare2n9inch;
    }

    eink_waveshare_init(&screen->data->base, (NfcEinkType)waveshare_type);
    eink_waveshare_on_config_received(screen);
}

static NfcCommand eink_waveshare_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkScreen* instance = context;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;

    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedData) {
        FURI_LOG_D(TAG, "ReceivedData");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeFieldOff) {
        FURI_LOG_D(TAG, "FieldOff");
        eink_waveshare_on_done(instance);
        command = NfcCommandStop;
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeHalted) {
        FURI_LOG_D(TAG, "Halted");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;

        const uint8_t* data = bit_buffer_get_data(buffer);
        FURI_LOG_D(TAG, "0x%02X, 0x%02X", data[0], data[1]);

        //TODO: move this to process function
        //NfcEinkScreen* const screen = instance->screen;
        NfcEinkScreenData* const screen_data = instance->data;
        if(data[1] == 0x00) {
            eink_waveshare_parse_config(instance, data + 2, 1);
        } else if(data[1] == 0x08) {
            memcpy(screen_data->image_data + screen_data->received_data, &data[3], data[2]);
            screen_data->received_data += data[2];

            uint16_t last = screen_data->received_data - 1;
            screen_data->image_data[last] &= 0xC0;
        }

        bit_buffer_reset(instance->tx_buf);
        bit_buffer_append_byte(instance->tx_buf, (data[1] == 0x0A) ? 0xFF : 0x00);
        bit_buffer_append_byte(instance->tx_buf, 0x00);
        nfc_listener_tx(event.instance, instance->tx_buf);
    }

    return command;
}

static NfcCommand eink_waveshare_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    UNUSED(event);
    return NfcCommandContinue;
}

const NfcEinkScreenHandlers waveshare_handlers = {
    .alloc_nfc_device = eink_waveshare_alloc,
    .free = eink_waveshare_free,
    .init = eink_waveshare_init,
    .listener_callback = eink_waveshare_listener_callback,
    .poller_callback = eink_waveshare_poller_callback,
};
