#include "nfc_eink_screen.h"

//#include <nfc/protocols/nfc_protocol.h>

///TODO: move this externs to separate files in screen folders for each type
extern const NfcEinkScreenHandlers waveshare_handlers;
extern const NfcEinkScreenHandlers goodisplay_handlers;

typedef struct {
    const NfcEinkScreenHandlers* handlers;
    const char* name;
} NfcEinkScreenManufacturerDescriptor;

/* static const NfcEinkScreenHandlers* handlers[NfcEinkManufacturerNum] = {
    [NfcEinkManufacturerWaveshare] = &waveshare_handlers,
    [NfcEinkManufacturerGoodisplay] = &waveshare_handlers,
};

static const char* manufacturer_names[NfcEinkManufacturerNum] = {
    [NfcEinkManufacturerWaveshare] = "Waveshare",
    [NfcEinkManufacturerGoodisplay] = "Goodisplay",
}; */

static const NfcEinkScreenManufacturerDescriptor manufacturers[NfcEinkManufacturerNum] = {
    [NfcEinkManufacturerWaveshare] =
        {
            .handlers = &waveshare_handlers,
            .name = "Waveshare",
        },
    [NfcEinkManufacturerGoodisplay] =
        {
            .handlers = &goodisplay_handlers,
            .name = "Goodisplay",
        },
};

const char* nfc_eink_screen_get_manufacturer_name(NfcEinkManufacturer manufacturer) {
    furi_assert(manufacturer < NfcEinkManufacturerNum);
    return manufacturers[manufacturer].name;
}

static void nfc_eink_screen_event_callback(NfcEinkScreenEventType type, void* context) {
    furi_assert(context);
    NfcEinkScreen* screen = context;
    if(type == NfcEinkScreenEventTypeDone) {
        NfcEinkScreenDoneEvent event = screen->done_event;
        if(event.done_callback != NULL) event.done_callback(event.context);
    } else if(type == NfcEinkScreenEventTypeConfigurationReceived) {
        FURI_LOG_D(TAG, "Config received");
        nfc_eink_screen_init(screen, screen->data->base.screen_type);
    }
}

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkManufacturer manufacturer) {
    furi_check(manufacturer < NfcEinkManufacturerNum);

    NfcEinkScreen* screen = malloc(sizeof(NfcEinkScreen));
    screen->handlers = manufacturers[manufacturer].handlers;

    screen->nfc_device = screen->handlers->alloc_nfc_device();
    screen->internal_event_callback = nfc_eink_screen_event_callback;
    //screen->internal_event.callback = nfc_eink_screen_event_callback;

    //Set NfcEinkScreenEvent callback
    //screen->handlers->set_event_callback();
    screen->data = malloc(sizeof(NfcEinkScreenData));

    //screen->data->base = screens[0];
    //eink_waveshare_init(screen->data);

    screen->tx_buf = bit_buffer_alloc(50);

    return screen;
}

void nfc_eink_screen_init(NfcEinkScreen* screen, NfcEinkType type) {
    UNUSED(type);
    if(screen->data->base.screen_type == NfcEinkTypeUnknown) {
        screen->handlers->init(&screen->data->base, type);
    }

    NfcEinkScreenData* data = screen->data;
    data->image_size =
        data->base.width *
        (data->base.height % 8 == 0 ? (data->base.height / 8) : (data->base.height / 8 + 1));
    data->image_data = malloc(data->image_size);
}

void nfc_eink_screen_free(NfcEinkScreen* screen) {
    furi_check(screen);

    NfcEinkScreenData* data = screen->data;
    free(data->image_data);
    free(data);

    screen->handlers->free(screen->nfc_device);
    bit_buffer_free(screen->tx_buf);
    screen->handlers = NULL;
    free(screen);
}

void nfc_eink_screen_set_done_callback(
    NfcEinkScreen* screen,
    NfcEinkScreenDoneCallback eink_screen_done_callback,
    void* context) {
    furi_assert(screen);
    furi_assert(eink_screen_done_callback);
    screen->done_event.done_callback = eink_screen_done_callback;
    screen->done_event.context = context;
    //screen->done_callback = eink_screen_done_callback;
}
