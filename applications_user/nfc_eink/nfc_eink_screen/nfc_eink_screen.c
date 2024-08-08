#include "nfc_eink_screen.h"

#include <simple_array.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>

static const NfcEinkScreenDescriptor screens[NfcEinkTypeNum] = {
    [NfcEinkTypeWaveshare2n13inch] =
        {
            .name = "Waveshare 2.13 inch",
            .width = 250,
            .height = 122,
            .screen_type = NfcEinkTypeWaveshare2n13inch,
            .data_block_size = 16,
        },
    [NfcEinkTypeWaveshare2n9inch] =
        {
            .name = "Waveshare 2.9 inch",
            .width = 296,
            .height = 128,
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

static NfcDevice* nfc_eink_screen_nfc_device_4a_alloc() {
    //TODO:this may differ for other screen types
    //think of replacing it to some function which
    //returns different data according to screen type
    const uint8_t uid[] = {0xC0, 0xC9, 0x06, 0x7F};
    const uint8_t atqa[] = {0x04, 0x00};
    const uint8_t ats_data[] = {0x90, 0xC0, 0xC9, 0x06, 0x7F, 0x00};
    //const uint8_t ats_data[] = {0x0B, 0x78, 0x33, 0xA0, 0x02, 0x90, 0xC0, 0xC9, 0x06, 0x7F, 0x00};

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

    /* SimpleArrayConfig config = {.init = ats_array_init, .type_size = sizeof(uint8_t)};
    iso14443_4a_edit_data->ats_data.t1_tk = simple_array_alloc(&config);
    simple_array_init(iso14443_4a_edit_data->ats_data.t1_tk, 6); */

    Iso14443_3aData* base_data = iso14443_4a_get_base_data(iso14443_4a_edit_data);
    iso14443_3a_set_sak(base_data, 0x28);
    iso14443_3a_set_atqa(base_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_4a, iso14443_4a_edit_data);

    iso14443_4a_free(iso14443_4a_edit_data);
    return nfc_device;
}

const char* nfc_eink_screen_get_name(NfcEinkType type) {
    furi_check(type < NfcEinkTypeNum);
    return screens[type].name;
}

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkType type) {
    furi_check(type < NfcEinkTypeNum);

    NfcEinkScreen* screen = malloc(sizeof(NfcEinkScreen));

    //or screen->base = screens[type];
    memcpy(&screen->base, &screens[type], sizeof(NfcEinkScreenDescriptor));

    //screen->nfc_device = nfc_eink_screen_nfc_device_alloc();
    screen->nfc_device = nfc_eink_screen_nfc_device_4a_alloc();

    screen->image_size =
        screen->base.width *
        (screen->base.height % 8 == 0 ? (screen->base.height / 8) : (screen->base.height / 8 + 1));
    screen->image_data = malloc(screen->image_size);
    return screen;
}

void nfc_eink_screen_free(NfcEinkScreen* screen) {
    furi_check(screen);
    free(screen->image_data);
    nfc_device_free(screen->nfc_device);
    free(screen);
}
