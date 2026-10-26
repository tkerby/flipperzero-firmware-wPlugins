#include "../debug_mode.h"
#include <string.h>

typedef enum {
    CaseViewActionAddEntry,
    CaseViewActionTimeline,
    CaseViewActionHealth,
    CaseViewActionMarkResolved,
    CaseViewActionMore,
} CaseViewAction;

static void debug_mode_scene_case_view_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_case_view_on_enter(void* context) {
    DebugMode* app = context;

    /* Re-read from disk every time we enter - status/updated may have
       changed (e.g. Mark Resolved) since we were last here. */
    CaseHeader header;
    if(case_store_read_header(app->storage, app->current_case_id, &header)) {
        strncpy(app->current_case_title, header.title, sizeof(app->current_case_title) - 1);
        app->current_case_title[sizeof(app->current_case_title) - 1] = '\0';
        strncpy(app->current_case_status, header.status, sizeof(app->current_case_status) - 1);
        app->current_case_status[sizeof(app->current_case_status) - 1] = '\0';
        strncpy(app->current_case_priority, header.priority, sizeof(app->current_case_priority) - 1);
        app->current_case_priority[sizeof(app->current_case_priority) - 1] = '\0';
    }
    uint32_t entry_count = case_store_count_entries(app->storage, app->current_case_id);

    submenu_reset(app->submenu);

    char header_text[80];
    snprintf(
        header_text,
        sizeof(header_text),
        "%s (%s, %lu)",
        app->current_case_title,
        app->current_case_priority,
        (unsigned long)entry_count);
    submenu_set_header(app->submenu, header_text);

    submenu_add_item(
        app->submenu,
        "Add Entry",
        CaseViewActionAddEntry,
        debug_mode_scene_case_view_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Timeline",
        CaseViewActionTimeline,
        debug_mode_scene_case_view_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Case Health",
        CaseViewActionHealth,
        debug_mode_scene_case_view_callback,
        app);
    if(strcmp(app->current_case_status, "Resolved") != 0) {
        submenu_add_item(
            app->submenu,
            "Resolve",
            CaseViewActionMarkResolved,
            debug_mode_scene_case_view_callback,
            app);
    }
    submenu_add_item(
        app->submenu,
        "More...",
        CaseViewActionMore,
        debug_mode_scene_case_view_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_case_view_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case CaseViewActionAddEntry:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryType);
            break;
        case CaseViewActionTimeline:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneTimeline);
            break;
        case CaseViewActionHealth:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseHealth);
            break;
        case CaseViewActionMarkResolved:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMarkResolvedConfirm);
            break;
        case CaseViewActionMore:
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseMore);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void debug_mode_scene_case_view_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
