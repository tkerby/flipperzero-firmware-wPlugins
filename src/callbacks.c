#include "callbacks.h"

void nfc_hid_render_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "NFC HID Scanner");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "v");
    canvas_draw_str(canvas, 96, 10, VERSION);
    canvas_draw_str(canvas, 0, 40, "Scan a NFC Card");
    canvas_draw_str(canvas, 0, 63, "Press [back] to exit");
}

void nfc_hid_input_callback(InputEvent* input_event, void* ctx) {
    NfcHidApp* app = ctx;

    if (input_event->type == InputTypePress && input_event->key == InputKeyBack) {
        app->running = false;
    }
}

void nfc_hid_scanner_callback(NfcScannerEvent event, void* ctx) {
    NfcHidApp* app = ctx;

    if (event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        
        app->new_uid = true;
    }
}
