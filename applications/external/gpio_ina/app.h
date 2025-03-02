/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
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

#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/number_input.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#include "app_common.h"
#include "app_config.h"
#include "views/current_gauge.h"

typedef enum {
    AppViewNumberInput,
    AppViewVariableList,
    AppViewCurrentGauge,
    AppViewWiring,
} AppView;

typedef struct {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    VariableItemList* var_item_list;
    NumberInput* number_input;
    CurrentGauge* current_gauge;
    Popup* popup;

    AppConfig config;
    SensorDriver* sensor;

    // Time of the last measurement
    // (== 0 if no measurement was done yet)
    uint32_t last_measurement_time;
} App;

// Restarts the sensor driver to apply the new configuration
void app_restart_sensor_driver(App* app);
