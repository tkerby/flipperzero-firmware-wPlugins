#include "../debug_mode.h"

static void debug_mode_scene_case_action_confirm_callback(DialogExResult result, void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void debug_mode_scene_case_action_confirm_on_enter(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);

    if(app->pending_case_action == DebugModeCaseActionDelete) {
        dialog_ex_set_header(app->dialog_ex, "Delete Case?", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            "This permanently\n"
            "removes the case file.",
            64,
            32,
            AlignCenter,
            AlignCenter);
        dialog_ex_set_right_button_text(app->dialog_ex, "Delete");
    } else {
        dialog_ex_set_header(app->dialog_ex, "Archive Case?", 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            "Hide this case from\n"
            "open/resolved lists.",
            64,
            32,
            AlignCenter,
            AlignCenter);
        dialog_ex_set_right_button_text(app->dialog_ex, "Archive");
    }

    dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
    dialog_ex_set_result_callback(app->dialog_ex, debug_mode_scene_case_action_confirm_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdDialogEx);
}

bool debug_mode_scene_case_action_confirm_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == DialogExResultLeft) {
            scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == DialogExResultRight) {
            bool ok = false;
            if(app->pending_case_action == DebugModeCaseActionDelete) {
                ok = case_store_delete(app->storage, app->current_case_id);
            } else {
                ok = case_store_set_status(app->storage, app->current_case_id, "Archived");
            }

            snprintf(
                app->message_title,
                sizeof(app->message_title),
                "%s",
                ok ? "Done" : "Failed");
            snprintf(
                app->message_body,
                sizeof(app->message_body),
                "%s",
                ok ? "Case list updated." : "Could not update the case.");
            app->message_return_scene = DebugModeSceneMainMenu;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        } else {
            consumed = false;
        }
    }

    return consumed;
}

void debug_mode_scene_case_action_confirm_on_exit(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);
}
