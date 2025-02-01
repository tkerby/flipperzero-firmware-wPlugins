#include "keyboard.h"

uint16_t get_key(char c) {
    if (c >= 'A' && c <= 'Z') {
        return HID_KEYBOARD_A + (c - 'A');
    }

    if (c >= 'a' && c <= 'z') {
        return HID_KEYBOARD_A + (c - 'a');
    }

    if (c >= '0' && c <= '9') {
        return HID_KEYBOARD_0 + (c - '0');
    }

    switch (c) {
    case ' ':
        return HID_KEYBOARD_SPACEBAR;
    case '\n':
        return HID_KEYBOARD_RETURN;
    }

    return 0;
}

bool is_uppercase(char c) {
    return (c >= 'A' && c <= 'Z');
}

void capslock() {
    furi_hal_hid_kb_press(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(HID_KEYBOARD_CAPS_LOCK);
    furi_delay_ms(KB_PRESS_DELAY);
}

void numlock() {
    furi_hal_hid_kb_press(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(HID_KEYPAD_NUMLOCK);
    furi_delay_ms(KB_PRESS_DELAY);
}

void write_char(char c) {
    uint16_t key = get_key(c);
    bool uppercase = is_uppercase(c);

    if (c == 0) {
        return;
    }

    if (uppercase) {
        capslock();
    }

    furi_hal_hid_kb_press(key);
    furi_delay_ms(KB_PRESS_DELAY);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(KB_PRESS_DELAY);

    if (uppercase) {
        capslock();
    }
}

void write_string(char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        write_char(str[i]);
    }
}
