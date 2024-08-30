#include "nfc_eink_screen_descriptors.h"

typedef bool (*NfcEinkDescriptorCompareDelegate)(
    const NfcEinkScreenDescriptor* const a,
    const NfcEinkScreenDescriptor* const b);

static const NfcEinkScreenDescriptor screen_descriptors[] = {
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
        .data_block_size =
            0xFA, ///TODO: remove this or think of more eficient algorithm of displaying
    },
};

#define DESCRIPTOR_ARRAY_SIZE (sizeof(screen_descriptors) / sizeof(NfcEinkScreenDescriptor))

const NfcEinkScreenDescriptor* nfc_eink_descriptor_get_by_type(const NfcEinkScreenType type) {
    furi_assert(type < NfcEinkScreenTypeNum);
    furi_assert(type != NfcEinkScreenTypeUnknown);

    const NfcEinkScreenDescriptor* item = NULL;
    for(uint8_t i = 0; i < DESCRIPTOR_ARRAY_SIZE; i++) {
        if(screen_descriptors[i].screen_type == type) {
            item = &screen_descriptors[i];
            break;
        }
    }
    return item;
}

static uint8_t nfc_eink_descriptor_filter_by(
    EinkScreenDescriptorArray_t result,
    const NfcEinkScreenDescriptor* sample,
    NfcEinkDescriptorCompareDelegate compare) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < DESCRIPTOR_ARRAY_SIZE; i++) {
        if(compare(&screen_descriptors[i], sample)) {
            EinkScreenDescriptorArray_push_back(result, &screen_descriptors[i]);
            count++;
        }
    }
    return count;
}

static inline bool nfc_eink_descriptor_compare_by_manufacturer(
    const NfcEinkScreenDescriptor* const a,
    const NfcEinkScreenDescriptor* const b) {
    return a->screen_manufacturer == b->screen_manufacturer;
}

static inline bool nfc_eink_descriptor_compare_by_screen_size(
    const NfcEinkScreenDescriptor* const a,
    const NfcEinkScreenDescriptor* const b) {
    return a->screen_size == b->screen_size;
}

uint8_t nfc_eink_descriptor_filter_by_manufacturer(
    EinkScreenDescriptorArray_t result,
    NfcEinkManufacturer manufacturer) {
    furi_assert(result);
    furi_assert(manufacturer < NfcEinkManufacturerNum);
    furi_assert(manufacturer != NfcEinkManufacturerUnknown);

    NfcEinkScreenDescriptor dummy = {.screen_manufacturer = manufacturer};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_manufacturer);
}

uint8_t nfc_eink_descriptor_filter_by_screen_size(
    EinkScreenDescriptorArray_t result,
    NfcEinkScreenSize screen_size) {
    furi_assert(result);
    furi_assert(screen_size != NfcEinkScreenSizeUnknown);
    furi_assert(screen_size < NfcEinkScreenSizeNum);

    NfcEinkScreenDescriptor dummy = {.screen_size = screen_size};
    return nfc_eink_descriptor_filter_by(
        result, &dummy, nfc_eink_descriptor_compare_by_screen_size);
}