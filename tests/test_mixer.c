// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_mixer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_notes_and_clipping(void) {
    const uint8_t don[] = {228, 28, 178};
    const uint8_t ka[] = {148, 108};
    const TjaNote notes[] = {
        {.time_ms = -1, .kind = 1},
        {.time_ms = 1, .kind = 1, .scroll_speed = 4},
        {.time_ms = 2, .kind = 2, .scroll_speed = -0.5f},
        {.time_ms = 3, .kind = 3},
        {.time_ms = 3, .kind = 4},
        {.time_ms = 4, .kind = 5}};
    TaikoMixer mixer = {
        {don, 3, 8000},
        {ka, 2, 8000},
        &(TjaChart){.notes = (TjaNote*)notes, .note_count = 6},
        100,
        100};
    uint8_t samples[48];
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 128);
    assert(samples[7] == 128 && samples[8] == 228 && samples[9] == 28 && samples[10] == 178);
    assert(samples[16] == 148 && samples[17] == 108);
    assert(samples[24] == 248 && samples[25] == 8 && samples[26] == 178);
    assert(samples[32] == 128); /* Roll starts do not synthesize extra hits. */

    const uint8_t peak[] = {168, 88};
    const TjaNote stacked[] = {
        {.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 1}};
    mixer = (TaikoMixer){
        {peak, 2, 8000}, {0}, &(TjaChart){.notes = (TjaNote*)stacked, .note_count = 3}, 100, 100};
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 168 && samples[1] == 88); /* Same-sound hits never stack. */
    memset(samples, 228, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 255); /* Music plus one voice still saturates, never wraps. */
    memset(samples, 28, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[1] == 0);
}

static void test_channel_retrigger(void) {
    uint8_t don[400], ka[400], samples[600];
    memset(don, 168, sizeof(don));
    memset(ka, 148, sizeof(ka));
    TjaNote notes[] = {
        {.time_ms = 0, .kind = 1},
        {.time_ms = 5, .kind = 2},
        {.time_ms = 10, .kind = 3},
        {.time_ms = 15, .kind = 4},
        {.time_ms = 16, .kind = 5}};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 8000},
        .ka = {ka, sizeof(ka), 8000},
        .chart = &(TjaChart){.notes = notes, .note_count = 5},
        .don_volume = 100,
        .ka_volume = 100};
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[39] == 168 && samples[40] == 188); /* Ka does not stop Don. */
    assert(samples[55] == 188 && samples[56] < 188 && samples[56] > 148);
    assert(samples[79] == 148 && samples[80] == 188); /* Only Don restarts. */
    assert(samples[95] == 188 && samples[96] < 188 && samples[96] > 168);
    assert(samples[119] == 168 && samples[120] == 188); /* Only Ka restarts. */
    assert(samples[479] == 188 && samples[480] == 148);
    assert(samples[519] == 148 && samples[520] == 128); /* Final tails end naturally. */
    for(size_t i = 0; i < sizeof(samples); i++)
        assert(samples[i] <= 188);
}

