#include "../usb_can_app_i.hpp"
#include "../usb_can_bridge.hpp"
#include "usbCan_scene.hpp"
#include "../views/usb_can_view.hpp"

usbCanState UsbCanStateToSet;

void usb_can_scene_set_state(usbCanState state) {
    UsbCanStateToSet = state;
}

void usb_can_scene_usb_can_callback(UsbCanCustomEvent event, void* context) {
    furi_assert(context);
    UsbCanApp* app = (UsbCanApp*)context;
    view_dispatcher_send_custom_event(app->views.view_dispatcher, event);
}

void usb_can_scene_usb_can_on_enter(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;

    usb_can_enable(app, UsbCanStateToSet);
    usb_can_view_set_callback(app->views.usb_can, usb_can_scene_usb_can_callback, app);
    view_dispatcher_switch_to_view(app->views.view_dispatcher, UsbCanAppViewUsbCan);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
}

bool usb_can_scene_usb_can_on_event(void* context, SceneManagerEvent event) {
    UsbCanApp* app = (UsbCanApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(app->scene_manager, UsbCanSceneConfig);
        return true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, UsbCanSceneExitConfirm);
        return true;
    } else if(event.type == SceneManagerEventTypeTick) {
        /*uint32_t tx_cnt_last = scene_usb_can->status.tx_cnt;
        uint32_t rx_cnt_last = scene_usb_can->status.rx_cnt;*/
        usb_can_view_update_state(
            app->views.usb_can, &app->usb_can_bridge->cfg_new, &app->usb_can_bridge->st);
        /*if(tx_cnt_last != scene_usb_can->state.tx_cnt)
            notification_message(app->notifications, &sequence_blink_blue_10);
        if(rx_cnt_last != scene_usb_can->state.rx_cnt)
            notification_message(app->notifications, &sequence_blink_green_10);*/
    }
    return false;
}

void usb_can_scene_usb_can_on_exit(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    usb_can_disable(app->usb_can_bridge);
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
}
