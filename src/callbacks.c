#include "callbacks.h"

void nfc_hid_render_callback(Canvas* canvas, void* ctx) {
    NfcHidApp* app = ctx;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "NFC HID Scanner");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "v");
    canvas_draw_str(canvas, 96, 10, VERSION);
    canvas_draw_str(canvas, 0, 20, "Scan a NFC Card");

    if (app->scanned) {
        canvas_draw_str(canvas, 0, 30, "Scanned: Y");
    } else {
        canvas_draw_str(canvas, 0, 30, "Scanned: N");
    }

    canvas_draw_str(canvas, 0, 63, "Press [back] to exit");
}

void nfc_hid_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    NfcHidApp* app = ctx;

    if (input_event->type == InputTypePress && input_event->key == InputKeyBack) {
        app->running = false;
    }
}

void nfc_hid_scanner_callback(NfcScannerEvent event, void* ctx) {
    NfcHidApp* app = ctx;
    app->running = false;

    if (event.type == NfcScannerEventTypeDetected) {
        app->running = false;
    }
}
