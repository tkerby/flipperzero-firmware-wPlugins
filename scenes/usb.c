#include "../firestring.h"

void fire_string_scene_on_enter_usb(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_usb");
    FireString* app = context;
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 0, 32, &I_Connect_me_62x31);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);
}

/** main menu event handler - switches scene based on the event */
bool fire_string_scene_on_event_usb(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "fire_string_scene_on_event_usb");
    furi_check(context);

    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        default:
            break;
        }
        break;
    case SceneManagerEventTypeBack:
        consumed = true;
        scene_manager_previous_scene(app->scene_manager);
        break;
    default:
        break;
    }
    return consumed;
}

void fire_string_scene_on_exit_usb(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_usb");
    furi_check(context);

    FireString* app = context;
    widget_reset(app->widget);
}
