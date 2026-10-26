#include "../debug_mode.h"

static void debug_mode_scene_entry_note_text_callback(void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void debug_mode_scene_entry_note_text_on_enter(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
    app->pending_entry_note[0] = '\0';
    text_input_set_header_text(app->text_input, "Note (optional)");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input,
        debug_mode_scene_entry_note_text_callback,
        app,
        app->pending_entry_note,
        sizeof(app->pending_entry_note),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdTextInput);
}

bool debug_mode_scene_entry_note_text_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        debug_mode_save_pending_entry_and_return(app);
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_entry_note_text_on_exit(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
}
