#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <toolbox/saved_struct.h>
#include <storage/storage.h>
#include "desktop_settings_filename.h"

#define DESKTOP_SETTINGS_VER (13)

#define DESKTOP_SETTINGS_OLD_PATH CFG_PATH(DESKTOP_SETTINGS_FILE_NAME)
#define DESKTOP_SETTINGS_PATH INT_PATH(DESKTOP_SETTINGS_FILE_NAME)
#define DESKTOP_SETTINGS_MAGIC (0x17)
#define PIN_MAX_LENGTH 12

#define DESKTOP_SETTINGS_RUN_PIN_SETUP_ARG "run_pin_setup"

#define MAX_PIN_SIZE 10
#define MIN_PIN_SIZE 4
#define MAX_APP_LENGTH 128

#define DISPLAY_BATTERY_BAR 0
#define DISPLAY_BATTERY_PERCENT 1
#define DISPLAY_BATTERY_INVERTED_PERCENT 2
#define DISPLAY_BATTERY_RETRO_3 3
#define DISPLAY_BATTERY_RETRO_5 4
#define DISPLAY_BATTERY_BAR_PERCENT 5
#define DISPLAY_BATTERY_NONE 6

#define ICON_STYLE_STOCK 0
#define ICON_STYLE_SLIM 1

typedef enum {
    FavoriteAppDownLong = 0,
    FavoriteAppLeftShort,
    FavoriteAppLeftLong,
    FavoriteAppRightShort,
    FavoriteAppRightLong,
    FavoriteAppUpLong,

    FavoriteAppNumber,
} FavoriteAppShortcut;

typedef enum {
    DummyAppLeft = 0,
    DummyAppLeftLong,
    DummyAppRight,
    DummyAppRightLong,
    DummyAppUpLong,
    DummyAppDown,
    DummyAppDownLong,
    DummyAppOk,
    DummyAppOkLong,

    DummyAppNumber,
} DummyAppShortcut;

typedef struct {
    InputKey data[MAX_PIN_SIZE];
    uint8_t length;
} PinCode;

typedef struct {
    char name_or_path[MAX_APP_LENGTH];
} FavoriteApp;

typedef struct {
    PinCode pin_code;
    uint32_t auto_lock_delay_ms;
    uint8_t displayBatteryPercentage;
    bool is_dumbmode;
    uint8_t icon_style;
    bool lock_icon;
    bool bt_icon;
    bool rpc_icon;
    bool sdcard;
    bool stealth_icon;
    bool top_bar;
    uint8_t dummy_mode;
    bool dumbmode_icon;
    bool auto_lock_with_pin;
    uint8_t display_clock;
    FavoriteApp favorite_apps[FavoriteAppNumber];
    FavoriteApp dummy_apps[DummyAppNumber];
} DesktopSettings;

#ifdef __cplusplus
extern "C" {
#endif

bool DESKTOP_SETTINGS_SAVE(DesktopSettings* x);

bool DESKTOP_SETTINGS_LOAD(DesktopSettings* x);

#ifdef __cplusplus
}
#endif