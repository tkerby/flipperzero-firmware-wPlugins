#ifndef NFC_HID_HEADERS
#define NFC_HID_HEADERS

#include "flipper.h"

#define VERSION "1.0.0"
#define TAG "NFC_HID"

struct NfcHidApp {
    bool running;

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
