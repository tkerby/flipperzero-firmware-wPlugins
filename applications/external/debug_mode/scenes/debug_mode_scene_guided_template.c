#include "../debug_mode.h"
#include <string.h>

typedef struct {
    const char* label;
    const char* category;
    const char* priority;
} DebugModeTemplate;

static const DebugModeTemplate debug_mode_templates[] = {
    {"Quick Blank", "", ""},
    {"Hardware Issue", "Hardware", "High"},
    {"Software Install", "Software", "Medium"},
    {"Network Problem", "Network", "High"},
    {"Flipper Test", "Flipper", "Medium"},
    {"Portfolio Bug", "Software", "Medium"},
};

static void debug_mode_scene_guided_template_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_guided_template_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Case Template");

    for(uint32_t i = 0; i < COUNT_OF(debug_mode_templates); i++) {
        submenu_add_item(
            app->submenu,
            debug_mode_templates[i].label,
            i,
            debug_mode_scene_guided_template_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_guided_template_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < COUNT_OF(debug_mode_templates)) {
        const DebugModeTemplate* selected = &debug_mode_templates[event.event];
        if(event.event == 0) {
            app->pending_template_id = 0;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneNewCaseCategory);
        } else {
            app->pending_template_id = event.event;
            strncpy(
                app->current_case_category,
                selected->category,
                sizeof(app->current_case_category) - 1);
            app->current_case_category[sizeof(app->current_case_category) - 1] = '\0';
            strncpy(
                app->current_case_priority,
                selected->priority,
                sizeof(app->current_case_priority) - 1);
            app->current_case_priority[sizeof(app->current_case_priority) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, DebugModeSceneNewCaseTitle);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_guided_template_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
