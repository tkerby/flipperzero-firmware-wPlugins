#include "../debug_mode.h"

static void debug_mode_scene_mark_resolved_lesson_text_callback(void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void debug_mode_scene_mark_resolved_lesson_text_on_enter(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
    app->pending_entry_note[0] = '\0';

    text_input_set_header_text(app->text_input, "Lesson Learned");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        debug_mode_scene_mark_resolved_lesson_text_callback,
        app,
        app->pending_entry_note,
        sizeof(app->pending_entry_note),
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdTextInput);
}

bool debug_mode_scene_mark_resolved_lesson_text_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        bool ok = case_store_append_entry(
            app->storage, app->current_case_id, "Root Cause", app->pending_root_cause, "", "");
        if(ok) {
            ok = case_store_append_entry(
                app->storage, app->current_case_id, "Resolution", app->pending_entry_text, "", "");
        }
        if(ok) {
            ok = case_store_append_entry(
                app->storage, app->current_case_id, "Lesson", app->pending_entry_note, "", "");
        }
        if(ok) {
            ok = case_store_set_status(app->storage, app->current_case_id, "Resolved");
        }

        if(!ok) {
            snprintf(app->message_title, sizeof(app->message_title), "Save Failed");
            snprintf(
                app->message_body,
                sizeof(app->message_body),
                "Could not mark the case resolved.");
            app->message_return_scene = DebugModeSceneCaseView;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        } else {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DebugModeSceneCaseView);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_mark_resolved_lesson_text_on_exit(void* context) {
    DebugMode* app = context;
    text_input_reset(app->text_input);
}
