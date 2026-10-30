#include "../debug_mode.h"

void debug_mode_scene_dashboard_on_enter(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);

    DebugModeDashboardStats stats;
    FuriString* text = furi_string_alloc();
    if(case_store_get_dashboard_stats(app->storage, &stats)) {
        furi_string_printf(
            text,
            "Debug Dashboard\n"
            "\n"
            "Open: %lu\n"
            "Resolved: %lu\n"
            "Archived: %lu\n"
            "Total Cases: %lu\n"
            "\n"
            "Entries: %lu\n"
            "Tests: %lu\n"
            "Pass/Fail: %lu/%lu\n"
            "Unknown: %lu\n"
            "Inconclusive: %lu\n"
            "\n"
            "Top Category: %s\n"
            "Recent: %s",
            (unsigned long)stats.open_cases,
            (unsigned long)stats.resolved_cases,
            (unsigned long)stats.archived_cases,
            (unsigned long)stats.total_cases,
            (unsigned long)stats.total_entries,
            (unsigned long)stats.total_tests,
            (unsigned long)stats.pass,
            (unsigned long)stats.fail,
            (unsigned long)stats.unknown,
            (unsigned long)stats.inconclusive,
            stats.most_used_category,
            stats.recent_title[0] ? stats.recent_title : "None");
    } else {
        furi_string_set(text, "Could not build dashboard.");
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdWidget);
}

bool debug_mode_scene_dashboard_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void debug_mode_scene_dashboard_on_exit(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);
}
