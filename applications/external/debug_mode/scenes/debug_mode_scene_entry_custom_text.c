#include "../debug_mode.h"
#include <string.h>

static void debug_mode_scene_entry_custom_text_callback(void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void debug_mode_scene_entry_custom_text_on_enter(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
    app->pending_entry_text[0] = '\0';

    char header[32];
    snprintf(header, sizeof(header), "%s Details", app->pending_entry_type);
    text_input_set_header_text(app->text_input, header);
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        debug_mode_scene_entry_custom_text_callback,
        app,
        app->pending_entry_text,
        sizeof(app->pending_entry_text),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdTextInput);
}

bool debug_mode_scene_entry_custom_text_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(strcmp(app->pending_entry_type, "Test") == 0) {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryOutcome);
        } else if(strcmp(app->pending_entry_type, "Note") == 0) {
            /* Note's own text IS the note - no separate note-on-a-note step. */
            debug_mode_save_pending_entry_and_return(app);
        } else {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneEntryNoteChoice);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_entry_custom_text_on_exit(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
}
