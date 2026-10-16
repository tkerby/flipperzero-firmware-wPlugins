// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_gpio_config.h"

TaikoGpioConfig taiko_gpio_default_config(void) {
    return (TaikoGpioConfig){
        .enabled = false,
        .pins = {TaikoGpioPinA7, TaikoGpioPinA6, TaikoGpioPinA4, TaikoGpioPinB3}};
}

const char* taiko_gpio_pin_name(TaikoGpioPin pin) {
    static const char* names[] = {"A7", "A6", "A4", "B3", "B2", "C3", "C1", "C0"};
    return (unsigned)pin < TaikoGpioPinCount ? names[pin] : "?";
}

bool taiko_gpio_config_valid(const TaikoGpioConfig* config) {
    for(unsigned i = 0; i < TaikoGpioOutputCount; i++) {
        if((unsigned)config->pins[i] >= TaikoGpioPinCount) return false;
        for(unsigned j = 0; j < i; j++)
            if(config->pins[i] == config->pins[j]) return false;
    }
    return true;
}

void taiko_gpio_config_adjust(TaikoGpioConfig* config, TaikoGpioOutput output, bool forward) {
    if((unsigned)output >= TaikoGpioOutputCount || !taiko_gpio_config_valid(config)) return;
    unsigned pin = config->pins[output];
    for(unsigned step = 1; step <= TaikoGpioPinCount; step++) {
        pin = (pin + (forward ? 1 : TaikoGpioPinCount - 1)) % TaikoGpioPinCount;
        bool used = false;
        for(unsigned i = 0; i < TaikoGpioOutputCount; i++)
            if(i != (unsigned)output && config->pins[i] == (TaikoGpioPin)pin) used = true;
        if(!used) {
            config->pins[output] = pin;
            return;
        }
    }
}
