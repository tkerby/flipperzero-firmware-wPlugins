#include "eink_goodisplay.h"
#include "eink_goodisplay_i.h"
#include <simple_array.h>

static const NfcEinkScreenDescriptor goodisplay_screens[NfcEinkScreenTypeGoodisplayNum] = {
    [NfcEinkScreenTypeGoodisplayUnknown] =
        {
            .name = "Goodisplay Unknown",
            .width = 0,
            .height = 0,
            .screen_manufacturer = NfcEinkManufacturerGoodisplay,
            .screen_type = NfcEinkScreenTypeGoodisplayUnknown,
            .data_block_size = 0,
        },
    [NfcEinkScreenTypeGoodisplay2n13inch] =
        {
            .name = "GDEY0213B74",
            .width = 250,
            .height = 122,
            .screen_manufacturer = NfcEinkManufacturerGoodisplay,
            .screen_type = NfcEinkScreenTypeGoodisplay2n13inch,
            .data_block_size =
                0xFA, ///TODO: remove this or think of more eficient algorithm of displaying
        },
};

static NfcDevice* eink_goodisplay_nfc_device_4a_alloc() {
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
}

static void eink_goodisplay_free(NfcDevice* instance) {
    furi_assert(instance);
    nfc_device_free(instance);
}

static void eink_goodisplay_init(NfcEinkScreenData* data, NfcEinkType generic_type) {
    furi_assert(data);

    NfcEinkScreenTypeGoodisplay goodisplay_type = (NfcEinkScreenTypeGoodisplay)generic_type;
    furi_assert(goodisplay_type != NfcEinkScreenTypeGoodisplayUnknown);
    furi_assert(goodisplay_type < NfcEinkScreenTypeGoodisplayNum);

    data->base = goodisplay_screens[goodisplay_type];
    NfcEinkScreenSpecificGoodisplayContext* context =
        malloc(sizeof(NfcEinkScreenSpecificGoodisplayContext));
    context->state = SendC2Cmd;
    data->screen_context = context;
}

void eink_goodisplay_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length) {
    UNUSED(data_length);
    UNUSED(data);
    NfcEinkScreenTypeGoodisplay goodisplay_type = NfcEinkScreenTypeGoodisplayUnknown;
    goodisplay_type = NfcEinkScreenTypeGoodisplay2n13inch;

    eink_goodisplay_init(screen->data, (NfcEinkType)goodisplay_type);
    eink_goodisplay_on_config_received(screen);
}

/* static uint8_t nfc_eink_screen_command_D4(const APDU_Command* command, APDU_Response* resp) {
    UNUSED(command);
    UNUSED(resp);

    //resp->status = __builtin_bswap16(0x9000);
    return 2;
} */

const NfcEinkScreenHandlers goodisplay_handlers = {
    .alloc_nfc_device = eink_goodisplay_nfc_device_4a_alloc,
    .free = eink_goodisplay_free,
    .init = eink_goodisplay_init,
    .listener_callback = eink_goodisplay_listener_callback,
    .poller_callback = eink_goodisplay_poller_callback,
};
