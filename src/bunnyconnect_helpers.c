#include "../lib/bunnyconnect_helpers.h"
#include <furi.h>
#include <furi_hal_usb_hid.h>

void bunnyconnect_send_key_press(uint16_t key) {
    furi_hal_hid_kb_press(key);
}

void bunnyconnect_send_key_release(uint16_t key) {
    furi_hal_hid_kb_release(key);
}

void bunnyconnect_send_string(const char* string) {
    if(!string) return;

    for(size_t i = 0; i < strlen(string); i++) {
        uint16_t key = bunnyconnect_char_to_hid_key(string[i]);
        if(key != HID_KEYBOARD_NONE) {
            bunnyconnect_send_key_press(key);
            bunnyconnect_send_key_release(key);
            furi_delay_ms(10); // Small delay between keystrokes
        }
    }
}

void bunnyconnect_send_enter(void) {
    bunnyconnect_send_key_press(HID_KEYBOARD_RETURN);
    bunnyconnect_send_key_release(HID_KEYBOARD_RETURN);
}

void bunnyconnect_send_backspace(void) {
    bunnyconnect_send_key_press(HID_KEYBOARD_DELETE);
    bunnyconnect_send_key_release(HID_KEYBOARD_DELETE);
}

uint16_t bunnyconnect_char_to_hid_key(char c) {
    return HID_ASCII_TO_KEY(c);
}
