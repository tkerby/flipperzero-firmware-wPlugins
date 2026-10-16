// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_hits.h"
#include "taiko_lane.h"
#include "taiko_mixer.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int read_line(void* context, char* line, size_t capacity) {
    return fgets(line, (int)capacity, context) != NULL;
}

static bool parse(const char* text, TjaCourse course, TjaChart* chart) {
    FILE* file = tmpfile();
    assert(file);
    fputs(text, file);
    rewind(file);
    const char* error;
    bool success = tja_parse_course(read_line, file, course, chart, &error);
    fclose(file);
    return success;
}

static void test_counts_and_seeks(void) {
    TjaChart chart;
    assert(parse(
        "BPM:120\nWAVE:a.ogg\nBALLOON:3,40\n#START\n5008,\n6008,\n7008,\n9008,\n12,\n#END\n",
        TjaCourseOni,
        &chart));
    assert(chart.note_count == 2 && chart.roll_count == 4 && chart.hit_run_count == 4);
    assert(chart.rolls[0].time_ms == 0 && chart.rolls[0].end_ms == 1500);
    assert(chart.rolls[0].hit_count == 24 && chart.rolls[1].hit_count == 24);
    assert(chart.rolls[2].balloon_count == 3 && chart.rolls[2].hit_count == 3);
    assert(chart.rolls[3].balloon_count == 40 && chart.rolls[3].hit_count == 24);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[2], 3999) == 0);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[2], 4000) == 2);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[2], 4063) == 1);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[2], 4125) == 0);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[2], 4062) == 2);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[3], 7499) == 16);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[3], 7500) == 0);
    TaikoHitCursor cursor;
    taiko_hits_seek(&chart, &cursor, 0);
    TaikoHit hit, expected[77];
    size_t count = 0;
    while(taiko_hits_peek(&chart, &cursor, &hit)) {
        assert(count < 77);
        assert(hit.right == (count % 2 == 0));
        if(count) assert(hit.time_ms >= expected[count - 1].time_ms);
        expected[count++] = hit;
        taiko_hits_advance(&chart, &cursor);
    }
    assert(count == 77 && expected[76].rim);
    for(size_t i = 0; i < count; i++) {
        taiko_hits_seek(&chart, &cursor, expected[i].time_ms);
        assert(taiko_hits_peek(&chart, &cursor, &hit));
        assert(hit.time_ms == expected[i].time_ms && hit.right == expected[i].right);
    }
    taiko_hits_seek(&chart, &cursor, 10000);
    assert(!taiko_hits_peek(&chart, &cursor, &hit));
    tja_chart_free(&chart);
}

static void test_tempo_phase_and_offset(void) {
    TjaChart chart;
    assert(parse(
        "BPM:120\nWAVE:a.ogg\n#START\n50\n#DELAY 0.03\n#BPMCHANGE 240\n08,\n#END\n",
        TjaCourseOni,
        &chart));
    assert(chart.roll_count == 1 && chart.hit_run_count == 2);
    assert(chart.hit_runs[0].count == 17 && chart.hit_runs[0].interval_ms == 62.5);
    assert(chart.hit_runs[1].count == 8 && fabs(chart.hit_runs[1].interval_ms - 31.25) < 1e-6);
    assert(fabs(chart.hit_runs[1].first_ms - 1046.25) < 0.001);
    assert(chart.rolls[0].end_ms == 1280);
    assert(taiko_hit_run_time(&chart.hit_runs[1], 7) == 1265);
    tja_chart_free(&chart);
    assert(parse("BPM:120\nWAVE:a.ogg\nOFFSET:0.5\n#START\n5008,\n#END\n", TjaCourseOni, &chart));
    TaikoHitCursor cursor;
    TaikoHit hit;
    taiko_hits_seek(&chart, &cursor, 0);
    assert(taiko_hits_peek(&chart, &cursor, &hit));
    assert(hit.time_ms == 0 && hit.right && cursor.ordinal == 8);
    tja_chart_free(&chart);
    /* A long roll remains compact and is capped even at an extreme chart BPM. */
    assert(
        parse("BPM:10000\nWAVE:a.ogg\n#START\n5\n#DELAY 3599\n8,\n#END\n", TjaCourseOni, &chart));
    assert(chart.note_count == 0 && chart.hit_run_count == 1);
    assert(chart.rolls[0].hit_count > 115100 && chart.rolls[0].hit_count < 115200);
    taiko_hits_seek(&chart, &cursor, chart.rolls[0].end_ms - 1);
    assert(cursor.ordinal > 115100);
    tja_chart_free(&chart);
}

