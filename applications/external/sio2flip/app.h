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

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/number_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <power/power_service/power.h>

#include "app_common.h"
#include "app_config.h"
#include "sio_driver.h"
#include "fdd_emulator.h"

#include "views/fdd_screen.h"

#define ATR_DATA_PATH_PREFIX STORAGE_APP_DATA_PATH_PREFIX "/atr"

typedef enum {
    AppViewNumberInput,
    AppViewVariableList,
    AppViewFddScreen,
    AppViewWiring,
    AppViewDialog,
    AppViewFileBrowser,
} AppView;

typedef struct {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Power* power;

    VariableItemList* var_item_list;
    NumberInput* number_input;
    FddScreen* fdd_screen;
    Popup* popup;
    DialogEx* dialog;
    FileBrowser* file_browser;
    FuriString* file_path;

    // Application configuration
    AppConfig config;
    // SIO driver
    SIODriver* sio;
    // FDD emulators
    FddEmulator* fdd[FDD_EMULATOR_COUNT];
    // Selected FDD emulator
    uint8_t selected_fdd;
} App;

// Builds a unique file name for a new ATR image.
// File name is stored in app->file_path and is valid until the next call to
// this function or FddSelect dialog is used.
const char* app_build_unique_file_name(App* app);
