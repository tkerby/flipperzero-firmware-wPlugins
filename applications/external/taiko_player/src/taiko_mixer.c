// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_mixer.h"
#include "taiko_hits.h"

#include <string.h>

#define MIX_BLOCK         128U
#define RETRIGGER_FADE_MS 3U

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

/* The cursor is already past the current hit. Each sound has its own voice;
 * automatic hits share Don, and Ka never interrupts it. Inspect the next run
 * directly so looking for Ka need not iterate over automatic Don hits. */
static uint64_t channel_end(
    const TjaChart* chart,
    const TaikoHitCursor* cursor,
    bool rim,
    uint32_t rate,
    uint64_t end) {
    if(!rim && cursor->run < chart->hit_run_count) {
        uint64_t next =
            (uint64_t)taiko_hit_run_time(&chart->hit_runs[cursor->run], cursor->repeat) * rate /
            1000;
        if(next < end) end = next;
    }
    for(size_t i = cursor->note; i < chart->note_count; i++) {
        const TjaNote* note = &chart->notes[i];
        uint64_t next = (uint64_t)note->time_ms * rate / 1000;
        if(next >= end) break;
        if((rim && (note->kind == 2 || note->kind == 4)) ||
           (!rim && (note->kind == 1 || note->kind == 3))) {
            end = next;
            break;
        }
    }
    return end;
}

void taiko_mixer_mix(
    const TaikoMixer* mixer,
    uint8_t* samples,
    size_t count,
    uint32_t chart_rate,
    uint32_t output_rate,
    uint32_t position) {
    if(!chart_rate || !output_rate || !count || !mixer->chart) return;
    const TaikoPcmClip* clips[] = {&mixer->don, &mixer->ka};
    const uint8_t volumes[] = {
        mixer->don_volume > 100 ? 100 : mixer->don_volume,
        mixer->ka_volume > 100 ? 100 : mixer->ka_volume};
    uint64_t lengths[2];
    for(size_t i = 0; i < 2; i++) {
        const TaikoPcmClip* clip = clips[i];
        lengths[i] = volumes[i] && clip->samples && clip->count && clip->rate ?
                         ((uint64_t)clip->count * output_rate + clip->rate - 1) / clip->rate :
                         0;
    }
    uint64_t longest = lengths[0] > lengths[1] ? lengths[0] : lengths[1];
    uint64_t duration = (longest * 1000 + chart_rate - 1) / chart_rate;
    if(!duration) return; /* Missing effects preserve the original backing audio. */

    int64_t earliest = (int64_t)((uint64_t)position * 1000 / chart_rate) - (int64_t)duration - 1;
    TaikoHitCursor cursor;
    taiko_hits_seek(mixer->chart, &cursor, earliest > 0 ? earliest : 0);
    const uint32_t retrigger_fade = ((uint64_t)output_rate * RETRIGGER_FADE_MS + 999) / 1000;

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
            uint64_t onset = (uint64_t)hit.time_ms * chart_rate / 1000;
            if(onset >= end) break;
            taiko_hits_advance(mixer->chart, &block);
            const unsigned channel = hit.rim ? 1 : 0;
            const TaikoPcmClip* clip = clips[channel];
            const uint32_t volume = volumes[channel];
            if(!lengths[channel]) {
                if(expired_prefix) cursor = block;
                continue;
            }
            uint64_t natural_end = onset + lengths[channel];
            uint64_t clip_end =
                channel_end(mixer->chart, &block, hit.rim, chart_rate, natural_end);
            uint32_t fade_samples = 0;
            if(clip_end < natural_end) {
                /* Look beyond this buffer so fades match after a split or seek. */
                fade_samples = retrigger_fade;
                if(fade_samples > clip_end - onset) fade_samples = clip_end - onset;
            }
            if(clip_end <= start) {
                if(expired_prefix) cursor = block;
                continue;
            }
            /* Retain both potentially active voices, but never rescan a
             * prefix that has finished (or is muted) in later mix blocks. */
            expired_prefix = false;
            uint64_t first = onset > start ? onset : start;
            uint64_t last = clip_end < end ? clip_end : end;
            uint64_t phase = (first - onset) * clip->rate;
            uint32_t index = phase / output_rate;
            uint32_t fraction = phase % output_rate;
            const bool same_rate = clip->rate == output_rate;
            for(uint64_t sample = first; sample < last && index < clip->count; sample++) {
                int32_t value = ((int32_t)clip->samples[index] - 128) * (int32_t)volume / 100;
                if(fade_samples && clip_end - sample <= fade_samples)
                    value = value * (int32_t)(clip_end - sample - 1) / (int32_t)fade_samples;
                mixed[sample - start] += value;
                if(same_rate) {
                    index++;
                } else {
                    fraction += clip->rate;
                    index += fraction / output_rate;
                    fraction %= output_rate;
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
