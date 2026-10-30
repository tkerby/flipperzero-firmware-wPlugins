#include "../debug_mode.h"

static void debug_mode_scene_mark_resolved_confirm_callback(DialogExResult result, void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void debug_mode_scene_mark_resolved_confirm_on_enter(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Resolve Case?", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "This will close the case\n"
        "after you add root cause\n"
        "and lesson learned.",
        64,
        32,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");
    dialog_ex_set_right_button_text(app->dialog_ex, "Continue");
    dialog_ex_set_result_callback(app->dialog_ex, debug_mode_scene_mark_resolved_confirm_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdDialogEx);
}

bool debug_mode_scene_mark_resolved_confirm_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == DialogExResultRight) {
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMarkResolvedRootCause);
        } else if(event.event == DialogExResultLeft) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            consumed = false;
        }
    }

    return consumed;
}

void debug_mode_scene_mark_resolved_confirm_on_exit(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);
}
