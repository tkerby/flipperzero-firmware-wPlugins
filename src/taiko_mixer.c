#include "taiko_mixer.h"

#define MIX_BLOCK 128U

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
    if(!rate || !count || !mixer->notes || !mixer->note_count) return;
    uint32_t duration = clip_duration(&mixer->don);
    uint32_t ka_duration = clip_duration(&mixer->ka);
    if(ka_duration > duration) duration = ka_duration;
    if(!duration) return; /* Missing effects preserve the original backing audio. */

    int64_t earliest = (int64_t)((uint64_t)position * 1000 / rate) - duration - 1;
    size_t low = 0, high = mixer->note_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(mixer->notes[mid].time_ms < earliest)
            low = mid + 1;
        else
            high = mid;
    }

    for(size_t base = 0; base < count; base += MIX_BLOCK) {
        size_t length = count - base;
        if(length > MIX_BLOCK) length = MIX_BLOCK;
        int32_t mixed[MIX_BLOCK];
        for(size_t j = 0; j < length; j++)
            mixed[j] = ((int32_t)samples[base + j] - 128) * 3 / 4;
        uint64_t start = (uint64_t)position + base;
        uint64_t end = start + length;
        for(size_t i = low; i < mixer->note_count; i++) {
            TjaNote note = mixer->notes[i];
            if(note.time_ms < 0) continue;
            uint64_t onset = (uint64_t)note.time_ms * rate / 1000;
            if(onset >= end) break;
            const TaikoPcmClip* clip;
            uint32_t volume;
            if(note.kind == 1 || note.kind == 3) {
                clip = &mixer->don;
                volume = mixer->don_volume;
            } else if(note.kind == 2 || note.kind == 4) {
                clip = &mixer->ka;
                volume = mixer->ka_volume;
            } else {
                continue;
            }
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
