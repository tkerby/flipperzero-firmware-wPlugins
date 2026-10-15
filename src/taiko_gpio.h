#pragma once

#include <stdbool.h>

#define TAIKO_HIT_MS 70U

typedef struct TaikoGpio TaikoGpio;

/* Owned by the player thread. Idle outputs are low.
 * Left rim / left face / right face / right rim: PA7 / PA6 / PA4 / PB3. */
TaikoGpio* taiko_gpio_alloc(void);
void taiko_gpio_pulse(TaikoGpio* gpio, bool right, bool rim);
void taiko_gpio_reset(TaikoGpio* gpio);
void taiko_gpio_free(TaikoGpio* gpio);