static void test_buffer_boundaries_and_seeking(void) {
    uint8_t don[177], ka[73], original[5000], full[5000], streamed[5000], seeked[257];
    for(size_t i = 0; i < sizeof(don); i++)
        don[i] = 80 + i % 100;
    for(size_t i = 0; i < sizeof(ka); i++)
        ka[i] = 100 + i % 60;
    for(size_t i = 0; i < sizeof(original); i++)
        original[i] = 64 + i % 128;
    const TjaNote notes[] = {
        {.time_ms = 0, .kind = 1},
        {.time_ms = 1, .kind = 2},
        {.time_ms = 8, .kind = 3},
        {.time_ms = 9, .kind = 4},
        {.time_ms = 10, .kind = 1},
        {.time_ms = 87, .kind = 1},
        {.time_ms = 88, .kind = 2},
        {.time_ms = 89, .kind = 1}};
    TaikoMixer mixer = {
        {don, sizeof(don), 22050},
        {ka, sizeof(ka), 48000},
        &(TjaChart){.notes = (TjaNote*)notes, .note_count = 8},
        70,
        30};
    const uint32_t rates[] = {8000, 22050, 44100, 48000};
    for(size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
        memcpy(full, original, sizeof(full));
        taiko_mixer_mix(&mixer, full, sizeof(full), rates[r], rates[r], 0);
        memcpy(streamed, original, sizeof(streamed));
        /* Match output regardless of DMA split or the mixer's own block boundary. */
        taiko_mixer_mix(&mixer, streamed, 4096, rates[r], rates[r], 0);
        taiko_mixer_mix(
            &mixer, streamed + 4096, sizeof(streamed) - 4096, rates[r], rates[r], 4096);
        assert(memcmp(full, streamed, sizeof(full)) == 0);
        for(size_t pos = 0; pos + sizeof(seeked) <= sizeof(full); pos += 113) {
            memcpy(seeked, original + pos, sizeof(seeked));
            taiko_mixer_mix(&mixer, seeked, sizeof(seeked), rates[r], rates[r], pos);
            assert(memcmp(seeked, full + pos, sizeof(seeked)) == 0);
        }
    }
}

static void test_missing_effects(void) {
    const TjaNote notes[] = {{.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 2}};
    uint8_t samples[] = {0, 64, 128, 192, 255};
    const uint8_t original[] = {0, 64, 128, 192, 255};
    TaikoMixer mixer = {.chart = &(TjaChart){.notes = (TjaNote*)notes, .note_count = 2}};
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 22050, 22050, 0);
    assert(memcmp(samples, original, sizeof(samples)) == 0);
}

static void test_auto_retrigger(void) {
    uint8_t don[2000], samples[2400];
    memset(don, 168, sizeof(don));
    TjaHitRun runs[] = {
        {.first_ms = 0, .interval_ms = 10, .count = 3},
        {.first_ms = 40, .interval_ms = 10, .count = 1}};
    TjaChart chart = {.hit_runs = runs, .hit_run_count = 2};
    TaikoMixer mixer = {.don = {don, sizeof(don), 8000}, .chart = &chart, .don_volume = 100};
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 168 && samples[55] == 168);
    assert(samples[56] < 168 && samples[56] > 128);
    assert(samples[79] == 128 && samples[80] == 168);
    assert(samples[159] == 128 && samples[160] == 168);
    /* A later run replaces the same auto voice; the final hit keeps its tail. */
    assert(samples[319] == 128 && samples[320] == 168);
    assert(samples[2319] == 168 && samples[2320] == 128);
    for(size_t i = 0; i < sizeof(samples); i++)
        assert(samples[i] <= 168); /* Long auto samples never accumulate. */

    TjaNote notes[] = {{.time_ms = 5, .kind = 1}, {.time_ms = 10, .kind = 2}};
    const uint8_t ka[] = {148, 108};
    chart.notes = notes;
    chart.note_count = 2;
    mixer.ka = (TaikoPcmClip){ka, sizeof(ka), 8000};
    mixer.ka_volume = 100;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[39] == 128 && samples[40] == 168); /* Normal Don replaces auto Don. */
    assert(samples[79] == 128); /* The next auto Don replaces the normal Don. */
    assert(samples[80] == 188 && samples[81] == 148); /* Normal Ka is independent. */
    assert(samples[319] == 128 && samples[320] == 168);

    /* Short clips finish naturally, without a fade caused by a later hit. */
    mixer.don.count = 16;
    chart.note_count = 0;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[15] == 168 && samples[16] == 128 && samples[80] == 168);
}

