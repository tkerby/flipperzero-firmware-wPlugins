// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <stdbool.h>

typedef enum {
    TaikoGpioLeftKa,
    TaikoGpioLeftDon,
    TaikoGpioRightDon,
    TaikoGpioRightKa,
    TaikoGpioOutputCount,
} TaikoGpioOutput;

typedef enum {
    TaikoGpioPinA7,
    TaikoGpioPinA6,
    TaikoGpioPinA4,
    TaikoGpioPinB3,
    TaikoGpioPinB2,
    TaikoGpioPinC3,
    TaikoGpioPinC1,
    TaikoGpioPinC0,
    TaikoGpioPinCount,
} TaikoGpioPin;

typedef struct {
    bool enabled;
    TaikoGpioPin pins[TaikoGpioOutputCount];
} TaikoGpioConfig;

TaikoGpioConfig taiko_gpio_default_config(void);
const char* taiko_gpio_pin_name(TaikoGpioPin pin);
bool taiko_gpio_config_valid(const TaikoGpioConfig* config);
/* Cycle through pins not assigned to another output. */
void taiko_gpio_config_adjust(TaikoGpioConfig* config, TaikoGpioOutput output, bool forward);
