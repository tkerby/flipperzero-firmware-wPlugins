#pragma once
#include <furi.h>
#include <m-array.h>

typedef enum {
    NfcEinkScreenSizeUnknown,
    NfcEinkScreenSize2n13inch,
    NfcEinkScreenSize2n9inch,

    NfcEinkScreenSizeNum
} NfcEinkScreenSize;

typedef enum {
    //NfcEinkManufacturerGeneric,
    NfcEinkManufacturerWaveshare,
    NfcEinkManufacturerGoodisplay,

    NfcEinkManufacturerNum,
    NfcEinkManufacturerUnknown
} NfcEinkManufacturer;

typedef enum {
    NfcEinkScreenTypeUnknown,
    NfcEinkScreenTypeGoodisplay2n13inch,
    NfcEinkScreenTypeGoodisplay2n9inch,
    //NfcEinkTypeGoodisplay4n2inch,

    NfcEinkScreenTypeWaveshare2n13inch,
    NfcEinkScreenTypeWaveshare2n9inch,
    //NfcEinkTypeWaveshare4n2inch,

    NfcEinkScreenTypeNum
} NfcEinkScreenType;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t data_block_size;
    NfcEinkScreenType screen_type;
    NfcEinkScreenSize screen_size;
    NfcEinkManufacturer screen_manufacturer;
    const char* name;
} NfcEinkScreenDescriptor; ///TODO: Rename to NfcEinkScreenInfo

#define M_ARRAY_SIZE (sizeof(NfcEinkScreenDescriptor*) * NfcEinkScreenTypeNum)
#define M_INIT(a)    ((a) = malloc(M_ARRAY_SIZE))

#define M_INIT_SET(new, old)                          \
    do {                                              \
        M_INIT(new);                                  \
        memcpy((void*)new, (void*)old, M_ARRAY_SIZE); \
    } while(false)

//#define M_SET(a, b) (M_INIT(a); memcpy(a, b, M_ARRAY_SIZE))
#define M_CLEAR(a) (free((void*)a))

#define M_DESCRIPTOR_ARRAY_OPLIST \
    (INIT(M_INIT), INIT_SET(M_INIT_SET), CLEAR(M_CLEAR), TYPE(const NfcEinkScreenDescriptor*))

ARRAY_DEF(EinkScreenDescriptorArray, const NfcEinkScreenDescriptor*, M_DESCRIPTOR_ARRAY_OPLIST);

const NfcEinkScreenDescriptor* nfc_eink_descriptor_get_by_type(NfcEinkScreenType type);

uint8_t nfc_eink_descriptor_filter_by_manufacturer(
    EinkScreenDescriptorArray_t result,
    NfcEinkManufacturer manufacturer);

uint8_t nfc_eink_descriptor_filter_by_screen_size(
    EinkScreenDescriptorArray_t result,
    NfcEinkScreenSize screen_size);