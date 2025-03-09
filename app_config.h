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

#pragma once

#include <storage/storage.h>

// Supported FDD drives
typedef enum {
    FddType_ATARI_1050,
    FddType_ATARI_XF551,
    FddType_count,
} FddType;

// Returns the name of the FDD drive
const char* fdd_type_name(FddType fdd_type);

// Application configuration
typedef struct {
    // Indicate an activity  by blinking the LED
    bool led_blinking;

    struct {
        // Type of FDD drive
        FddType type;
        // Disk image file name
        // (empty string means no disk inserted)
        FuriString* image;
    } fdd[FDD_EMULATOR_COUNT];

} AppConfig;

// Initializes the application configuration and sets the default values
void app_config_init(AppConfig* config);

// Frees memory allocated for the application configuration
void app_config_free(AppConfig* config);

// Saves the application configuration to the storage
void app_config_save(const AppConfig* config, Storage* storage);

// Loads the application configuration from the storage
void app_config_load(AppConfig* config, Storage* storage);
