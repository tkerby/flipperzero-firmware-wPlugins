#include "../chameleon_app_i.h"

void chameleon_scene_ble_scan_on_enter(void* context) {
    ChameleonApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Scanning...", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Looking for\nChameleon devices", 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);

    // Initialize BLE and start scan
    if(chameleon_app_connect_ble(app)) {
        // Scan started successfully, wait for results
        furi_delay_ms(3000);

        // Check if any devices were found
        size_t device_count = ble_handler_get_device_count(app->ble_handler);

        if(device_count > 0) {
            popup_set_header(app->popup, "Found Devices", 64, 10, AlignCenter, AlignTop);
            snprintf(app->text_buffer, sizeof(app->text_buffer), "Found %zu device(s)", device_count);
            popup_set_text(app->popup, app->text_buffer, 64, 32, AlignCenter, AlignCenter);
            furi_delay_ms(1500);

            // Navigate to device selection
            scene_manager_next_scene(app->scene_manager, ChameleonSceneBleConnect);
        } else {
            popup_set_header(app->popup, "No Devices", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "No Chameleon\ndevices found", 64, 32, AlignCenter, AlignCenter);
            furi_delay_ms(2000);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChameleonSceneMainMenu);
        }
    } else {
        popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, "BLE initialization\nfailed", 64, 32, AlignCenter, AlignCenter);
        furi_delay_ms(2000);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChameleonSceneMainMenu);
    }
}

bool chameleon_scene_ble_scan_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_ble_scan_on_exit(void* context) {
    ChameleonApp* app = context;
    popup_reset(app->popup);
}
