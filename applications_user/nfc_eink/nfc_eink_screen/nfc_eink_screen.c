#include "nfc_eink_screen.h"

#include <simple_array.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>

extern const NfcEinkScreenHandlers waveshare_handlers;

static const NfcEinkScreenHandlers* handlers[NfcEinkManufacturerNum] = {
    [NfcEinkManufacturerWaveshare] = &waveshare_handlers,
    [NfcEinkManufacturerGoodisplay] = &waveshare_handlers,
};

/// TODO: maybe this can be separated for each eink manufacturer
/// and moved to a dedicated file. After that we can use this array
/// inside of .init function callback in order to fully initialize
/// screen data instance. Also think of protecting this as protected so there will be no opened callback functions
static const NfcEinkScreenDescriptor screens[NfcEinkTypeNum] = {
    [NfcEinkTypeWaveshare2n13inch] =
        {
            .name = "Waveshare 2.13 inch",
            .width = 250,
            .height = 122,
            .screen_manufacturer = NfcEinkManufacturerWaveshare,
            .screen_type = NfcEinkTypeWaveshare2n13inch,
            .data_block_size = 16,
        },
    [NfcEinkTypeWaveshare2n9inch] =
        {
            .name = "Waveshare 2.9 inch",
            .width = 296,
            .height = 128,
            .screen_manufacturer = NfcEinkManufacturerWaveshare,
            .screen_type = NfcEinkTypeWaveshare2n9inch,
            .data_block_size = 16,
        },
    /*     [NfcEinkTypeWaveshare4n2inch] =
        {
            .name = "Waveshare 4.2 inch",
            .width = 296,
            .height = 128,
            .screen_type = NfcEinkTypeWaveshare4n2inch,
            .data_block_size = 16,
        }, */
};

/* static NfcDevice* nfc_eink_screen_nfc_device_alloc() {
    //TODO:this may differ for other screen types
    //think of replacing it to some function which
    //returns different data according to screen type
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
 */
/* static void ats_array_init(SimpleArrayElement* elem) {
    const uint8_t ats_data[] = {0x90, 0xC0, 0xC9, 0x06, 0x7F, 0x00};
    static uint8_t index = 0;

    *(uint8_t*)elem = ats_data[index++];
} */

/* static NfcDevice* nfc_eink_screen_nfc_device_4a_alloc() {
    const uint8_t uid[] = {0xC0, 0xC9, 0x06, 0x7F};
    const uint8_t atqa[] = {0x04, 0x00};
    const uint8_t ats_data[] = {0x90, 0xC0, 0xC9, 0x06, 0x7F, 0x00};

    Iso14443_4aData* iso14443_4a_edit_data = iso14443_4a_alloc();
    iso14443_4a_set_uid(iso14443_4a_edit_data, uid, sizeof(uid));

    Iso14443_4aAtsData* _4aAtsData = &iso14443_4a_edit_data->ats_data;
    _4aAtsData->tl = 0x0B;
    _4aAtsData->t0 = 0x78;
    _4aAtsData->ta_1 = 0x33;
    _4aAtsData->tb_1 = 0xA0;
    _4aAtsData->tc_1 = 0x02;

    _4aAtsData->t1_tk = simple_array_alloc(&simple_array_config_uint8_t);
    simple_array_init(_4aAtsData->t1_tk, sizeof(ats_data));
    SimpleArrayData* ats_array = simple_array_get_data(_4aAtsData->t1_tk);
    memcpy(ats_array, ats_data, sizeof(ats_data));

    Iso14443_3aData* base_data = iso14443_4a_get_base_data(iso14443_4a_edit_data);
    iso14443_3a_set_sak(base_data, 0x28);
    iso14443_3a_set_atqa(base_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_4a, iso14443_4a_edit_data);

    iso14443_4a_free(iso14443_4a_edit_data);
    return nfc_device;
} */

const char* nfc_eink_screen_get_name(NfcEinkType type) {
    furi_check(type < NfcEinkTypeNum);
    return screens[type].name;
}

static bool eink_waveshare_init(NfcEinkScreenData* screen) {
    furi_assert(screen);
    screen->image_size =
        screen->base.width *
        (screen->base.height % 8 == 0 ? (screen->base.height / 8) : (screen->base.height / 8 + 1));
    screen->image_data = malloc(screen->image_size);

    return false;
}

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkType type) {
    furi_check(type < NfcEinkTypeNum);

    NfcEinkScreen* screen = malloc(sizeof(NfcEinkScreen));
    screen->handlers = handlers[0];

    screen->nfc_device = screen->handlers->alloc_nfc_device();

    screen->data = malloc(sizeof(NfcEinkScreenData));

    screen->data->base = screens[0];
    eink_waveshare_init(screen->data);
    //screen->handlers->init(screen->data);

    screen->tx_buf = bit_buffer_alloc(50);

    return screen;
}

void nfc_eink_screen_free(NfcEinkScreen* screen) {
    furi_check(screen);
    screen->handlers->free(screen->nfc_device);

    //TODO: remember to free screen->data->image_data
    free(screen->data);
    bit_buffer_free(screen->tx_buf);
    screen->handlers = NULL;
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
