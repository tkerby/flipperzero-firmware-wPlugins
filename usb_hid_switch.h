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
