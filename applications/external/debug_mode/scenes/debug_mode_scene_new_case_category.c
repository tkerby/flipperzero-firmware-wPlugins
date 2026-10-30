#include "../debug_mode.h"
#include <string.h>

static void debug_mode_scene_new_case_category_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_new_case_category_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Category");
    for(uint32_t i = 0; i < DEBUG_MODE_CATEGORY_COUNT; i++) {
        submenu_add_item(
            app->submenu,
            debug_mode_categories[i],
            i,
            debug_mode_scene_new_case_category_callback,
            app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_new_case_category_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < DEBUG_MODE_CATEGORY_COUNT) {
            strncpy(
                app->current_case_category,
                debug_mode_categories[event.event],
                sizeof(app->current_case_category) - 1);
            app->current_case_category[sizeof(app->current_case_category) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, DebugModeSceneNewCasePriority);
            consumed = true;
        }
    }

    return consumed;
}

void debug_mode_scene_new_case_category_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
