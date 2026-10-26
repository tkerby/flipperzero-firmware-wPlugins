#include "include/app/wc_app.h"

#include "include/platform/wc_clock_furi.h"
#include "include/platform/wc_store_furi.h"
#include "include/scenes/wc_scene.h"

#include <furi.h>

static bool wc_custom_event_callback(void *context, uint32_t event) {
    WcApp *app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wc_back_event_callback(void *context) {
    WcApp *app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static WcApp *wc_app_alloc(void) {
    WcApp *app = malloc(sizeof(WcApp));
    if (!app) {
        return NULL; // the entry point exits; memset on NULL would take the device down
    }
    memset(app, 0, sizeof(WcApp));

    app->store = wc_store_furi_port();
    app->clock = wc_clock_furi_port();
    WcSettings settings;
    wc_settings_service_load(&app->store, &settings); // corrupt file -> defaults, still usable
    app->baud = settings.baud;
    app->autosave = settings.autosave;
    wc_scan_init(&app->scan, app->clock);
    wc_known_service_load(&app->store, &app->known);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, wc_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wc_back_event_callback);

    app->list_view = wc_scroll_list_alloc();
    app->text_input = text_input_alloc();
    app->text_box = text_box_alloc();
    app->widget = widget_alloc();
    app->var_item_list = variable_item_list_alloc();

    view_dispatcher_add_view(app->view_dispatcher, WcViewList,
                             wc_scroll_list_get_view(app->list_view));
    view_dispatcher_add_view(app->view_dispatcher, WcViewTextInput,
                             text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, WcViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(app->view_dispatcher, WcViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, WcViewVarList,
                             variable_item_list_get_view(app->var_item_list));

    app->scene_manager = scene_manager_alloc(&wc_scene_handlers, app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void wc_app_free(WcApp *app) {
    view_dispatcher_remove_view(app->view_dispatcher, WcViewList);
    view_dispatcher_remove_view(app->view_dispatcher, WcViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WcViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, WcViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, WcViewVarList);

    wc_scroll_list_free(app->list_view);
    text_input_free(app->text_input);
    text_box_free(app->text_box);
    widget_free(app->widget);
    variable_item_list_free(app->var_item_list);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    if (app->serial) {
        wc_serial_furi_free(app->serial);
    }
    wc_census_free(&app->scan.census);
    if (app->browse_census) {
        wc_census_free(app->browse_census);
        free(app->browse_census);
    }
    free(app);
}

int32_t wc_app_run(void) {
    WcApp *app = wc_app_alloc();
    if (!app) {
        return -1;
    }
    scene_manager_next_scene(app->scene_manager, WcSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    wc_app_free(app);
    return 0;
}
