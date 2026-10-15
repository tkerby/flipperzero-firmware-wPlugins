#pragma once

#include "taiko_mixer.h"

#include <storage/storage.h>

typedef struct {
    TaikoPcmClip don;
    TaikoPcmClip ka;
} TaikoSfx;

/* Missing, invalid or oversized effects are left empty; music still plays. */
TaikoSfx taiko_sfx_load(Storage* storage);
void taiko_sfx_free(TaikoSfx* sfx);
