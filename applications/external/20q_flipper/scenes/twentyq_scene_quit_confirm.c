#include "../twentyq_app_i.h"

static void twentyq_scene_quit_confirm_callback(DialogExResult result, void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)result);
}

void twentyq_scene_quit_confirm_on_enter(void* context) {
    TwentyQApp* app = context;
    DialogEx* dialog = app->dialog;

    dialog_ex_reset(dialog);
    dialog_ex_set_header(dialog, "Give up?", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        dialog, "This game will not\nbe counted.", 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog, "Keep going");
    dialog_ex_set_right_button_text(dialog, "Quit");
    dialog_ex_set_result_callback(dialog, twentyq_scene_quit_confirm_callback);
    dialog_ex_set_context(dialog, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewDialog);
}

bool twentyq_scene_quit_confirm_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DialogExResultRight) {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, TwentyQSceneStart);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void twentyq_scene_quit_confirm_on_exit(void* context) {
    TwentyQApp* app = context;
    dialog_ex_reset(app->dialog);
}
