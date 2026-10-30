#include "../debug_mode.h"
#include <string.h>

typedef enum {
    RootCauseIndexConfig,
    RootCauseIndexSoftware,
    RootCauseIndexHardware,
    RootCauseIndexUserError,
    RootCauseIndexExternal,
    RootCauseIndexUnknown,
} RootCauseIndex;

static const char* const root_cause_options[] = {
    "Config",
    "Software",
    "Hardware",
    "User Error",
    "External",
    "Unknown",
};

static void debug_mode_scene_mark_resolved_root_cause_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_mark_resolved_root_cause_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Root Cause");

    for(uint32_t i = 0; i < COUNT_OF(root_cause_options); i++) {
        submenu_add_item(
            app->submenu,
            root_cause_options[i],
            i,
            debug_mode_scene_mark_resolved_root_cause_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_mark_resolved_root_cause_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < COUNT_OF(root_cause_options)) {
        strncpy(
            app->pending_root_cause,
            root_cause_options[event.event],
            sizeof(app->pending_root_cause) - 1);
        app->pending_root_cause[sizeof(app->pending_root_cause) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMarkResolvedText);
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_mark_resolved_root_cause_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
