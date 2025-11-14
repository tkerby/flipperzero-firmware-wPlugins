#pragma once

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#ifdef __cplusplus
extern "C" {
#endif

// Nintendo Switch Pro Controller VID/PID
#define USB_VID_NINTENDO 0x057E
#define USB_PID_SWITCH_PRO 0x2009

// Button bit positions for Switch Pro Controller
#define SWITCH_BTN_Y (1 << 0)
#define SWITCH_BTN_B (1 << 1)
#define SWITCH_BTN_A (1 << 2)
#define SWITCH_BTN_X (1 << 3)
#define SWITCH_BTN_L (1 << 4)
#define SWITCH_BTN_R (1 << 5)
#define SWITCH_BTN_ZL (1 << 6)
#define SWITCH_BTN_ZR (1 << 7)
#define SWITCH_BTN_MINUS (1 << 8)
#define SWITCH_BTN_PLUS (1 << 9)
#define SWITCH_BTN_LSTICK (1 << 10)
#define SWITCH_BTN_RSTICK (1 << 11)
#define SWITCH_BTN_HOME (1 << 12)
#define SWITCH_BTN_CAPTURE (1 << 13)

// D-Pad values (HAT switch)
#define SWITCH_HAT_UP 0x00
#define SWITCH_HAT_UP_RIGHT 0x01
#define SWITCH_HAT_RIGHT 0x02
#define SWITCH_HAT_DOWN_RIGHT 0x03
#define SWITCH_HAT_DOWN 0x04
#define SWITCH_HAT_DOWN_LEFT 0x05
#define SWITCH_HAT_LEFT 0x06
#define SWITCH_HAT_UP_LEFT 0x07
#define SWITCH_HAT_NEUTRAL 0x08

// Analog stick center position
#define STICK_CENTER 0x8000

// USB descriptor constants
#define USB_CSCP_NoDeviceClass 0x00
#define USB_CSCP_NoDeviceSubclass 0x00
#define USB_CSCP_NoDeviceProtocol 0x00
#define USB_EP0_SIZE 64
#define NO_DESCRIPTOR 0

// USB version macro (creates BCD version)
#define VERSION_BCD(maj, min, rev) \
    ((((maj) & 0xFF) << 8) | (((min) & 0x0F) << 4) | ((rev) & 0x0F))

// USB string descriptor macro
#define USB_STRING_DESC(str) \
    { \
        .bLength = sizeof(struct usb_string_descriptor) + sizeof(str) - 2, \
        .bDescriptorType = USB_DTYPE_STRING, \
        .wString = str \
    }

// USB descriptor types
#define USB_DTYPE_DEVICE 0x01
#define USB_DTYPE_CONFIGURATION 0x02
#define USB_DTYPE_STRING 0x03
#define USB_DTYPE_INTERFACE 0x04
#define USB_DTYPE_ENDPOINT 0x05
#define USB_DTYPE_HID 0x21
#define USB_DTYPE_HID_REPORT 0x22

// USB string descriptor indices
#define UsbDevManuf 1
#define UsbDevProduct 2
#define UsbDevSerial 3

// USB HID class constants
#define USB_CLASS_HID 0x03
#define USB_HID_SUBCLASS_NONBOOT 0x00
#define USB_HID_PROTO_NONBOOT 0x00

// USB configuration attributes and power
#define USB_CFG_ATTR_RESERVED 0x80
#define USB_CFG_POWER_MA(mA) ((mA) / 2)

// USB endpoint types
#define USB_EPTYPE_INTERRUPT 0x03

// USB request types and recipients
#define USB_REQ_RECIPIENT 0x1F
#define USB_REQ_TYPE 0x60
#define USB_REQ_INTERFACE 0x01
#define USB_REQ_STANDARD 0x00
#define USB_REQ_CLASS 0x20

// USB standard requests
#define USB_STD_GET_DESCRIPTOR 0x06

// USB HID class requests
#define USB_HID_SETIDLE 0x0A
#define USB_HID_GETREPORT 0x01

// USB HID country codes
#define USB_HID_COUNTRY_NONE 0x00

// HID endpoint configuration
#define HID_EP_IN 0x81
#define HID_EP_SZ 64
#define HID_INTERVAL 5

// Switch controller state structure
typedef struct {
    uint16_t buttons;
    uint8_t hat;
    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
} SwitchControllerState;

// Initialize USB HID for Switch Pro Controller
bool usb_hid_switch_init();

// Deinitialize USB HID
void usb_hid_switch_deinit();

// Send controller state
bool usb_hid_switch_send_report(SwitchControllerState* state);

// Reset controller state to neutral
void usb_hid_switch_reset_state(SwitchControllerState* state);

#ifdef __cplusplus
}
#endif
