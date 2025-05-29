#pragma once

#include <flipper_format/flipper_format.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <hvac_hitachi.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "ac_remote_app.h"
#include "ac_remote_custom_event.h"
#include "hitachi_ac_remote_icons.h"
#include "scenes/ac_remote_scene.h"
#include "views/ac_remote_panel.h"

#define AC_REMOTE_APP_SETTINGS APP_DATA_PATH("settings.txt")

typedef enum {
    PowerButtonOff,
    PowerButtonOn,
    POWER_STATE_MAX,
} PowerButtonState;

typedef enum {
    ModeButtonHeating,
    ModeButtonCooling,
    ModeButtonDehumidifying,
    ModeButtonFan,
    MODE_BUTTON_STATE_MAX,
} ModeButtonState;

typedef enum {
    FanSpeedButtonLow,
    FanSpeedButtonMedium,
    FanSpeedButtonHigh,
    FAN_SPEED_BUTTON_STATE_MAX,
} FanSpeedButtonState;

typedef enum {
    VaneButtonPos0,
    VaneButtonPos1,
    VaneButtonPos2,
    VaneButtonPos3,
    VaneButtonPos4,
    VaneButtonPos5,
    VaneButtonPos6,
    VaneButtonAuto,
    VANE_BUTTON_STATE_MAX,
} VaneButtonState;

typedef enum {
    LabelStringTemp,
    LabelStringTimerOnHour,
    LabelStringTimerOnMinute,
    LabelStringTimerOffHour,
    LabelStringTimerOffMinute,
    LABEL_STRING_POOL_SIZE,
} LabelStringPoolIndex;

typedef struct {
    uint32_t power;
    uint32_t mode;
    uint32_t temperature;
    uint32_t fan;
    uint32_t vane;
    uint32_t timer_on_h;
    uint32_t timer_on_m;
    uint32_t timer_off_h;
    uint32_t timer_off_m;
} ACRemoteAppSettings;

struct AC_RemoteApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    ACRemotePanel* panel_main;
    ACRemotePanel* panel_sub;
    ACRemoteAppSettings app_state;
    char label_string_pool[LABEL_STRING_POOL_SIZE][4];
    HvacHitachiContext* protocol;
};

typedef enum {
    AC_RemoteAppViewMain,
    AC_RemoteAppViewSub,
} AC_RemoteAppView;

#define LABEL_STRING_SIZE sizeof(ac_remote->label_string_pool[0])
