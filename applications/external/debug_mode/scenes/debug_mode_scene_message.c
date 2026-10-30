#include "../debug_mode.h"

static void debug_mode_scene_message_callback(DialogExResult result, void* context) {
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void debug_mode_scene_message_on_enter(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, app->message_title, 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, app->message_body, 64, 34, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(app->dialog_ex, "OK");
    dialog_ex_set_result_callback(app->dialog_ex, debug_mode_scene_message_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdDialogEx);
}

bool debug_mode_scene_message_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultCenter) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, app->message_return_scene)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DebugModeSceneMainMenu);
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_message_on_exit(void* context) {
    DebugMode* app = context;
    dialog_ex_reset(app->dialog_ex);
}
