#include "../debug_mode.h"

void debug_mode_scene_case_health_on_enter(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);

    CaseHeader header;
    CaseStats stats;
    FuriString* text = furi_string_alloc();

    if(case_store_read_header(app->storage, app->current_case_id, &header) &&
       case_store_get_stats(app->storage, app->current_case_id, &stats)) {
        uint32_t signal = stats.pass + stats.fail + stats.resolutions + stats.lessons;
        uint32_t open_threads = stats.next_steps > stats.resolutions ?
                                    stats.next_steps - stats.resolutions :
                                    0;

        furi_string_printf(
            text,
            "Case Health\n"
            "\n"
            "%s\n"
            "Status: %s\n"
            "Category: %s\n"
            "\n"
            "Entries: %lu\n"
            "Tests: %lu\n"
            "Pass/Fail: %lu/%lu\n"
            "Unknown: %lu\n"
            "Inconclusive: %lu\n"
            "\n"
            "Next Steps: %lu\n"
            "Open Threads: %lu\n"
            "Root Causes: %lu\n"
            "Lessons: %lu\n"
            "\n"
            "Debug Signal: %lu",
            header.title,
            header.status,
            header.category,
            (unsigned long)stats.total_entries,
            (unsigned long)stats.tests,
            (unsigned long)stats.pass,
            (unsigned long)stats.fail,
            (unsigned long)stats.unknown,
            (unsigned long)stats.inconclusive,
            (unsigned long)stats.next_steps,
            (unsigned long)open_threads,
            (unsigned long)stats.root_causes,
            (unsigned long)stats.lessons,
            (unsigned long)signal);
    } else {
        furi_string_set(text, "Could not read case health.");
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdWidget);
}

bool debug_mode_scene_case_health_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void debug_mode_scene_case_health_on_exit(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);
}