static void assert_rate_cap(const TjaChart* chart, unsigned speed) {
    double last = 0;
    bool have_last = false;
    for(size_t i = 0; i < chart->hit_run_count; i++) {
        const TjaHitRun* run = &chart->hit_runs[i];
        assert(run->interval_ms * 10 / speed >= 31.25 - 1e-6);
        for(uint32_t j = 0; j < run->count; j++) {
            double time = run->first_ms + j * run->interval_ms;
            if(have_last) assert((time - last) * 10 / speed >= 31.25 - 1e-6);
            last = time;
            have_last = true;
        }
    }
}

static void test_rate_cap(void) {
    TjaChart chart;
    const char* error;
    assert(parse("BPM:300\nWAVE:a.ogg\n#START\n5008,\n#END\n", TjaCourseOni, &chart));
    assert(chart.rolls[0].end_ms == 600 && chart.rolls[0].hit_count == 20);
    assert_rate_cap(&chart, 10);
    for(unsigned speed = 5; speed <= 20; speed++) {
        assert(tja_chart_set_roll_speed(&chart, speed, &error) && !error);
        assert_rate_cap(&chart, speed);
        if(speed == 5) assert(chart.rolls[0].hit_count == 24);
        if(speed == 20) assert(chart.rolls[0].hit_count == 10);
    }
    TjaRoll* original_rolls = chart.rolls;
    TjaHitRun* original_runs = chart.hit_runs;
    assert(tja_chart_set_roll_speed(&chart, 20, &error));
    assert(chart.rolls == original_rolls && chart.hit_runs == original_runs);
    assert(!tja_chart_set_roll_speed(&chart, 0, &error) && error);
    assert(chart.roll_speed_tenths == 20 && chart.hit_runs == original_runs);
    const uint8_t don[] = {228};
    TaikoMixer mixer = {{don, 1, 8000}, {0}, &chart, 100, 100};
    uint8_t mixed[4800];
    memset(mixed, 128, sizeof(mixed));
    taiko_mixer_mix(&mixer, mixed, sizeof(mixed), 8000, 0);
    assert(mixed[0] == 228 && mixed[400] == 128 && mixed[504] == 228 && mixed[1000] == 228);
    tja_chart_free(&chart);

    assert(parse("BPM:300\nWAVE:a.ogg\nBALLOON:12\n#START\n7008,\n#END\n", TjaCourseOni, &chart));
    assert(chart.rolls[0].hit_count == 12);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[0], 599) == 0);
    assert(tja_chart_set_roll_speed(&chart, 20, &error));
    assert(chart.rolls[0].hit_count == 10);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[0], 599) == 2);
    TaikoHitCursor cursor;
    TaikoHit hit;
    taiko_hits_seek(&chart, &cursor, 500);
    assert(taiko_hits_peek(&chart, &cursor, &hit) && hit.time_ms == 500);
    assert(cursor.ordinal == 8 && hit.right);
    assert(tja_chart_set_roll_speed(&chart, 10, &error));
    assert(taiko_balloon_remaining(&chart, &chart.rolls[0], 599) == 0);
    tja_chart_free(&chart);

    const char* fixtures[] = {
        "BPM:240\nWAVE:a.ogg\n#START\n5\n#DELAY 0.01\n#BPMCHANGE 480\n"
        "0\n#BPMCHANGE 120\n08,\n#END\n",
        "BPM:300\nWAVE:a.ogg\nBALLOON:40\n#START\n5578,\n#END\n",
        "BPM:10000\nWAVE:a.ogg\nBALLOON:40\n#START\n5578,\n#END\n",
    };
    for(size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        assert(parse(fixtures[i], TjaCourseOni, &chart));
        for(unsigned speed = 5; speed <= 20; speed++) {
            assert(tja_chart_set_roll_speed(&chart, speed, &error));
            assert_rate_cap(&chart, speed);
        }
        if(i == 2)
            assert(
                chart.rolls[0].hit_count == 1 && chart.rolls[1].hit_count == 0 &&
                chart.rolls[2].hit_count == 0);
        tja_chart_free(&chart);
    }
}

