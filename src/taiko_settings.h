#pragma once

#include "taiko_menu.h"

#define TAIKO_DEFAULT_VOLUME 70U

typedef struct {
    uint32_t volume;
    uint32_t don_volume;
    uint32_t ka_volume;
    uint32_t backward_ms;
    uint32_t forward_ms;
    uint32_t playback_tenths;
    uint32_t scroll_tenths;
    TaikoGpioPin pins[TaikoGpioOutputCount];
} TaikoSettings;

void taiko_settings_defaults(TaikoSettings* settings);
void taiko_settings_capture(TaikoSettings* settings, const TaikoMenu* menu, uint8_t volume);
/* Apply global preferences while preserving chart and menu navigation state.
 * GPIO output is always disabled when applying saved/default settings. */
void taiko_settings_apply(const TaikoSettings* settings, TaikoMenu* menu, uint8_t* volume);
/* Replace invalid fields with defaults; a conflicting pin map resets as a group. */
bool taiko_settings_validate(TaikoSettings* settings);
bool taiko_settings_equal(const TaikoSettings* a, const TaikoSettings* b);
bool taiko_settings_parse_uint(const char* text, uint32_t* value);
bool taiko_settings_parse_pin(const char* text, TaikoGpioPin* pin);
