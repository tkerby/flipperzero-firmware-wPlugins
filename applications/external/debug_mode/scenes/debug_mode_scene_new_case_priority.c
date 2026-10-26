#include "../debug_mode.h"
#include <string.h>

static const char* const priority_options[] = {"High", "Medium", "Low"};

static void debug_mode_scene_new_case_priority_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_new_case_priority_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Priority");

    for(uint32_t i = 0; i < COUNT_OF(priority_options); i++) {
        submenu_add_item(
            app->submenu,
            priority_options[i],
            i,
            debug_mode_scene_new_case_priority_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_new_case_priority_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < COUNT_OF(priority_options)) {
        strncpy(
            app->current_case_priority,
            priority_options[event.event],
            sizeof(app->current_case_priority) - 1);
        app->current_case_priority[sizeof(app->current_case_priority) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, DebugModeSceneNewCaseTitle);
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_new_case_priority_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
