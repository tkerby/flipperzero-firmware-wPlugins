#include "../debug_mode.h"
#include <string.h>

static void debug_mode_scene_entry_outcome_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_entry_outcome_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Test Outcome");
    for(uint32_t i = 0; i < DEBUG_MODE_OUTCOME_COUNT; i++) {
        submenu_add_item(
            app->submenu, debug_mode_outcomes[i], i, debug_mode_scene_entry_outcome_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_entry_outcome_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < DEBUG_MODE_OUTCOME_COUNT) {
        strncpy(
            app->pending_entry_outcome,
            debug_mode_outcomes[event.event],
            sizeof(app->pending_entry_outcome) - 1);
        app->pending_entry_outcome[sizeof(app->pending_entry_outcome) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryNoteChoice);
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_entry_outcome_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
