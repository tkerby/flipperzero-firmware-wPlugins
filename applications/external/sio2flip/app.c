/* 
 * This file is part of the 8-bit ATARI FDD Emulator for Flipper Zero 
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi.h>
#include <notification/notification_messages.h>

#include "app.h"
#include "scenes/scenes.h"

static bool app_custom_event_callback(void* context, uint32_t event) {
    furi_check(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool app_back_event_callback(void* context) {
    furi_check(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void fdd_activity_callback(void* context, SIODevice device, FddActivity activity, uint16_t sector) {
    furi_check(context != NULL);
    App* app = (App*)context;

    switch(activity) {
    case FddEmuActivity_Read:
        notification_message(app->notifications, &sequence_blink_green_10);
        break;
    case FddEmuActivity_Write:
        notification_message(app->notifications, &sequence_blink_red_10);
        break;
    default:
        notification_message(app->notifications, &sequence_blink_blue_10);
        break;
    }

    if(app->selected_fdd == device - SIO_DEVICE_DISK1) {
        fdd_screen_update_activity(app->fdd_screen, activity, sector);
    }
}

static App* app_alloc() {
    FURI_LOG_T(TAG, "Starting application...");

    // Allocate the application state
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    // Open the GUI
    app->gui = furi_record_open(RECORD_GUI);
    // Open storage
    app->storage = furi_record_open(RECORD_STORAGE);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Create the ATR data directory
    storage_common_mkdir(app->storage, ATR_DATA_PATH_PREFIX);

    // Initialize the application configuration
    app_config_init(&app->config);
    app_config_load(&app->config, app->storage);

    // Initialize scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_event_callback);

    // Attach view dispatcher to the GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate view
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewVariableList,
        variable_item_list_get_view(app->var_item_list));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewNumberInput, number_input_get_view(app->number_input));

    app->fdd_screen = fdd_screen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFddScreen, fdd_screen_get_view(app->fdd_screen));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewWiring, popup_get_view(app->popup));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewDialog, dialog_ex_get_view(app->dialog));

    app->file_path = furi_string_alloc();

    app->file_browser = file_browser_alloc(app->file_path);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewFileBrowser, file_browser_get_view(app->file_browser));

    app->sio = sio_driver_alloc();
    furi_check(app->sio != NULL);

    app->selected_fdd = 0;

    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        app->fdd[i] = fdd_alloc(SIO_DEVICE_DISK1 + i, app->sio, &app->config);
        furi_check(app->fdd[i] != NULL);

        fdd_set_activity_callback(app->fdd[i], fdd_activity_callback, app);

        const char* path = furi_string_get_cstr(app->config.fdd[i].image);

        if(path[0] != '\0') {
            DiskImage* image = disk_image_open(path, app->storage);
            if(image != NULL && !fdd_insert_disk(app->fdd[i], image)) {
                disk_image_close(image);
            }
        }
    }

    FURI_LOG_T(TAG, "Application started");

    return app;
}

static void app_free(App* app) {
    FURI_LOG_T(TAG, "Stopping application...");

    if(app->sio) {
        for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
            fdd_free(app->fdd[i]);
        }
    }

    sio_driver_free(app->sio);

    app_config_save(&app->config, app->storage);

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, AppViewFileBrowser);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_path);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewVariableList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewWiring);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewDialog);
    dialog_ex_free(app->dialog);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewFddScreen);
    fdd_screen_free(app->fdd_screen);

    // Free view dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    app_config_free(&app->config);

    free(app);

    FURI_LOG_T(TAG, "Application stopped");
}

static void app_run(App* app) {
    // Switch to the fdd emulator screen
    scene_manager_next_scene(app->scene_manager, SceneMainMenu);

    FURI_LOG_D(TAG, "Running view dispatcher...");
    view_dispatcher_run(app->view_dispatcher);
}

// Application entrypoint called by the Furi runtime
int32_t app_startup(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    app_run(app);
    app_free(app);

    return 0;
}
