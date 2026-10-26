// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TAIKO_GPIO_PULSE_MIN_MS     25U
#define TAIKO_GPIO_PULSE_MAX_MS     100U
#define TAIKO_GPIO_PULSE_STEP_MS    5U
#define TAIKO_GPIO_PULSE_DEFAULT_MS 70U

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
    uint32_t pulse_ms;
} TaikoGpioConfig;

TaikoGpioConfig taiko_gpio_default_config(void);
const char* taiko_gpio_pin_name(TaikoGpioPin pin);
bool taiko_gpio_pulse_valid(uint32_t pulse_ms);
bool taiko_gpio_config_valid(const TaikoGpioConfig* config);
/* Cycle through pins not assigned to another output. */
void taiko_gpio_config_adjust(TaikoGpioConfig* config, TaikoGpioOutput output, bool forward);
