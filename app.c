/* 
 * This file is part of the TINA application for Flipper Zero (https://github.com/cepetr/tina).
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

#include "app.h"
#include "scenes/scenes.h"

#include "sensor/ina209_driver.h"
#include "sensor/ina219_driver.h"
#include "sensor/ina226_driver.h"

static bool app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool app_back_event_callback(void* context) {
    furi_assert(context != NULL);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void app_tick_event_callback(void* context) {
    furi_assert(context != NULL);
    App* app = (App*)context;

    if(app->sensor != NULL) {
        app->sensor->tick(app->sensor);
    }

    scene_manager_handle_tick_event(app->scene_manager);
}

void app_restart_sensor_driver(App* app) {
    if(app->sensor != NULL) {
        app->sensor->free(app->sensor);
        app->sensor = NULL;
    }

    switch(app->config.sensor_type) {
    /*
    case SensorType_INA209:
        Ina226Config ina209_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
        };
        sensor->driver = ina209_driver_alloc(&ina209_config);
        break;
    */
    case SensorType_INA219:
        Ina219Config ina219_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
        };
        app->sensor = ina219_driver_alloc(&ina219_config);
        break;

    case SensorType_INA226:
        Ina226Config ina226_config = {
            .i2c_address = app->config.i2c_address,
            .shunt_resistor = app->config.shunt_resistor,
        };
        app->sensor = ina226_driver_alloc(&ina226_config);
        break;

    default:
        break;
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

    // Initialize the application configuration
    app_config_init(&app->config);
    app_config_load(&app->config, app->storage);

    // Initialize scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, app_tick_event_callback, 100);

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

    app->tina_gauge = tina_gauge_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewTinaGauge, tina_gauge_get_view(app->tina_gauge));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, AppViewWiring, popup_get_view(app->popup));

    app_restart_sensor_driver(app);

    FURI_LOG_T(TAG, "Application started");

    return app;
}

static void app_free(App* app) {
    FURI_LOG_T(TAG, "Stopping application...");

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, AppViewVariableList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewWiring);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewTinaGauge);
    tina_gauge_free(app->tina_gauge);

    // Free sensor driver
    if(app->sensor != NULL) {
        app->sensor->free(app->sensor);
    }

    // Free view dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Save the application configuration
    app_config_save(&app->config, app->storage);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);

    FURI_LOG_T(TAG, "Application stopped");
}

static void app_run(App* app) {
    // Switch to the gauge screen
    scene_manager_next_scene(app->scene_manager, SceneGauge);

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
