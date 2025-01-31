#include "nfc_hid.h"

/** Draws the UI */
static void nfc_hid_render_callback(Canvas* canvas, void* ctx) {
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

/** Callback for User inputs (buttons) */
static void nfc_hid_input_callback(InputEvent* input_event, void* ctx) {
    NfcHidApp* app = ctx;

    if (input_event->type == InputTypePress && input_event->key == InputKeyBack) {
        app->running = false;
    }
}

static void nfc_hid_scanner_callback(NfcScannerEvent event, void* ctx) {
    NfcHidApp* app = ctx;

    if (event.type == NfcScannerEventTypeDetected) {
        app->running = false;

        // TODO
    }
}

static NfcHidApp* nfchid_alloc() {
    NfcHidApp* app = malloc(sizeof(NfcHidApp));

    app->view_port = view_port_alloc();

    // Open GUI and register view_port
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Save current USB Mode
    app->usb_mode_prev = furi_hal_usb_get_config();

    // Set USB mode to hid
    furi_hal_usb_set_config(&usb_hid, NULL);

    // Enable nfc scanner
    app->nfc = nfc_alloc();
    app->scanner = nfc_scanner_alloc(app->nfc);
    app->device = nfc_device_alloc();

    // Register callbacks
    view_port_draw_callback_set(app->view_port, nfc_hid_render_callback, NULL);
    view_port_input_callback_set(app->view_port, nfc_hid_input_callback, app);
    nfc_scanner_start(app->scanner, nfc_hid_scanner_callback, app);

    app->running = true;

    return app;
}

static void nfchid_free(NfcHidApp* app) {
    // Stop and free nfc scanner
    nfc_scanner_stop(app->scanner);
    nfc_scanner_free(app->scanner);
    nfc_free(app->nfc);

    // Restore previous USB Mode
    furi_hal_usb_set_config(app->usb_mode_prev, NULL);

    // remove & free all stuff created by app
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    free(app);
}

/** Entrypoint */
int32_t nfc_hid_app(void* p) {
    UNUSED(p);

    NfcHidApp* app = nfchid_alloc();

    while(app->running) {
        furi_hal_hid_kb_press(HID_KEYBOARD_C);
        furi_delay_ms(200);
        furi_hal_hid_kb_release(HID_KEYBOARD_C);
        furi_delay_ms(200);

        // Refresh UI
        view_port_update(app->view_port);
    }

    nfchid_free(app);

    return 0;
}