static void test_auto_retrigger_boundaries(void) {
    uint8_t don[2400], full[6000], streamed[6000], seeked[257];
    for(size_t i = 0; i < sizeof(don); i++)
        don[i] = 80 + i % 100;
    TjaHitRun runs[] = {
        {.first_ms = 0, .interval_ms = 31.25, .count = 3},
        {.first_ms = 130, .interval_ms = 62.5, .count = 3}};
    TjaNote notes[] = {{.time_ms = 81, .kind = 3}, {.time_ms = 130, .kind = 4}};
    TjaChart chart = {.hit_runs = runs, .hit_run_count = 2, .notes = notes, .note_count = 2};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 8000},
        .ka = {don, sizeof(don), 22050},
        .chart = &chart,
        .don_volume = 70,
        .ka_volume = 30};
    const uint32_t rates[] = {8000, 22050, 44100, 48000};
    for(size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
        memset(full, 128, sizeof(full));
        taiko_mixer_mix(&mixer, full, sizeof(full), rates[r], rates[r], 0);
        memset(streamed, 128, sizeof(streamed));
        for(size_t pos = 0; pos < sizeof(streamed);) {
            size_t count = sizeof(streamed) - pos;
            if(count > 61) count = 61; /* Splits inside fades, with the next hit outside. */
            taiko_mixer_mix(&mixer, streamed + pos, count, rates[r], rates[r], pos);
            pos += count;
        }
        assert(memcmp(full, streamed, sizeof(full)) == 0);
        for(size_t pos = 0; pos + sizeof(seeked) <= sizeof(full); pos += 113) {
            memset(seeked, 128, sizeof(seeked));
            taiko_mixer_mix(&mixer, seeked, sizeof(seeked), rates[r], rates[r], pos);
            assert(memcmp(seeked, full + pos, sizeof(seeked)) == 0);
        }
    }
}

static void test_long_note_with_shorter_hits(void) {
    uint8_t don[8000], samples[10000];
    memset(don, 168, sizeof(don));
    const uint8_t ka[] = {148};
    TjaNote notes[] = {{.time_ms = 5, .kind = 1}, {.time_ms = 120, .kind = 2}};
    TjaHitRun run = {.first_ms = 0, .interval_ms = 10, .count = 3};
    TjaChart chart = {.notes = notes, .note_count = 2, .hit_runs = &run, .hit_run_count = 1};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 8000},
        .ka = {ka, sizeof(ka), 8000},
        .chart = &chart,
        .don_volume = 100,
        .ka_volume = 100};
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    /* A short Ka must not discard the long Don tail, and replaced normal Don
     * must never resume after a later automatic Don. */
    assert(samples[960] == 188 && samples[961] == 168);
    assert(samples[8039] == 168 && samples[8040] == 168);
    assert(samples[8159] == 168 && samples[8160] == 128);

    mixer.don_volume = 0;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    for(size_t i = 0; i < sizeof(samples); i++)
        assert(samples[i] == (i == 960 ? 148 : 128));
}

static void test_independent_volume(void) {
    const uint8_t don[] = {228, 28};
    const uint8_t ka[] = {88, 168};
    const TjaNote notes[] = {{.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 2}};
    TaikoMixer mixer = {
        {don, 2, 8000},
        {ka, 2, 8000},
        &(TjaChart){.notes = (TjaNote*)notes, .note_count = 2},
        50,
        25};
    uint8_t samples[] = {128, 128};
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 168 && samples[1] == 88);
    mixer.ka_volume = 0;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 178 && samples[1] == 78);
    mixer.don_volume = 0;
    mixer.ka_volume = 100;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 88 && samples[1] == 168);
    mixer.ka_volume = 0;
    memset(samples, 200, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 8000, 0);
    assert(samples[0] == 200 && samples[1] == 200); /* Muting drums keeps the backing track. */
}

