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
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[0] == 128);
    assert(samples[7] == 128 && samples[8] == 228 && samples[9] == 28 && samples[10] == 178);
    assert(samples[16] == 148 && samples[17] == 108);
    assert(samples[24] == 248 && samples[25] == 8 && samples[26] == 178);
    assert(samples[32] == 128); /* Roll starts do not synthesize extra hits. */

    const uint8_t peak[] = {255, 0};
    const TjaNote stacked[] = {
        {.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 1}};
    mixer = (TaikoMixer){
        {peak, 2, 8000}, {0}, &(TjaChart){.notes = (TjaNote*)stacked, .note_count = 3}, 100, 100};
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[0] == 255 && samples[1] == 0); /* Saturate rather than wrap. */
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
        taiko_mixer_mix(&mixer, full, sizeof(full), rates[r], 0);
        memcpy(streamed, original, sizeof(streamed));
        /* Match output regardless of DMA split or the mixer's own block boundary. */
        taiko_mixer_mix(&mixer, streamed, 4096, rates[r], 0);
        taiko_mixer_mix(&mixer, streamed + 4096, sizeof(streamed) - 4096, rates[r], 4096);
        assert(memcmp(full, streamed, sizeof(full)) == 0);
        for(size_t pos = 0; pos + sizeof(seeked) <= sizeof(full); pos += 113) {
            memcpy(seeked, original + pos, sizeof(seeked));
            taiko_mixer_mix(&mixer, seeked, sizeof(seeked), rates[r], pos);
            assert(memcmp(seeked, full + pos, sizeof(seeked)) == 0);
        }
    }
}

static void test_missing_effects(void) {
    const TjaNote notes[] = {{.time_ms = 0, .kind = 1}, {.time_ms = 0, .kind = 2}};
    uint8_t samples[] = {0, 64, 128, 192, 255};
    const uint8_t original[] = {0, 64, 128, 192, 255};
    TaikoMixer mixer = {.chart = &(TjaChart){.notes = (TjaNote*)notes, .note_count = 2}};
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 22050, 0);
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
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
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
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[40] == 208); /* Normal Don overlaps the automatic voice. */
    assert(samples[79] == 168); /* Only the auto tail fades. */
    assert(samples[80] == 228 && samples[81] == 188); /* Normal Ka is independent. */
    assert(samples[319] == 168 && samples[320] == 208);

    /* Short clips finish naturally, without a fade caused by a later hit. */
    mixer.don.count = 16;
    chart.note_count = 0;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
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
        taiko_mixer_mix(&mixer, full, sizeof(full), rates[r], 0);
        memset(streamed, 128, sizeof(streamed));
        for(size_t pos = 0; pos < sizeof(streamed);) {
            size_t count = sizeof(streamed) - pos;
            if(count > 61) count = 61; /* Splits inside fades, with the next hit outside. */
            taiko_mixer_mix(&mixer, streamed + pos, count, rates[r], pos);
            pos += count;
        }
        assert(memcmp(full, streamed, sizeof(full)) == 0);
        for(size_t pos = 0; pos + sizeof(seeked) <= sizeof(full); pos += 113) {
            memset(seeked, 128, sizeof(seeked));
            taiko_mixer_mix(&mixer, seeked, sizeof(seeked), rates[r], pos);
            assert(memcmp(seeked, full + pos, sizeof(seeked)) == 0);
        }
    }
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
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[0] == 168 && samples[1] == 88);
    mixer.ka_volume = 0;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[0] == 178 && samples[1] == 78);
    mixer.don_volume = 0;
    mixer.ka_volume = 100;
    memset(samples, 128, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
    assert(samples[0] == 88 && samples[1] == 168);
    mixer.ka_volume = 0;
    memset(samples, 200, sizeof(samples));
    taiko_mixer_mix(&mixer, samples, sizeof(samples), 8000, 0);
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
    taiko_mixer_mix(&mixer, mixed, sizeof(mixed), 8000, 0);
    /* Music and both drum channels are attenuated together after mixing. */
    assert(mixed[0] == 208 && mixed[1] == 48);
    taiko_mixer_apply_volume(mixed, sizeof(mixed), 50);
    assert(mixed[0] == 168 && mixed[1] == 88);
    taiko_mixer_apply_volume(mixed, sizeof(mixed), 0);
    assert(mixed[0] == 128 && mixed[1] == 128);
}

int main(void) {
    test_notes_and_clipping();
    test_buffer_boundaries_and_seeking();
    test_missing_effects();
    test_auto_retrigger();
    test_auto_retrigger_boundaries();
    test_independent_volume();
    test_master_volume();
    puts(
        "Drum mixing, volume, overlap, clipping, resampling, DMA boundaries and seeking tests passed");
    return 0;
}
