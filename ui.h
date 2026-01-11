#pragma once

#include "moisture_sensor.h"

const char* get_moisture_status(uint8_t percent);
void draw_callback(Canvas* canvas, void* ctx);
