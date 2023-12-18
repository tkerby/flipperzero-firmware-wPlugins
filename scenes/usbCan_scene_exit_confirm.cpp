#include "../usb_can_app_i.hpp"

void usb_can_scene_exit_confirm_dialog_callback(DialogExResult result, void* context) {
    UsbCanApp* app = (UsbCanApp*)context;

    view_dispatcher_send_custom_event(app->views.view_dispatcher, result);
}

void usb_can_scene_exit_confirm_on_enter(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    DialogEx* dialog = app->views.quit_dialog;

    dialog_ex_set_context(dialog, app);
    dialog_ex_set_left_button_text(dialog, "Exit");
    dialog_ex_set_right_button_text(dialog, "Stay");
    dialog_ex_set_header(dialog, "Exit USB-CAN?", 22, 12, AlignLeft, AlignTop);
    dialog_ex_set_result_callback(dialog, usb_can_scene_exit_confirm_dialog_callback);

    view_dispatcher_switch_to_view(app->views.view_dispatcher, UsbCanAppViewExitConfirm);
}

bool usb_can_scene_exit_confirm_on_event(void* context, SceneManagerEvent event) {
    UsbCanApp* app = (UsbCanApp*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultRight) {
            consumed = scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == DialogExResultLeft) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, UsbCanSceneStart);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }

    return consumed;
}

void usb_can_scene_exit_confirm_on_exit(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;

    // Clean view
    dialog_ex_reset(app->views.quit_dialog);
}
