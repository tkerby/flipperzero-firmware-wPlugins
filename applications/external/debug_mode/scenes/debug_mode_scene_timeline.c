#include "../debug_mode.h"

void debug_mode_scene_timeline_on_enter(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);

    FuriString* text = case_store_build_timeline_text(app->storage, app->current_case_id);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdWidget);
}

bool debug_mode_scene_timeline_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* No custom events from this view - hardware Back pops to Case View
       via the default scene manager behavior. */
    return false;
}

void debug_mode_scene_timeline_on_exit(void* context) {
    DebugMode* app = context;
    widget_reset(app->widget);
}
