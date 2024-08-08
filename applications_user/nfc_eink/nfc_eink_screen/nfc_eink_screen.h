#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_common.h>
#include <nfc/nfc_device.h>

typedef enum {
    NfcEinkTypeWaveshare2n13inch,
    NfcEinkTypeWaveshare2n9inch,
    //NfcEinkTypeWaveshare4n2inch,

    NfcEinkTypeNum
} NfcEinkType;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t data_block_size;
    NfcEinkType screen_type;
    const char* name;
    //TODO: add here possible function pointers for handlers
} NfcEinkScreenDescriptor; //TODO:NfcEinkScreenBase

typedef struct {
    NfcDevice* nfc_device;
    uint8_t* image_data;
    uint16_t image_size;
    uint16_t received_data;
    NfcEinkScreenDescriptor base;
} NfcEinkScreen;

const char* nfc_eink_screen_get_name(NfcEinkType type);
NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkType type);
void nfc_eink_screen_free(NfcEinkScreen* screen);