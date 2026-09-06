#include "../../app/wmbus_app_i.h"

static void
    wmbus_scene_exit_confirm_callback(GuiButtonType result, InputType type, void* context) {
    WmBusApp* app = context;

    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WmBusCustomEventExit);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WmBusCustomEventStay);
    }
}

void wmbus_scene_exit_confirm_on_enter(void* context) {
    WmBusApp* app = context;
    if(!app || !wmbus_app_ensure_widget(app)) return;

    widget_reset(app->widget);
    widget_add_text_box_element(
        app->widget,
        0U,
        0U,
        128U,
        54U,
        AlignCenter,
        AlignTop,
        "\e#Exit WM-Bus Decoder?\e#\nPackets in memory\nwill be lost",
        false);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Exit", wmbus_scene_exit_confirm_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Stay", wmbus_scene_exit_confirm_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WmBusAppViewWidget);
}

bool wmbus_scene_exit_confirm_on_event(void* context, SceneManagerEvent event) {
    WmBusApp* app = context;

    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == WmBusCustomEventStay)) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && event.event == WmBusCustomEventExit) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    return false;
}

void wmbus_scene_exit_confirm_on_exit(void* context) {
    WmBusApp* app = context;
    if(!app->widget) return;
    widget_reset(app->widget);
}
