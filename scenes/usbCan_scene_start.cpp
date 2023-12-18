#include "../usb_can_app_i.hpp"
#include <furi_hal_power.h>
#include <furi_hal_usb.h>
#include <dolphin/dolphin.h>
#include "usbCan_scene_setState.hpp"

typedef enum { UsbCanItemUsbCan, UsbCanItemTestUsb, UsbCanItemTestCan } UsbCanItem;

static void usb_can_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    UsbCanApp* app = (UsbCanApp*)context;
    scene_manager_set_scene_state(app->scene_manager, UsbCanSceneStart, index);
    switch(index) {
    case UsbCanItemTestUsb:
        usb_can_scene_set_state(UsbCanLoopBackTestState);
        view_dispatcher_send_custom_event(app->views.view_dispatcher, UsbCanCustomEventTestUsb);
        break;
    case UsbCanItemTestCan:
        usb_can_scene_set_state(UsbCanPingTestState);
        view_dispatcher_send_custom_event(app->views.view_dispatcher, UsbCanCustomEventTestCan);
        break;
    case UsbCanItemUsbCan:
        usb_can_scene_set_state(UsbCanNominalState);
        view_dispatcher_send_custom_event(app->views.view_dispatcher, UsbCanCustomEventUsbCan);
        break;
    }
}

void usb_can_scene_start_on_enter(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    VariableItemList* var_item_list = app->views.menu;

    variable_item_list_set_enter_callback(
        var_item_list, usb_can_scene_start_var_list_enter_callback, app);

    variable_item_list_add(var_item_list, "USB-CAN Bridge", 0, NULL, NULL);

    variable_item_list_add(var_item_list, "TEST USB LOOPBACK", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "TEST CAN", 0, NULL, NULL);

    furi_hal_power_enable_otg();

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, UsbCanSceneStart));

    view_dispatcher_switch_to_view(app->views.view_dispatcher, UsbCanAppViewMenu);
}

bool usb_can_scene_start_on_event(void* context, SceneManagerEvent event) {
    UsbCanApp* app = (UsbCanApp*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case UsbCanCustomEventTestCan:
        case UsbCanCustomEventTestUsb:
        case UsbCanCustomEventUsbCan:
            if(!furi_hal_usb_is_locked()) {
                //dolphin_deed(DolphinDeedusb_canUartBridge);
                scene_manager_next_scene(app->scene_manager, UsbCanSceneUsbCan);
            } else {
                scene_manager_next_scene(app->scene_manager, UsbCanSceneErrorDialog);
            }
            consumed = true;
            break;
        }
    }
    return consumed;
}

void usb_can_scene_start_on_exit(void* context) {
    UsbCanApp* app = (UsbCanApp*)context;
    variable_item_list_reset(app->views.menu);
}
