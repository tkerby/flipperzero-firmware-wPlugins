#include "flipperpwn.h"

#define TAG "FPwn"

/* ----------------------------------------------------------------------------
 * fpwn_os_detect()
 *
 * Heuristic: probe USB HID LED feedback to fingerprint the host OS.
 *
 * Phase 0 — Connectivity check (CapsLock probe)
 *   Toggle CapsLock and verify the host echoes an LED update within 50 ms.
 *   If no echo arrives the USB HID stack is not yet active or the host is
 *   unresponsive — return FPwnOSUnknown immediately and skip all further
 *   probing to avoid leaving the host in a dirty keyboard state.
 *
 * Phase 1 — NumLock probe (macOS discriminator)
 *   macOS treats external USB keyboards as if they have no NumLock key; the
 *   host OS simply does not reflect NumLock LED changes back to the device.
 *   Windows and Linux both acknowledge NumLock toggles via the LED report.
 *
 * Phase 2 — ScrollLock probe (Windows vs. Linux discriminator)
 *   Modern Linux desktop environments (X11/Wayland) do not maintain a
 *   ScrollLock LED state; Windows does.  If the ScrollLock bit changes in the
 *   LED report after a toggle, the host is Windows; otherwise it is Linux.
 *
 * After all probing, any LEDs that were changed are toggled back to their
 * original state so the user's keyboard LEDs are restored.
 * --------------------------------------------------------------------------*/

FPwnOS fpwn_os_detect(void) {
    /* -------------------------------------------------------------------------
     * Phase 0: connectivity / CapsLock pre-flight
     * ---------------------------------------------------------------------- */
    uint8_t led_initial = furi_hal_hid_get_led_state();

    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(50);

    uint8_t led_after_caps = furi_hal_hid_get_led_state();

    bool caps_changed = ((led_after_caps ^ led_initial) & HID_KB_LED_CAPS) != 0;

    if(!caps_changed) {
        /*
         * Host did not echo a CapsLock LED update.  Either USB HID is not
         * active yet, or this is a host configuration that suppresses LED
         * feedback entirely.  Bail out without further probing.
         */
        FURI_LOG_I(
            TAG, "OS detect: no CapsLock LED response — USB HID inactive or host unresponsive");
        return FPwnOSUnknown;
    }

    /* Restore CapsLock to its original state before continuing. */
    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(50);

    /* -------------------------------------------------------------------------
     * Phase 1: NumLock probe — distinguish macOS from Windows/Linux
     * ---------------------------------------------------------------------- */
    uint8_t led_before_num = furi_hal_hid_get_led_state();

    furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
    furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(100);

    uint8_t led_after_num = furi_hal_hid_get_led_state();

    bool num_changed = ((led_after_num ^ led_before_num) & HID_KB_LED_NUM) != 0;

    if(!num_changed) {
        /*
         * Host ignored the NumLock toggle — characteristic of macOS, which
         * does not maintain a NumLock LED for external keyboards.
         */
        FURI_LOG_I(TAG, "OS detect: NumLock LED unchanged → macOS");
        return FPwnOSMac;
    }

    /* NumLock changed — restore it before moving to Phase 2. */
    furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
    furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(50);

    /* -------------------------------------------------------------------------
     * Phase 2: ScrollLock probe — distinguish Windows from Linux
     * ---------------------------------------------------------------------- */
    uint8_t led_before_scroll = furi_hal_hid_get_led_state();

    furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
    furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
    furi_delay_ms(100);

    uint8_t led_after_scroll = furi_hal_hid_get_led_state();

    bool scroll_changed = ((led_after_scroll ^ led_before_scroll) & HID_KB_LED_SCROLL) != 0;

    if(scroll_changed) {
        /* Restore ScrollLock before returning. */
        furi_hal_hid_kb_press(HID_KEYBOARD_SCROLL_LOCK);
        furi_hal_hid_kb_release(HID_KEYBOARD_SCROLL_LOCK);
        furi_delay_ms(50);

        FURI_LOG_I(TAG, "OS detect: ScrollLock LED toggled → Windows");
        return FPwnOSWindows;
    }

    /*
     * ScrollLock LED did not change — Linux desktop environments (both X11
     * and Wayland compositors) typically do not propagate ScrollLock LED
     * state back to USB HID devices.
     */
    FURI_LOG_I(TAG, "OS detect: ScrollLock LED unchanged → Linux");
    return FPwnOSLinux;
}

/* ----------------------------------------------------------------------------
 * fpwn_os_name()
 * --------------------------------------------------------------------------*/

const char* fpwn_os_name(FPwnOS os) {
    switch(os) {
    case FPwnOSWindows:
        return "Windows";
    case FPwnOSMac:
        return "macOS";
    case FPwnOSLinux:
        return "Linux";
    default:
        return "Unknown";
    }
}
