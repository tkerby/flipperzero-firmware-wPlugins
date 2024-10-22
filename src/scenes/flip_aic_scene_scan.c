#include "../flip_aic.h"
#include "flip_aic_scene.h"

static void flip_aic_scene_scan_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(TAG, "Found %d protocol(s):", event.data.protocol_num);
    for (size_t i = 0; i < event.data.protocol_num; i++) {
        NfcProtocol protocol = event.data.protocols[i];
        FURI_LOG_I(TAG, "Protocol %d: %d", i, protocol);
    }

    app->nfc_protocol = event.data.protocols[0];

    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void flip_aic_scene_scan_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(TAG, "Starting scanner");
    nfc_scanner_start(app->nfc_scanner, flip_aic_scene_scan_nfc_scanner_callback, app);
}

bool flip_aic_scene_scan_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if (event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
        case 0:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneRead);
            return true;
        }
    }

    return false;
}

void flip_aic_scene_scan_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(TAG, "Stopping scanner");
    nfc_scanner_stop(app->nfc_scanner);
}
