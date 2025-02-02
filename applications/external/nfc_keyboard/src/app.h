#ifndef USB_HID_APP
#define USB_HID_APP

#include "flipper.h"

#define VERSION "1.0.0"
#define TAG "NFC_HID"
#define MAX_NFC_UID_SIZE 32

struct NfcHidApp {
    bool running;
    bool detected;
    FuriString* uid_str;

    ViewPort* view_port;
    Gui* gui;

    FuriHalUsbInterface* usb_mode_prev;

    Nfc* nfc;
	NfcScanner* scanner;
    NfcPoller* poller;
    NfcDevice* device;
};

typedef struct NfcHidApp NfcHidApp;

#endif
