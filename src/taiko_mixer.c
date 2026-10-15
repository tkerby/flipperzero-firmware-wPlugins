#include "taiko_mixer.h"
#include "taiko_hits.h"

#include <string.h>

#define MIX_BLOCK 128U

void taiko_mixer_apply_volume(uint8_t* samples, size_t count, uint8_t volume) {
    if(volume >= 100) return;
    if(!volume) {
        memset(samples, 128, count);
        return;
    }
    for(size_t i = 0; i < count; i++)
        samples[i] = 128 + ((int32_t)samples[i] - 128) * volume / 100;
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
    uint32_t duration = clip_duration(&mixer->don);
    uint32_t ka_duration = clip_duration(&mixer->ka);
    if(ka_duration > duration) duration = ka_duration;
    if(!duration) return; /* Missing effects preserve the original backing audio. */

    int64_t earliest = (int64_t)((uint64_t)position * 1000 / rate) - duration - 1;
    TaikoHitCursor cursor;
    taiko_hits_seek(mixer->chart, &cursor, earliest > 0 ? earliest : 0);

    for(size_t base = 0; base < count; base += MIX_BLOCK) {
        size_t length = count - base;
        if(length > MIX_BLOCK) length = MIX_BLOCK;
        int32_t mixed[MIX_BLOCK];
        for(size_t j = 0; j < length; j++)
            mixed[j] = (int32_t)samples[base + j] - 128;
        uint64_t start = (uint64_t)position + base;
        uint64_t end = start + length;
        TaikoHitCursor block = cursor;
        TaikoHit hit;
        while(taiko_hits_peek(mixer->chart, &block, &hit)) {
            uint64_t onset = (uint64_t)hit.time_ms * rate / 1000;
            if(onset >= end) break;
            taiko_hits_advance(mixer->chart, &block);
            const TaikoPcmClip* clip = hit.rim ? &mixer->ka : &mixer->don;
            uint32_t volume = hit.rim ? mixer->ka_volume : mixer->don_volume;
            if(!volume) continue;
            if(volume > 100) volume = 100;
            if(!clip_duration(clip)) continue;
            uint64_t clip_end =
                onset + ((uint64_t)clip->count * rate + clip->rate - 1) / clip->rate;
            if(clip_end <= start) continue;
            uint64_t first = onset > start ? onset : start;
            uint64_t last = clip_end < end ? clip_end : end;
            uint64_t phase = (first - onset) * clip->rate;
            uint32_t index = phase / rate;
            uint32_t fraction = phase % rate;
            for(uint64_t sample = first; sample < last && index < clip->count; sample++) {
                mixed[sample - start] +=
                    ((int32_t)clip->samples[index] - 128) * (int32_t)volume / 100;
                fraction += clip->rate;
                index += fraction / rate;
                fraction %= rate;
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
