#include "../chameleon_app_i.h"

static void chameleon_scene_ble_connect_submenu_callback(void* context, uint32_t index) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void chameleon_scene_ble_connect_on_enter(void* context) {
    ChameleonApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    // Add found devices to submenu
    size_t device_count = ble_handler_get_device_count(app->ble_handler);
    for(size_t i = 0; i < device_count; i++) {
        const char* device_name = ble_handler_get_device_name(app->ble_handler, i);
        submenu_add_item(submenu, device_name, i, chameleon_scene_ble_connect_submenu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
}

bool chameleon_scene_ble_connect_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Connect to selected device
        size_t device_index = event.event;

        popup_reset(app->popup);
        popup_set_header(app->popup, "Connecting...", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, "BLE Connection", 64, 32, AlignCenter, AlignCenter);
        view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);

        if(ble_handler_connect(app->ble_handler, device_index)) {
            popup_set_header(app->popup, "Connected!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Device connected\nvia Bluetooth", 64, 32, AlignCenter, AlignCenter);
            app->connection_status = ChameleonStatusConnected;
        } else {
            popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Failed to connect", 64, 32, AlignCenter, AlignCenter);
            app->connection_status = ChameleonStatusError;
        }

        furi_delay_ms(2000);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChameleonSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void chameleon_scene_ble_connect_on_exit(void* context) {
    ChameleonApp* app = context;
    submenu_reset(app->submenu);
    popup_reset(app->popup);
}
