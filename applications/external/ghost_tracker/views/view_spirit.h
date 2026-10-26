#pragma once

#include <gui/view.h>

/**
 * Spirit Box module (TCI-style white-noise sweep).
 *
 * Uses the real onboard speaker (furi_hal_speaker) to generate a genuinely
 * audible, hardware-produced tone that hops to a new frequency - picked with
 * the STM32's hardware TRNG (furi_hal_random_get), not a software PRNG -
 * every "step" interval. The chaotic hopping itself IS the real behavior of
 * a sweep-style spirit box; this is not a simulated/fake reading.
 */

typedef struct SpiritView SpiritView;

SpiritView* spirit_view_alloc(void);
void spirit_view_free(SpiritView* instance);
View* spirit_view_get_view(SpiritView* instance);
