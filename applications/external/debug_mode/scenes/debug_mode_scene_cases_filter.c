#include "../debug_mode.h"

typedef enum {
    CasesFilterIndexOpen,
    CasesFilterIndexResolved,
    CasesFilterIndexArchived,
} CasesFilterIndex;

static void debug_mode_scene_cases_filter_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_cases_filter_on_enter(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Cases");
    submenu_add_item(
        app->submenu, "Open", CasesFilterIndexOpen, debug_mode_scene_cases_filter_callback, app);
    submenu_add_item(
        app->submenu,
        "Resolved",
        CasesFilterIndexResolved,
        debug_mode_scene_cases_filter_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Archived",
        CasesFilterIndexArchived,
        debug_mode_scene_cases_filter_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_cases_filter_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case CasesFilterIndexOpen:
            app->case_list_show_resolved = false;
            app->case_list_show_archived = false;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseList);
            break;
        case CasesFilterIndexResolved:
            app->case_list_show_resolved = true;
            app->case_list_show_archived = false;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseList);
            break;
        case CasesFilterIndexArchived:
            app->case_list_show_resolved = false;
            app->case_list_show_archived = true;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseList);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void debug_mode_scene_cases_filter_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
