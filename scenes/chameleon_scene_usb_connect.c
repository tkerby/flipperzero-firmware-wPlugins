#include "../chameleon_app_i.h"

void chameleon_scene_usb_connect_on_enter(void* context) {
    ChameleonApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Connecting...", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "USB Connection", 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);

    // Attempt USB connection
    if(chameleon_app_connect_usb(app)) {
        popup_set_header(app->popup, "Connected!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Device connected\nvia USB", 64, 32, AlignCenter, AlignCenter);
        app->connection_status = ChameleonStatusConnected;
    } else {
        popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Failed to connect\nvia USB", 64, 32, AlignCenter, AlignCenter);
        app->connection_status = ChameleonStatusError;
    }

    // Show result for 2 seconds then return to main menu
    furi_delay_ms(2000);
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChameleonSceneMainMenu);
}

bool chameleon_scene_usb_connect_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_usb_connect_on_exit(void* context) {
    ChameleonApp* app = context;
    popup_reset(app->popup);
}
