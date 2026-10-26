#include "../debug_mode.h"
#include <stdlib.h>
#include <string.h>

#define CASE_LIST_SORT_INDEX 0

static const char* debug_mode_case_list_sort_label(DebugModeCaseListSort sort) {
    switch(sort) {
    case DebugModeCaseListSortPriority:
        return "Sort: Priority";
    case DebugModeCaseListSortCategory:
        return "Sort: Category";
    case DebugModeCaseListSortTitle:
        return "Sort: Title";
    case DebugModeCaseListSortUpdated:
    default:
        return "Sort: Updated";
    }
}

static uint8_t debug_mode_priority_rank(const char* priority) {
    if(strcmp(priority, "High") == 0) return 0;
    if(strcmp(priority, "Medium") == 0) return 1;
    return 2;
}

static void debug_mode_case_list_sort(CaseHeader* headers, uint32_t count, DebugModeCaseListSort sort) {
    for(uint32_t i = 0; i < count; i++) {
        for(uint32_t j = i + 1; j < count; j++) {
            int cmp = 0;
            if(sort == DebugModeCaseListSortPriority) {
                cmp = (int)debug_mode_priority_rank(headers[i].priority) -
                      (int)debug_mode_priority_rank(headers[j].priority);
                if(cmp == 0) cmp = strcmp(headers[j].updated, headers[i].updated);
            } else if(sort == DebugModeCaseListSortCategory) {
                cmp = strcmp(headers[i].category, headers[j].category);
                if(cmp == 0) cmp = strcmp(headers[j].updated, headers[i].updated);
            } else if(sort == DebugModeCaseListSortTitle) {
                cmp = strcmp(headers[i].title, headers[j].title);
            } else {
                cmp = strcmp(headers[j].updated, headers[i].updated);
            }

            if(cmp > 0) {
                CaseHeader tmp = headers[i];
                headers[i] = headers[j];
                headers[j] = tmp;
            }
        }
    }
}

static void debug_mode_scene_case_list_callback(void* context, uint32_t index) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void debug_mode_scene_case_list_on_enter(void* context) {
    DebugMode* app = context;

    CaseHeader* headers = malloc(DEBUG_MODE_MAX_CASES * sizeof(CaseHeader));

    app->case_list_count = case_store_list_headers(
        app->storage,
        app->case_list_show_resolved,
        app->case_list_show_archived,
        headers,
        DEBUG_MODE_MAX_CASES);

    if(app->case_list_count == 0) {
        free(headers);
        snprintf(
            app->message_title,
            sizeof(app->message_title),
            "%s",
            app->case_list_show_archived ?
                "No Archived Cases" :
                (app->case_list_show_resolved ? "No Resolved Cases" : "No Open Cases"));
        snprintf(
            app->message_body,
            sizeof(app->message_body),
            "%s",
            app->case_list_show_archived ?
                "Nothing has been archived yet." :
                (app->case_list_show_resolved ? "Nothing has been resolved yet."
                                              : "Create a case to get started."));
        app->message_return_scene = DebugModeSceneMainMenu;
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        return;
    }

    debug_mode_case_list_sort(headers, app->case_list_count, app->case_list_sort);

    submenu_reset(app->submenu);
    submenu_set_header(
        app->submenu,
        app->case_list_show_archived ?
            "Archived Cases" :
            (app->case_list_show_resolved ? "Resolved Cases" : "Open Cases"));
    submenu_add_item(
        app->submenu,
        debug_mode_case_list_sort_label(app->case_list_sort),
        CASE_LIST_SORT_INDEX,
        debug_mode_scene_case_list_callback,
        app);
    for(uint32_t i = 0; i < app->case_list_count; i++) {
        char label[64];
        snprintf(
            label,
            sizeof(label),
            "%c/%s %s",
            headers[i].priority[0],
            headers[i].category,
            headers[i].title);
        app->case_list_ids[i] = headers[i].id;
        submenu_add_item(app->submenu, label, i + 1, debug_mode_scene_case_list_callback, app);
    }

    free(headers);

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSubmenu);
}

bool debug_mode_scene_case_list_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CASE_LIST_SORT_INDEX) {
            app->case_list_sort = (app->case_list_sort + 1) % 4;
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, DebugModeSceneCaseList);
            consumed = true;
        } else if(event.event > 0 && event.event <= app->case_list_count) {
            app->current_case_id = app->case_list_ids[event.event - 1];
            scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseView);
            consumed = true;
        }
    }

    return consumed;
}

void debug_mode_scene_case_list_on_exit(void* context) {
    DebugMode* app = context;
    submenu_reset(app->submenu);
}
