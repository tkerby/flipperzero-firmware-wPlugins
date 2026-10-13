// A USB HID interface for the jiggler: a mouse, a keyboard, and a small
// vendor-defined channel the desktop helper uses to report host activity.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define JIG_HOST_MAGIC   0x4A // 'J'
#define JIG_HOST_VERSION 1
#define JIG_PACKET_LEN   8

// Flags in a host packet
#define JIG_HOST_HAS_IDLE (1 << 0)
#define JIG_HOST_HAS_LOCK (1 << 1)
#define JIG_HOST_LOCKED   (1 << 2)

// What the helper last told us, copied out under the USB stack's lock
typedef struct {
    bool valid;
    uint8_t flags;
    uint16_t idle_seconds;
    uint32_t tick; // furi_get_tick when the packet arrived
} JigHostInfo;

void jig_usb_start(void);
void jig_usb_stop(void);
bool jig_usb_is_connected(void);

bool jig_usb_mouse_move(int8_t dx, int8_t dy);
bool jig_usb_key_tap(uint16_t key); // HID_KEYBOARD_* with optional KEY_MOD_*

// Returns true when a new host packet has arrived since the last call
bool jig_usb_take_host_info(JigHostInfo* info);
// Status back to the helper
bool jig_usb_send_status(
    uint8_t state,
    uint16_t next_seconds,
    uint16_t since_move_ms,
    uint32_t moves);
