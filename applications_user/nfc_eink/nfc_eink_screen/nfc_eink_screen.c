#include "nfc_eink_screen.h"

#include <../lib/flipper_format/flipper_format.h>
#include <../applications/services/storage/storage.h>

///TODO: possibly move this closer to save/load functions
#define NFC_EINK_FORMAT_VERSION               (1)
#define NFC_EINK_FILE_HEADER                  "Flipper NFC Eink screen"
#define NFC_EINK_DEVICE_UID_KEY               "UID"
#define NFC_EINK_DEVICE_TYPE_KEY              "NFC type"
#define NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY "Manufacturer type"
#define NFC_EINK_SCREEN_MANUFACTURER_NAME_KEY "Manufacturer name"
#define NFC_EINK_SCREEN_TYPE_KEY              "Screen type"
#define NFC_EINK_SCREEN_NAME_KEY              "Screen name"
#define NFC_EINK_SCREEN_WIDTH_KEY             "Width"
#define NFC_EINK_SCREEN_HEIGHT_KEY            "Height"
#define NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY   "Data block size"
#define NFC_EINK_SCREEN_DATA_TOTAL_KEY        "Data total"
#define NFC_EINK_SCREEN_DATA_READ_KEY         "Data read"
#define NFC_EINK_SCREEN_BLOCK_DATA_KEY        "Block"
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
    screen->rx_buf = bit_buffer_alloc(50);
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
    bit_buffer_free(screen->rx_buf);
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

bool nfc_eink_screen_load(const char* file_path, NfcEinkScreen** screen) {
    furi_assert(screen);
    furi_assert(file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);

    bool loaded = false;
    NfcEinkScreen* scr = NULL;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, file_path)) break;

        uint32_t manufacturer = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY, &manufacturer, 1))
            break;
        scr = nfc_eink_screen_alloc(manufacturer);

        uint32_t screen_type = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_TYPE_KEY, &screen_type, 1)) break;
        nfc_eink_screen_init(scr, screen_type);

        uint32_t buf = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_DATA_READ_KEY, &buf, 1)) break;
        scr->data->received_data = buf;

        FuriString* temp_str = furi_string_alloc();
        bool block_loaded = false;
        for(uint16_t i = 0; i < scr->data->received_data; i += scr->data->base.data_block_size) {
            furi_string_printf(temp_str, "%s %d", NFC_EINK_SCREEN_BLOCK_DATA_KEY, i);
            block_loaded = flipper_format_read_hex(
                ff,
                furi_string_get_cstr(temp_str),
                &scr->data->image_data[i],
                scr->data->base.data_block_size);
            if(!block_loaded) break;
        }
        furi_string_free(temp_str);

        loaded = block_loaded;
        *screen = scr;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    if(!loaded) {
        nfc_eink_screen_free(scr);
        *screen = NULL;
    }
    return loaded;
}

bool nfc_eink_screen_save(const NfcEinkScreen* screen, const char* file_path) {
    furi_assert(screen);
    furi_assert(file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();

    bool saved = false;
    do {
        if(!flipper_format_buffered_file_open_always(ff, file_path)) break;

        // Write header
        if(!flipper_format_write_header_cstr(ff, NFC_EINK_FILE_HEADER, NFC_EINK_FORMAT_VERSION))
            break;

        // Write device type
        NfcProtocol protocol = nfc_device_get_protocol(screen->nfc_device);
        if(!flipper_format_write_string_cstr(
               ff, NFC_EINK_DEVICE_TYPE_KEY, nfc_device_get_protocol_name(protocol)))
            break;

        // Write UID
        furi_string_printf(
            temp_str, "%s size can be different depending on type", NFC_EINK_DEVICE_UID_KEY);
        if(!flipper_format_write_comment(ff, temp_str)) break;

        size_t uid_len;
        const uint8_t* uid = nfc_device_get_uid(screen->nfc_device, &uid_len);
        if(!flipper_format_write_hex(ff, NFC_EINK_DEVICE_UID_KEY, uid, uid_len)) break;

        // Write manufacturer type
        uint32_t buf = screen->data->base.screen_manufacturer;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_MANUFACTURER_TYPE_KEY, &buf, 1)) break;

        //uint32_t buf = screen->data->base.screen_manufacturer;
        //if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_MANUFACTURER_NAME_KEY, &buf, 1)) break;

        // Write screen type
        buf = screen->data->base.screen_type;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_TYPE_KEY, &buf, 1)) break;

        // Write screen name
        if(!flipper_format_write_string_cstr(ff, NFC_EINK_SCREEN_NAME_KEY, screen->data->base.name))
            break;

        // Write screen width
        buf = screen->data->base.width;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_WIDTH_KEY, &buf, 1)) break;

        // Write screen height
        buf = screen->data->base.height;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_HEIGHT_KEY, &buf, 1)) break;

        // Write data block size
        furi_string_printf(
            temp_str,
            "%s may be different depending on type",
            NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY);
        if(!flipper_format_write_comment(ff, temp_str)) break;

        buf = screen->data->base.data_block_size;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_DATA_BLOCK_SIZE_KEY, &buf, 1)) break;

        // Write image total size
        buf = screen->data->image_size;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_DATA_TOTAL_KEY, &buf, 1)) break;

        // Write image read size
        buf = screen->data->received_data;
        if(!flipper_format_write_uint32(ff, NFC_EINK_SCREEN_DATA_READ_KEY, &buf, 1)) break;

        // Write image data
        furi_string_printf(temp_str, "Data");
        if(!flipper_format_write_comment(ff, temp_str)) break;

        bool block_saved = false;
        for(uint16_t i = 0; i < screen->data->received_data;
            i += screen->data->base.data_block_size) {
            furi_string_printf(temp_str, "%s %d", NFC_EINK_SCREEN_BLOCK_DATA_KEY, i);
            block_saved = flipper_format_write_hex(
                ff,
                furi_string_get_cstr(temp_str),
                &screen->data->image_data[i],
                screen->data->base.data_block_size);

            if(!block_saved) break;
        }
        saved = block_saved;
    } while(false);

    flipper_format_free(ff);
    furi_string_free(temp_str);
    furi_record_close(RECORD_STORAGE);

    return saved;
}

bool nfc_eink_screen_delete(const char* file_path) {
    furi_assert(file_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool deleted = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);
    return deleted;
}