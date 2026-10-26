#include "../debug_mode.h"
#include <string.h>

typedef enum {
    MainMenuIndexStartDebugging,
    MainMenuIndexCases,
    MainMenuIndexNfcScan,
    MainMenuIndexDashboard,
    MainMenuIndexAbout,
} MainMenuIndex;

static void debug_mode_scene_main_menu_submenu_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_main_menu_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Debug Mode");
    submenu_add_item(
        app->submenu,
        "Start Debugging",
        MainMenuIndexStartDebugging,
        debug_mode_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Cases",
        MainMenuIndexCases,
        debug_mode_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Scan NFC Tag",
        MainMenuIndexNfcScan,
        debug_mode_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Dashboard",
        MainMenuIndexDashboard,
        debug_mode_scene_main_menu_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", MainMenuIndexAbout, debug_mode_scene_main_menu_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case MainMenuIndexStartDebugging:
            app->pending_template_id = 0;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneGuidedTemplate);
            break;
        case MainMenuIndexCases:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCasesFilter);
            break;
        case MainMenuIndexDashboard:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneDashboard);
            break;
        case MainMenuIndexNfcScan:
            app->nfc_mode = DebugModeNfcModeOpen;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneNfcScan);
            break;
        case MainMenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void debug_mode_scene_main_menu_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
