#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tja_parser.h"
#include "taiko_gpio_config.h"

typedef enum {
    TaikoMenuReturn,
    TaikoMenuRestart,
    TaikoMenuNewSong,
    TaikoMenuDifficulty,
    TaikoMenuPlaybackSpeed,
    TaikoMenuScrollSpeed,
    TaikoMenuBackward,
    TaikoMenuForward,
    TaikoMenuDonVolume,
    TaikoMenuKaVolume,
    TaikoMenuGpioOutput,
    TaikoMenuGpioSetting,
    TaikoMenuRestoreDefaults,
    TaikoMenuExit,
    TaikoMenuItemCount,
} TaikoMenuItem;

typedef enum {
    TaikoGpioMenuReturn,
    TaikoGpioMenuLeftKa,
    TaikoGpioMenuLeftDon,
    TaikoGpioMenuRightDon,
    TaikoGpioMenuRightKa,
    TaikoGpioMenuItemCount,
} TaikoGpioMenuItem;

typedef enum {
    TaikoMenuActionNone,
    TaikoMenuActionReturn,
    TaikoMenuActionRestart,
    TaikoMenuActionNewSong,
    TaikoMenuActionRestoreDefaults,
    TaikoMenuActionExit,
} TaikoMenuAction;

typedef struct {
    bool open;
    TaikoMenuItem selected;
    bool gpio_settings;
    TaikoGpioMenuItem gpio_selected;
    TaikoGpioConfig gpio;
    uint8_t backward_index;
    uint8_t forward_index;
    uint8_t don_volume;
    uint8_t ka_volume;
    uint8_t courses;
    TjaCourse course;
    uint8_t playback_index;
    uint8_t scroll_index;
} TaikoMenu;

void taiko_menu_init(TaikoMenu* menu);
void taiko_menu_open(TaikoMenu* menu);
void taiko_menu_move(TaikoMenu* menu, bool down);
void taiko_menu_adjust(TaikoMenu* menu, bool forward);
TaikoMenuAction taiko_menu_activate(TaikoMenu* menu);
TaikoMenuAction taiko_menu_back(TaikoMenu* menu);
uint32_t taiko_menu_backward_ms(const TaikoMenu* menu);
uint32_t taiko_menu_forward_ms(const TaikoMenu* menu);
uint8_t taiko_menu_playback_tenths(const TaikoMenu* menu);
uint8_t taiko_menu_scroll_tenths(const TaikoMenu* menu);
/* Select by the persisted value rather than the menu table index. */
bool taiko_menu_set_value(TaikoMenu* menu, TaikoMenuItem item, uint32_t value);
