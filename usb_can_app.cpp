#include "usb_can_app_i.hpp"

#include <furi.h>
#include <furi_hal.h>
#include "views/usb_can_view.hpp"

static bool usb_can_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    UsbCanApp* app = (UsbCanApp*)context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool usb_can_app_back_event_callback(void* context) {
    furi_assert(context);
    UsbCanApp* app = (UsbCanApp*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void usb_can_app_tick_event_callback(void* context) {
    furi_assert(context);
    UsbCanApp* app = (UsbCanApp*)context;
    scene_manager_handle_tick_event(app->scene_manager);
}

UsbCanApp* usb_can_app_alloc() {
    UsbCanApp* app = (UsbCanApp*)malloc(sizeof(UsbCanApp));

    app->views.gui = (Gui*)furi_record_open(RECORD_GUI);

    app->scene_manager = scene_manager_alloc(&usb_can_scene_handlers, app);

    app->views.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->views.view_dispatcher);
    view_dispatcher_set_event_callback_context(app->views.view_dispatcher, app);

    // register view callbacks
    view_dispatcher_set_custom_event_callback(
        app->views.view_dispatcher, usb_can_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->views.view_dispatcher, usb_can_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->views.view_dispatcher, usb_can_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(
        app->views.view_dispatcher, app->views.gui, ViewDispatcherTypeFullscreen);

    app->notifications = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);

    // add views

    app->views.menu = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->views.view_dispatcher,
        UsbCanAppViewMenu,
        variable_item_list_get_view(app->views.menu));

    view_dispatcher_add_view(
        app->views.view_dispatcher,
        UsbCanAppViewCfg,
        variable_item_list_get_view(app->views.menu));

    app->views.usb_busy_popup = widget_alloc();
    view_dispatcher_add_view(
        app->views.view_dispatcher,
        UsbCanAppViewUsbBusy,
        widget_get_view(app->views.usb_busy_popup));

    app->views.usb_can = usb_can_view_alloc();
    view_dispatcher_add_view(
        app->views.view_dispatcher, UsbCanAppViewUsbCan, usb_can_get_view(app->views.usb_can));

    app->views.quit_dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->views.view_dispatcher,
        UsbCanAppViewExitConfirm,
        dialog_ex_get_view(app->views.quit_dialog));

    scene_manager_next_scene(app->scene_manager, UsbCanSceneStart);

    return app;
}

void usb_can_app_free(UsbCanApp* app) {
    furi_assert(app);

    // Views
    view_dispatcher_remove_view(app->views.view_dispatcher, UsbCanAppViewMenu);
    view_dispatcher_remove_view(app->views.view_dispatcher, UsbCanAppViewCfg);
    view_dispatcher_remove_view(app->views.view_dispatcher, UsbCanAppViewUsbCan);
    view_dispatcher_remove_view(app->views.view_dispatcher, UsbCanAppViewUsbBusy);
    view_dispatcher_remove_view(app->views.view_dispatcher, UsbCanAppViewExitConfirm);
    variable_item_list_free(app->views.menu);
    widget_free(app->views.usb_busy_popup);
    usb_can_view_free(app->views.usb_can);
    dialog_ex_free(app->views.quit_dialog);

    // View dispatcher
    view_dispatcher_free(app->views.view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

int32_t usb_can_app(void* p) {
    UNUSED(p);
    UsbCanApp* usb_can_app = usb_can_app_alloc();

    view_dispatcher_run(usb_can_app->views.view_dispatcher);

    usb_can_app_free(usb_can_app);

    return 0;
}
