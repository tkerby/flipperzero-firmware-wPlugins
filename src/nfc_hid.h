#ifndef NFC_HID_HEADERS
#define NFC_HID_HEADERS

#include "flipper.h"

#define VERSION "1.0.0"
#define TAG "NFC_HID"
#define MAX_NFC_UID_SIZE 32

struct NfcHidApp {
    bool running;
    bool new_uid;
    uint8_t* uid[MAX_NFC_UID_SIZE];
    size_t uid_len;

    ViewPort* view_port;
    Gui* gui;

    FuriHalUsbInterface* usb_mode_prev;

    Nfc* nfc;
	NfcScanner* scanner;
    NfcDevice* device;
};

typedef struct NfcHidApp NfcHidApp;

int32_t nfc_hid_app(void* p);

#endif
