#include "../usb_can_app_i.hpp"
#include "../usb_can_custom_event.hpp"
#include "assets_icons.h"

void usb_can_scene_error_dialog_on_enter(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;

    widget_add_icon_element(app->views.usb_busy_popup, 78, 0, &I_ActiveConnection_50x64);
    widget_add_string_multiline_element(
        app->views.usb_busy_popup,
        3,
        2,
        AlignLeft,
        AlignTop,
        FontPrimary,
        "Connection\nis active!");
    widget_add_string_multiline_element(
        app->views.usb_busy_popup,
        3,
        30,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Disconnect from\nPC or phone to\nuse this function.");

    view_dispatcher_switch_to_view(app->views.view_dispatcher, UsbCanAppViewUsbBusy);
}

bool usb_can_scene_error_dialog_on_event(void* context, SceneManagerEvent event) {
    UsbCanApp* app = (UsbCanApp*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == UsbCanCustomEventErrorBack) {
            if(!scene_manager_previous_scene(app->scene_manager)) {
                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->views.view_dispatcher);
            }
            consumed = true;
        }
    }
    return consumed;
}

void usb_can_scene_error_dialog_on_exit(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    widget_reset(app->views.usb_busy_popup);
}