static void test_lane(void) {
    TjaChart chart;
    assert(parse(
        "BPM:180\nWAVE:a.ogg\nOFFSET:-1\nBALLOON:3\n#START\n5008,\n7008,\n#END\n",
        TjaCourseOni,
        &chart));
    TaikoLaneRoll visible[16];
    TaikoLaneBalloon balloon;
    size_t count = taiko_lane_collect_rolls(&chart, 1300, 10, visible, 16, &balloon);
    assert(count == 2 && visible[0].x < TAIKO_LANE_HIT_X && visible[1].kind == 7);
    assert(visible[0].tail_x > visible[0].x && balloon.remaining == 0);
    int head_before = visible[0].x, tail_before = visible[0].tail_x;
    taiko_lane_collect_rolls(&chart, 1400, 10, visible, 16, &balloon);
    assert(visible[0].x < head_before && visible[0].tail_x < tail_before);
    /* Keep drawing after note 8 until the tail has drifted off the screen. */
    count =
        taiko_lane_collect_rolls(&chart, chart.rolls[0].end_ms + 50, 10, visible, 16, &balloon);
    assert(count >= 1 && visible[0].kind == 5 && visible[0].tail_x < TAIKO_LANE_HIT_X);
    chart.rolls[0].time_ms = -100000;
    count = taiko_lane_collect_rolls(&chart, 1300, 10, visible, 16, &balloon);
    assert(count == 2 && visible[0].x < 0 && visible[0].tail_x > 0);
    chart.rolls[0].scroll_speed = -1;
    assert(taiko_lane_collect_rolls(&chart, 1300, 10, visible, 16, &balloon) == 2);
    assert(visible[0].x > 127 && visible[0].tail_x < TAIKO_LANE_HIT_X);
    /* Reverse-scroll balloons trail toward the left and may be partly clipped. */
    chart.rolls[1].scroll_speed = -0.1f;
    assert(taiko_lane_collect_rolls(&chart, 1300, 10, visible, 16, &balloon) == 2);
    assert(visible[1].kind == 7 && visible[1].tail_x < visible[1].x);
    taiko_lane_collect_rolls(&chart, chart.rolls[1].time_ms, 10, visible, 16, &balloon);
    assert(balloon.remaining == 2 && balloon.total == 3 && balloon.direction == -1);
    taiko_lane_collect_rolls(&chart, chart.rolls[1].time_ms + 100, 10, visible, 16, &balloon);
    assert(balloon.remaining == 0);
    taiko_lane_collect_rolls(&chart, chart.rolls[1].time_ms, 10, visible, 0, &balloon);
    assert(balloon.remaining == 2); /* A full lane never suppresses the balloon overlay. */
    tja_chart_free(&chart);
}

static void test_next_note_interrupts(void) {
    TjaChart chart;
    assert(parse("BPM:120\nWAVE:a.ogg\nBALLOON:40\n#START\n7208,\n#END\n", TjaCourseOni, &chart));
    assert(chart.roll_count == 1 && chart.note_count == 1);
    assert(chart.rolls[0].end_ms == 500 && chart.rolls[0].hit_count == 8);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[0], 499) == 32);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[0], 500) == 0);
    TaikoLaneBalloon balloon;
    TaikoLaneRoll visible[16];
    taiko_lane_collect_rolls(&chart, 499, 10, visible, 16, &balloon);
    assert(balloon.remaining == 32 && balloon.total == 40);
    taiko_lane_collect_rolls(&chart, 500, 10, visible, 16, &balloon);
    assert(balloon.remaining == 0 && balloon.total == 0);
    TaikoHitCursor cursor;
    TaikoHit hit;
    taiko_hits_seek(&chart, &cursor, 500);
    assert(taiko_hits_peek(&chart, &cursor, &hit));
    assert(hit.time_ms == 500 && hit.rim && hit.right && cursor.ordinal == 8);
    taiko_hits_advance(&chart, &cursor);
    assert(!taiko_hits_peek(&chart, &cursor, &hit));
    /* A 32nd-note onset coincides with the Ka. Only Ka may sound at the cutoff. */
    const uint8_t don[] = {228}, ka[] = {88};
    TaikoMixer mixer = {{don, 1, 8000}, {ka, 1, 8000}, &chart, 100, 100};
    uint8_t mixed[8000];
    memset(mixed, 128, sizeof(mixed));
    taiko_mixer_mix(&mixer, mixed, sizeof(mixed), 8000, 0);
    assert(mixed[3504] == 228 && mixed[4000] == 88 && mixed[4504] == 128);
    tja_chart_free(&chart);

    assert(parse("BPM:120\nWAVE:a.ogg\n#START\n5208,\n#END\n", TjaCourseOni, &chart));
    assert(chart.rolls[0].end_ms == 500 && chart.rolls[0].hit_count == 8);
    tja_chart_free(&chart);
    assert(parse("BPM:120\nWAVE:a.ogg\nBALLOON:40\n#START\n7508,\n#END\n", TjaCourseOni, &chart));
    assert(chart.roll_count == 2 && chart.rolls[0].end_ms == 500);
    assert(
        chart.rolls[1].kind == 5 && chart.rolls[1].time_ms == 500 &&
        chart.rolls[1].hit_count == 16);
    tja_chart_free(&chart);
    assert(
        parse("BPM:120\nWAVE:a.ogg\nBALLOON:40,3\n#START\n7708,\n#END\n", TjaCourseOni, &chart));
    assert(chart.roll_count == 2 && chart.rolls[0].end_ms == 500);
    assert(chart.rolls[1].balloon_count == 3 && chart.rolls[1].hit_count == 3);
    assert(taiko_balloon_remaining(&chart, &chart.rolls[1], 500) == 2);
    tja_chart_free(&chart);
    /* Kusudama's second 9 is a scoring marker and must not interrupt it. */
    assert(parse("BPM:120\nWAVE:a.ogg\nBALLOON:40\n#START\n9908,\n#END\n", TjaCourseOni, &chart));
    assert(
        chart.roll_count == 1 && chart.rolls[0].end_ms == 1500 && chart.rolls[0].hit_count == 24);
    tja_chart_free(&chart);
}

