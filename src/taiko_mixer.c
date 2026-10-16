// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_mixer.h"
#include "taiko_hits.h"

#include <string.h>

#define MIX_BLOCK              128U
#define AUTO_RETRIGGER_FADE_MS 3U

void taiko_mixer_apply_volume(uint8_t* samples, size_t count, uint8_t volume) {
    if(volume > TAIKO_MASTER_VOLUME_MAX) volume = TAIKO_MASTER_VOLUME_MAX;
    if(volume == 100) return;
    if(!volume) {
        memset(samples, 128, count);
        return;
    }
    for(size_t i = 0; i < count; i++) {
        int32_t value = 128 + ((int32_t)samples[i] - 128) * volume / 100;
        if(value < 0) value = 0;
        if(value > 255) value = 255;
        samples[i] = (uint8_t)value;
    }
}

static uint32_t clip_duration(const TaikoPcmClip* clip) {
    if(!clip->samples || !clip->count || !clip->rate) return 0;
    return ((uint64_t)clip->count * 1000 + clip->rate - 1) / clip->rate;
}

void taiko_mixer_mix(
    const TaikoMixer* mixer,
    uint8_t* samples,
    size_t count,
    uint32_t rate,
    uint32_t position) {
    if(!rate || !count || !mixer->chart) return;
    const TaikoPcmClip* clips[] = {&mixer->don, &mixer->ka};
    const uint8_t volumes[] = {
        mixer->don_volume > 100 ? 100 : mixer->don_volume,
        mixer->ka_volume > 100 ? 100 : mixer->ka_volume};
    uint32_t durations[2];
    uint64_t lengths[2];
    for(size_t i = 0; i < 2; i++) {
        durations[i] = volumes[i] ? clip_duration(clips[i]) : 0;
        lengths[i] = durations[i] ?
                         ((uint64_t)clips[i]->count * rate + clips[i]->rate - 1) / clips[i]->rate :
                         0;
    }
    uint32_t duration = durations[0] > durations[1] ? durations[0] : durations[1];
    if(!duration) return; /* Missing effects preserve the original backing audio. */

    int64_t earliest = (int64_t)((uint64_t)position * 1000 / rate) - duration - 1;
    TaikoHitCursor cursor;
    taiko_hits_seek(mixer->chart, &cursor, earliest > 0 ? earliest : 0);
    const uint32_t retrigger_fade = ((uint64_t)rate * AUTO_RETRIGGER_FADE_MS + 999) / 1000;

    for(size_t base = 0; base < count; base += MIX_BLOCK) {
        size_t length = count - base;
        if(length > MIX_BLOCK) length = MIX_BLOCK;
        int32_t mixed[MIX_BLOCK];
        for(size_t j = 0; j < length; j++)
            mixed[j] = (int32_t)samples[base + j] - 128;
        uint64_t start = (uint64_t)position + base;
        uint64_t end = start + length;
        TaikoHitCursor block = cursor;
        bool expired_prefix = true;
        TaikoHit hit;
        while(taiko_hits_peek(mixer->chart, &block, &hit)) {
            uint64_t onset = (uint64_t)hit.time_ms * rate / 1000;
            if(onset >= end) break;
            taiko_hits_advance(mixer->chart, &block);
            const unsigned channel = hit.rim ? 1 : 0;
            const TaikoPcmClip* clip = clips[channel];
            const uint32_t volume = volumes[channel];
            if(!durations[channel]) {
                if(expired_prefix) cursor = block;
                continue;
            }
            uint64_t clip_end = onset + lengths[channel];
            uint32_t fade_samples = 0;
            if(hit.roll && block.run < mixer->chart->hit_run_count) {
                /* The advanced cursor identifies the next automatic hit even across
                 * run boundaries and intervening normal notes. Look ahead beyond
                 * this buffer so fades are identical after a split or seek. */
                uint64_t next = (uint64_t)taiko_hit_run_time(
                                    &mixer->chart->hit_runs[block.run], block.repeat) *
                                rate / 1000;
                if(next < clip_end) {
                    clip_end = next;
                    fade_samples = retrigger_fade;
                    if(fade_samples > clip_end - onset) fade_samples = clip_end - onset;
                }
            }
            if(clip_end <= start) {
                if(expired_prefix) cursor = block;
                continue;
            }
            /* Retain every potentially overlapping voice, but never rescan a
             * prefix that has finished (or is muted) in later mix blocks. */
            expired_prefix = false;
            uint64_t first = onset > start ? onset : start;
            uint64_t last = clip_end < end ? clip_end : end;
            uint64_t phase = (first - onset) * clip->rate;
            uint32_t index = phase / rate;
            uint32_t fraction = phase % rate;
            const bool same_rate = clip->rate == rate;
            for(uint64_t sample = first; sample < last && index < clip->count; sample++) {
                int32_t value = ((int32_t)clip->samples[index] - 128) * (int32_t)volume / 100;
                if(fade_samples && clip_end - sample <= fade_samples)
                    value = value * (int32_t)(clip_end - sample - 1) / (int32_t)fade_samples;
                mixed[sample - start] += value;
                if(same_rate) {
                    index++;
                } else {
                    fraction += clip->rate;
                    index += fraction / rate;
                    fraction %= rate;
                }
            }
        }
        for(size_t j = 0; j < length; j++) {
            int32_t value = mixed[j] + 128;
            if(value < 0) value = 0;
            if(value > 255) value = 255;
            samples[base + j] = (uint8_t)value;
        }
    }
}
