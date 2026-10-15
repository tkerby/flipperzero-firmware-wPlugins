#pragma once

#include "tja_parser.h"

typedef struct {
    const uint8_t* samples;
    uint32_t count;
    uint32_t rate;
} TaikoPcmClip;

/* All sample and chart data is borrowed and must outlive the audio worker. */
typedef struct {
    TaikoPcmClip don;
    TaikoPcmClip ka;
    const TjaChart* chart;
    uint8_t don_volume; /* 0..100 percent, applied independently before summing. */
    uint8_t ka_volume;
} TaikoMixer;

/* Mix scheduled drum sounds into unsigned 8-bit mono backing audio. Position
 * is an absolute sample offset in the backing WAV, including after a seek. */
void taiko_mixer_mix(
    const TaikoMixer* mixer,
    uint8_t* samples,
    size_t count,
    uint32_t rate,
    uint32_t position);

/* Final application gain for all PCM output, including music without effects.
 * Unsigned PCM silence stays centered at 128. */
void taiko_mixer_apply_volume(uint8_t* samples, size_t count, uint8_t volume);
