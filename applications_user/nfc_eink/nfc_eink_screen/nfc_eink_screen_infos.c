#include "nfc_eink_screen_infos.h"

typedef bool (*NfcEinkDescriptorCompareDelegate)(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b);

static const NfcEinkScreenInfo screen_descriptors[] = {
    {
        .name = "Unknown",
        .width = 0,
        .height = 0,
        .screen_size = NfcEinkScreenSizeUnknown,
        .screen_manufacturer = NfcEinkManufacturerUnknown,
        .screen_type = NfcEinkScreenTypeUnknown,
        .data_block_size = 0,
    },
    {
        .name = "Waveshare 2.13 inch",
        .width = 250,
        .height = 122,
        .screen_size = NfcEinkScreenSize2n13inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .screen_type = NfcEinkScreenTypeWaveshare2n13inch,
        .data_block_size = 16,
    },
    {
        .name = "Waveshare 2.9 inch",
        .width = 296,
        .height = 128,
        .screen_size = NfcEinkScreenSize2n9inch,
        .screen_manufacturer = NfcEinkManufacturerWaveshare,
        .screen_type = NfcEinkScreenTypeWaveshare2n9inch,
        .data_block_size = 16,
    },
    {
        .name = "GDEY0213B74",
        .width = 250,
        .height = 122,
        .screen_size = NfcEinkScreenSize2n13inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .screen_type = NfcEinkScreenTypeGoodisplay2n13inch,
        .data_block_size = 0xFA,
    },
    {
        .name = "GDEY029T94",
        .width = 296,
        .height = 128,
        .screen_size = NfcEinkScreenSize2n9inch,
        .screen_manufacturer = NfcEinkManufacturerGoodisplay,
        .screen_type = NfcEinkScreenTypeGoodisplay2n9inch,
        .data_block_size = 0xFA,
    },
};

const NfcEinkScreenInfo* nfc_eink_descriptor_get_by_type(const NfcEinkScreenType type) {
    furi_assert(type < NfcEinkScreenTypeNum);
    furi_assert(type != NfcEinkScreenTypeUnknown);

    const NfcEinkScreenInfo* item = NULL;
    for(uint8_t i = 0; i < COUNT_OF(screen_descriptors); i++) {
        if(screen_descriptors[i].screen_type == type) {
            item = &screen_descriptors[i];
            break;
        }
    }
    return item;
}

static uint8_t nfc_eink_descriptor_filter_by(
    EinkScreenInfoArray_t result,
    const NfcEinkScreenInfo* sample,
    NfcEinkDescriptorCompareDelegate compare) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < COUNT_OF(screen_descriptors); i++) {
        if(compare(&screen_descriptors[i], sample)) {
            EinkScreenInfoArray_push_back(result, &screen_descriptors[i]);
            count++;
        }
    }
    return count;
}

static inline bool nfc_eink_descriptor_compare_by_manufacturer(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    return a->screen_manufacturer == b->screen_manufacturer;
}

static inline bool nfc_eink_descriptor_compare_by_screen_size(
    const NfcEinkScreenInfo* const a,
    const NfcEinkScreenInfo* const b) {
    return a->screen_size == b->screen_size;
}

uint8_t nfc_eink_descriptor_filter_by_manufacturer(
    EinkScreenInfoArray_t result,
    NfcEinkManufacturer manufacturer) {
    furi_assert(result);
    furi_assert(manufacturer < NfcEinkManufacturerNum);
    furi_assert(manufacturer != NfcEinkManufacturerUnknown);

    NfcEinkScreenInfo dummy = {.screen_manufacturer = manufacturer};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_manufacturer);
}

uint8_t nfc_eink_descriptor_filter_by_screen_size(
    EinkScreenInfoArray_t result,
    NfcEinkScreenSize screen_size) {
    furi_assert(result);
    furi_assert(screen_size != NfcEinkScreenSizeUnknown);
    furi_assert(screen_size < NfcEinkScreenSizeNum);

    NfcEinkScreenInfo dummy = {.screen_size = screen_size};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_screen_size);
}

uint8_t nfc_eink_descriptor_filter_by_screen_type(
    EinkScreenInfoArray_t result,
    NfcEinkScreenType screen_type) {
    furi_assert(result);
    furi_assert(screen_type != NfcEinkScreenTypeUnknown);
    furi_assert(screen_type < NfcEinkScreenTypeNum);

    EinkScreenInfoArray_push_back(result, nfc_eink_descriptor_get_by_type(screen_type));
    return 1;
}
