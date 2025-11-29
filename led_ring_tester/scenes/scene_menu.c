#include "../led_ring_tester_app.h"

typedef enum {
    SubmenuIndexConfig,
    SubmenuIndexIndividualTest,
    SubmenuIndexColorSweep,
    SubmenuIndexAllOnOff,
    SubmenuIndexBrightness,
    SubmenuIndexQuickTest,
} SubmenuIndex;

void led_ring_tester_scene_on_enter_menu(void* context) {
    LedRingTesterApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "LED Ring Tester");

    submenu_add_item(submenu, "Configuration", SubmenuIndexConfig,
                    scene_manager_handle_custom_event, app->scene_manager);
    submenu_add_item(submenu, "Individual LED Test", SubmenuIndexIndividualTest,
                    scene_manager_handle_custom_event, app->scene_manager);
    submenu_add_item(submenu, "Color Sweep", SubmenuIndexColorSweep,
                    scene_manager_handle_custom_event, app->scene_manager);
    submenu_add_item(submenu, "All On/Off Test", SubmenuIndexAllOnOff,
                    scene_manager_handle_custom_event, app->scene_manager);
    submenu_add_item(submenu, "Brightness Test", SubmenuIndexBrightness,
                    scene_manager_handle_custom_event, app->scene_manager);
    submenu_add_item(submenu, "Quick Test (Auto)", SubmenuIndexQuickTest,
                    scene_manager_handle_custom_event, app->scene_manager);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewSubmenu);
}

bool led_ring_tester_scene_on_event_menu(void* context, SceneManagerEvent event) {
    LedRingTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexConfig:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneConfig);
            consumed = true;
            break;
        case SubmenuIndexIndividualTest:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneIndividualTest);
            consumed = true;
            break;
        case SubmenuIndexColorSweep:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneColorSweep);
            consumed = true;
            break;
        case SubmenuIndexAllOnOff:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneAllOnOff);
            consumed = true;
            break;
        case SubmenuIndexBrightness:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneBrightness);
            consumed = true;
            break;
        case SubmenuIndexQuickTest:
            scene_manager_next_scene(app->scene_manager, LedRingTesterSceneQuickTest);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void led_ring_tester_scene_on_exit_menu(void* context) {
    LedRingTesterApp* app = context;
    submenu_reset(app->submenu);
}
