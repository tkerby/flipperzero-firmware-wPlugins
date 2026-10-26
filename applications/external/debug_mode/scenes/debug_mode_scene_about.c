#include "../debug_mode.h"

void debug_mode_scene_about_on_enter(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);

    const char* text =
        "Debug Mode v0.5\n"
        "\n"
        "A pocket debugging notebook\n"
        "for messy real problems.\n"
        "\n"
        "Capture symptoms, tests,\n"
        "results, next steps, notes,\n"
        "and resolutions before the\n"
        "thread gets lost.\n"
        "\n"
        "Built by Alex Dean after\n"
        "running into enough problems\n"
        "to need a system for solving\n"
        "them.\n"
        "\n"
        "Saves cases to\n"
        "/ext/apps_data/debug_mode\n"
        "\n"
        "Export cases as text or\n"
        "Markdown reports.";
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdWidget);
}

bool debug_mode_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void debug_mode_scene_about_on_exit(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);
}