static void test_course_and_legacy_metadata(void) {
    TjaChart chart;
    const char* text = "WAVE:a.ogg\nCOURSE:Easy\nBALLOON:20\n#START\n7008,\n#END\n"
                       "COURSE:Oni\nBALLOON:3,7\n#START\n7008,\n9008,\n#END\n";
    assert(parse(text, TjaCourseOni, &chart));
    assert(
        chart.roll_count == 2 && chart.rolls[0].balloon_count == 3 &&
        chart.rolls[1].balloon_count == 7);
    tja_chart_free(&chart);
    assert(parse("WAVE:a.ogg\n#START\n7008,\n5,\n#END\n", TjaCourseOni, &chart));
    assert(chart.rolls[0].balloon_count == 5);
    assert(chart.rolls[1].end_ms == chart.end_ms);
    tja_chart_free(&chart);
    assert(!parse("WAVE:a.ogg\nBALLOON:0\n#START\n7008,\n#END\n", TjaCourseOni, &chart));
    tja_chart_free(&chart);
}

static void test_mix_boundaries(void) {
    TjaChart chart;
    assert(parse(
        "BPM:120\nWAVE:a.ogg\nBALLOON:3\n#START\n5008,\n7008,\n12,\n#END\n", TjaCourseOni, &chart));
    uint8_t clip[777];
    for(size_t i = 0; i < sizeof(clip); i++)
        clip[i] = 128 + i % 20;
    TaikoMixer mixer = {{clip, sizeof(clip), 8000}, {clip, sizeof(clip), 8000}, &chart, 70, 30};
    uint8_t full[60000], streamed[60000], seeked[700];
    memset(full, 128, sizeof(full));
    memset(streamed, 128, sizeof(streamed));
    taiko_mixer_mix(&mixer, full, sizeof(full), 8000, 0);
    for(size_t pos = 0; pos < sizeof(streamed); pos += 4096) {
        size_t count = sizeof(streamed) - pos;
        if(count > 4096) count = 4096;
        taiko_mixer_mix(&mixer, streamed + pos, count, 8000, pos);
    }
    assert(memcmp(full, streamed, sizeof(full)) == 0);
    assert(full[109] > 128 && full[16000 + 109] > 128);
    assert(full[18000] == 128); /* Balloon already popped: stop mixing its hits. */
    for(size_t pos = 0; pos + sizeof(seeked) < sizeof(full); pos += 137) {
        memset(seeked, 128, sizeof(seeked));
        taiko_mixer_mix(&mixer, seeked, sizeof(seeked), 8000, pos);
        assert(memcmp(seeked, full + pos, sizeof(seeked)) == 0);
    }
    tja_chart_free(&chart);
}

int main(void) {
    test_counts_and_seeks();
    test_tempo_phase_and_offset();
    test_rate_cap();
    test_lane();
    test_next_note_interrupts();
    test_course_and_legacy_metadata();
    test_mix_boundaries();
    puts("Roll / balloon timing, counts, interruption, seek, tails and mixed-audio tests passed");
    return 0;
}