static void test_master_volume(void) {
    const uint8_t original[] = {0, 64, 128, 192, 255};
    const uint8_t half[] = {64, 96, 128, 160, 191};
    uint8_t samples[sizeof(original)];
    memcpy(samples, original, sizeof(samples));
    taiko_mixer_apply_volume(samples, sizeof(samples), 100);
    assert(memcmp(samples, original, sizeof(samples)) == 0);
    taiko_mixer_apply_volume(samples, sizeof(samples), 50);
    assert(memcmp(samples, half, sizeof(samples)) == 0);
    taiko_mixer_apply_volume(samples, sizeof(samples), 0);
    for(size_t i = 0; i < sizeof(samples); i++)
        assert(samples[i] == 128);

    const uint8_t don[] = {228, 28};
    const uint8_t ka[] = {88, 168};
    const TjaNote notes[] = {{.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 2}};
    TaikoMixer mixer = {
        {don, 2, 8000},
        {ka, 2, 8000},
        &(TjaChart){.notes = (TjaNote*)notes, .note_count = 2},
        50,
        25};
    uint8_t mixed[] = {168, 88};
    taiko_mixer_mix(&mixer, mixed, sizeof(mixed), 8000, 8000, 0);
    /* Music and both drum channels are attenuated together after mixing. */
    assert(mixed[0] == 208 && mixed[1] == 48);
    taiko_mixer_apply_volume(mixed, sizeof(mixed), 50);
    assert(mixed[0] == 168 && mixed[1] == 88);
    taiko_mixer_apply_volume(mixed, sizeof(mixed), 0);
    assert(mixed[0] == 128 && mixed[1] == 128);
}

static void test_master_volume_boost(void) {
    const uint8_t original[] = {0, 32, 64, 96, 127, 128, 129, 160, 191, 192, 224, 255};
    const uint8_t boosted[] = {0, 0, 0, 64, 126, 128, 130, 192, 254, 255, 255, 255};
    const uint8_t one_and_half[] = {0, 0, 32, 80, 127, 128, 129, 176, 222, 224, 255, 255};
    uint8_t samples[sizeof(original)];
    memcpy(samples, original, sizeof(samples));
    taiko_mixer_apply_volume(samples, sizeof(samples), 150);
    assert(memcmp(samples, one_and_half, sizeof(samples)) == 0);
    memcpy(samples, original, sizeof(samples));
    taiko_mixer_apply_volume(samples, sizeof(samples), 200);
    assert(memcmp(samples, boosted, sizeof(samples)) == 0);
    memcpy(samples, original, sizeof(samples));
    taiko_mixer_apply_volume(samples, sizeof(samples), 255);
    assert(memcmp(samples, boosted, sizeof(samples)) == 0); /* Cap invalid API input. */

    for(unsigned volume = 0; volume <= 200; volume += 10) {
        uint8_t ramp[256];
        for(size_t i = 0; i < sizeof(ramp); i++)
            ramp[i] = i;
        taiko_mixer_apply_volume(ramp, sizeof(ramp), volume);
        assert(ramp[128] == 128); /* Gain never changes the silence center. */
        for(size_t i = 1; i < sizeof(ramp); i++)
            assert(ramp[i] >= ramp[i - 1]); /* Saturate instead of wrapping peaks. */
    }

    const uint8_t don[] = {168, 88};
    const TjaNote notes[] = {{.time_ms = 0, .kind = 1}};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 8000},
        .chart = &(TjaChart){.notes = (TjaNote*)notes, .note_count = 1},
        .don_volume = 50};
    uint8_t mixed[] = {148, 108};
    taiko_mixer_mix(&mixer, mixed, sizeof(mixed), 8000, 8000, 0);
    assert(mixed[0] == 168 && mixed[1] == 88);
    taiko_mixer_apply_volume(mixed, sizeof(mixed), 200);
    assert(mixed[0] == 208 && mixed[1] == 48); /* Music and drums both get the boost. */
}

