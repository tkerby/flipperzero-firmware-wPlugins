#include "../debug_mode.h"
#include <string.h>

/* Only the first 5 of the 6 entry types are user-selectable here.
   "Resolution" is reserved for the Mark Resolved flow only. */
#define DEBUG_MODE_SELECTABLE_ENTRY_TYPES 5

static void debug_mode_scene_entry_type_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_entry_type_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Add Entry");
    for(uint32_t i = 0; i < DEBUG_MODE_SELECTABLE_ENTRY_TYPES; i++) {
        submenu_add_item(
            app->submenu, debug_mode_entry_types[i], i, debug_mode_scene_entry_type_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_entry_type_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < DEBUG_MODE_SELECTABLE_ENTRY_TYPES) {
        strncpy(
            app->pending_entry_type,
            debug_mode_entry_types[event.event],
            sizeof(app->pending_entry_type) - 1);
        app->pending_entry_type[sizeof(app->pending_entry_type) - 1] = '\0';
        app->pending_entry_text[0] = '\0';
        app->pending_entry_outcome[0] = '\0';
        app->pending_entry_note[0] = '\0';

        if(strcmp(app->pending_entry_type, "Note") == 0) {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryCustomText);
        } else {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryPreset);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_entry_type_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
