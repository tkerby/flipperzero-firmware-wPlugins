#pragma once

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

#include <notification/notification_messages.h>
#include <infrared/worker/infrared_worker.h>
#include <power/power_service/power.h>
#include <furi_hal_infrared.h>

#include "scenes/lidaremulator_scene.h"

#include "view_hijacker.h"

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool is_otg_enabled;
} LidarEmulatorAppState;

struct LidarEmulatorApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Gui* gui;

    Submenu* submenu;
    VariableItemList* var_item_list;

    ViewInputCallback tmp_input_callback;

    ViewHijacker* view_hijacker;

    LidarEmulatorAppState app_state;
};

typedef struct LidarEmulatorApp LidarEmulatorApp;

//Enumeration of all used view types.
typedef enum {
    LidarEmulatorViewSubmenu,
    LidarEmulatorViewVariableList,
} LidarEmulatorView;

// GPIO Settings functions
void lidaremulator_set_tx_pin(LidarEmulatorApp* app, FuriHalInfraredTxPin tx_pin);
void lidaremulator_enable_otg(LidarEmulatorApp* app, bool enable);
void lidaremulator_load_settings(LidarEmulatorApp* app);
void lidaremulator_save_settings(LidarEmulatorApp* app);