static void test_drum_pitch_at_playback_speeds(void) {
    uint8_t don[2205], ka[4410], samples[48000], streamed[48000], seeked[257];
    for(size_t i = 0; i < sizeof(don); i++)
        don[i] = 100 + i % 57;
    for(size_t i = 0; i < sizeof(ka); i++)
        ka[i] = 100 + i % 79;
    TjaNote notes[] = {{.time_ms = 100, .kind = 1}, {.time_ms = 500, .kind = 4}};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 22050},
        .ka = {ka, sizeof(ka), 44100},
        .chart = &(TjaChart){.notes = notes, .note_count = 2},
        .don_volume = 100,
        .ka_volume = 100};
    const uint32_t rates[] = {8000, 22050, 44100, 48000};
    for(size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
        uint32_t rate = rates[r];
        for(unsigned speed = 5; speed <= 20; speed++) {
            uint32_t output_rate = rate * speed / 10;
            memset(samples, 128, sizeof(samples));
            taiko_mixer_mix(&mixer, samples, sizeof(samples), rate, output_rate, 0);
            const TaikoPcmClip* clips[] = {&mixer.don, &mixer.ka};
            for(size_t i = 0; i < 2; i++) {
                uint32_t onset = notes[i].time_ms * rate / 1000;
                /* Both recordings last 100 ms in real time at every speed.
                 * Their onsets stay fixed in chart samples (so rhythm speeds up). */
                uint32_t duration = (output_rate + 9) / 10;
                assert(samples[onset - 1] == 128);
                for(uint32_t j = 0; j < duration; j++) {
                    uint32_t source = (uint64_t)j * clips[i]->rate / output_rate;
                    assert(samples[onset + j] == clips[i]->samples[source]);
                }
                assert(samples[onset + duration] == 128);
                /* Seek deep into the tail, including beyond its old chart-time
                 * duration at high speed. Reconstruct the same waveform. */
                uint32_t position = onset + duration - 31;
                memset(seeked, 128, sizeof(seeked));
                taiko_mixer_mix(&mixer, seeked, sizeof(seeked), rate, output_rate, position);
                assert(memcmp(seeked, samples + position, sizeof(seeked)) == 0);
            }
            memset(streamed, 128, sizeof(streamed));
            for(size_t pos = 0; pos < sizeof(streamed);) {
                size_t count = sizeof(streamed) - pos;
                if(count > 509) count = 509;
                taiko_mixer_mix(&mixer, streamed + pos, count, rate, output_rate, pos);
                pos += count;
            }
            assert(memcmp(samples, streamed, sizeof(samples)) == 0);
        }
    }
}

static void test_retrigger_fade_at_playback_speeds(void) {
    uint8_t don[8000], samples[17000], seeked[257];
    memset(don, 168, sizeof(don));
    TjaNote note = {.time_ms = 0, .kind = 3};
    TjaHitRun run = {.first_ms = 100, .interval_ms = 100, .count = 1};
    TaikoMixer mixer = {
        .don = {don, sizeof(don), 8000},
        .chart =
            &(TjaChart){.notes = &note, .note_count = 1, .hit_runs = &run, .hit_run_count = 1},
        .don_volume = 100};
    for(unsigned speed = 5; speed <= 20; speed++) {
        uint32_t output_rate = 8000 * speed / 10;
        uint32_t fade = (output_rate * 3 + 999) / 1000;
        memset(samples, 128, sizeof(samples));
        taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, output_rate, 0);
        assert(samples[800 - fade - 1] == 168);
        assert(samples[800 - fade] < 168 && samples[800 - fade] > 128);
        assert(samples[799] == 128 && samples[800] == 168);
        assert(samples[800 + output_rate - 1] == 168);
        assert(samples[800 + output_rate] == 128);
        for(size_t pos = 700; pos + sizeof(seeked) <= sizeof(samples); pos += 239) {
            memset(seeked, 128, sizeof(seeked));
            taiko_mixer_mix(&mixer, seeked, sizeof(seeked), 8000, output_rate, pos);
            assert(memcmp(seeked, samples + pos, sizeof(seeked)) == 0);
        }
    }
}

int main(void) {
    test_notes_and_clipping();
    test_channel_retrigger();
    test_buffer_boundaries_and_seeking();
    test_missing_effects();
    test_auto_retrigger();
    test_auto_retrigger_boundaries();
    test_long_note_with_shorter_hits();
    test_independent_volume();
    test_master_volume();
    test_master_volume_boost();
    test_drum_pitch_at_playback_speeds();
    test_retrigger_fade_at_playback_speeds();
    puts(
        "Drum mixing, independent retrigger, volume, clipping, fixed drum pitch at all speeds, resampling, DMA boundaries and seeking tests passed");
    return 0;
}
