// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include "taiko_gpio_config.h"

typedef struct TaikoGpio TaikoGpio;

/* Owned by the player thread. Disabled by default, without configuring any pins.
 * Enabled outputs idle low. Configuration changes drain all pending pulses. */
TaikoGpio* taiko_gpio_alloc(void);
bool taiko_gpio_configure(TaikoGpio* gpio, const TaikoGpioConfig* config);
void taiko_gpio_pulse(TaikoGpio* gpio, bool right, bool rim);
void taiko_gpio_reset(TaikoGpio* gpio);
void taiko_gpio_free(TaikoGpio* gpio);
