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

#pragma once

#include <gui/view.h>

#include "sensor/sensor_driver.h"

// Gauge screen showing the sensor state
typedef struct TinaGauge TinaGauge;

// Callback invoked when the menu button is pressed
typedef void (*TinaGaugeCallback)(void* context);

// Allocates gauge screen
TinaGauge* tina_gauge_alloc(void);

// Frees gauge screen
void tina_gauge_free(TinaGauge* gauge);

// Returns the view of the gauge screen
View* tina_gauge_get_view(TinaGauge* gauge);

// Updates the gauge screen with the sensor state
void tina_gauge_update(TinaGauge* gauge, const SensorState* state);

// Sets the callback invoked when the menu button is pressed
void tina_gauge_set_menu_callback(TinaGauge* gauge, TinaGaugeCallback callback, void* context);
