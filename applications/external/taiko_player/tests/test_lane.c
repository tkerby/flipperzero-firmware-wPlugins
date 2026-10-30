// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "taiko_lane.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    TaikoLaneNote visible[TAIKO_LANE_MAX_NOTES];
    TjaNote notes[] = {
        {.time_ms = 500, .kind = 1},
        {.time_ms = 750, .kind = 3},
        {.time_ms = 1000, .kind = 2},
        {.time_ms = 1250, .kind = 4},
    };
    size_t count = taiko_lane_collect(notes, 4, 1, 0, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 2 && visible[0].x == 82 && visible[1].x == 114);
    count = taiko_lane_collect(notes, 4, 1, 500, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 3 && visible[0].kind == 3);
    assert(visible[1].kind == 2 && visible[1].x == 82);
    assert(visible[2].kind == 4 && visible[2].x == 114);

    /* A time jump skips old notes, without needing intermediate frame updates. */
    count = taiko_lane_collect(notes, 4, 1, 1225, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].kind == 4 && visible[0].x == 21);
    assert(taiko_lane_collect(notes, 4, 1, 1500, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);

    count = taiko_lane_collect(notes, 1, 1, 499, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].x == TAIKO_LANE_HIT_X);
    assert(taiko_lane_collect(notes, 1, 1, 500, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);
    assert(taiko_lane_collect(notes, 1, 1, 501, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);

    TjaNote mixed[] = {
        {.time_ms = 0, .kind = 5},
        {.time_ms = 1, .kind = 6},
        {.time_ms = 2, .kind = 7},
        {.time_ms = 3, .kind = 8},
        {.time_ms = 4, .kind = 9},
        {.time_ms = 5, .kind = 1},
        {.time_ms = 6, .kind = 4},
    };
    count = taiko_lane_collect(mixed, 7, 1, 0, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 2 && visible[0].kind == 1 && visible[1].kind == 4);
    assert(taiko_lane_collect(mixed, 7, 1, 0, 10, visible, 1) == 1);

    /* Hits disappear by time, even if their outlines would still be on screen. */
    TjaNote edge[] = {{.time_ms = 1820, .kind = 1}, {.time_ms = 1820, .kind = 3}};
    count = taiko_lane_collect(edge, 2, 1, 2000, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);

    assert(taiko_lane_collect(NULL, 0, 1, 0, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);
    assert(taiko_lane_collect(notes, 4, 1, 0, 10, NULL, 0) == 0);
    assert(taiko_lane_collect(notes, 4, 1, UINT32_MAX, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);
    TjaNote very_old[] = {{.time_ms = INT32_MIN, .kind = 1}};
    assert(taiko_lane_collect(very_old, 1, 1, 0, 10, visible, TAIKO_LANE_MAX_NOTES) == 0);
    count = taiko_lane_collect(notes, 4, 1, 0, 5, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 4 && visible[0].x == 50 && visible[3].x == 98);
    count = taiko_lane_collect(notes, 4, 1, 500, 40, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);
    assert(taiko_lane_collect(notes, 4, 1, 0, 0, visible, TAIKO_LANE_MAX_NOTES) == 0);
    /* A slower later note can enter the screen before an earlier fast note. */
    TjaNote variable[] = {
        {.time_ms = 500, .kind = 1, .scroll_speed = 4},
        {.time_ms = 1000, .kind = 2, .scroll_speed = 0.5f},
        {.time_ms = 1600, .kind = 3, .scroll_speed = 0.25f},
    };
    count = taiko_lane_collect(variable, 3, 0.25f, 0, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 2 && visible[0].x == 82 && visible[1].x == 69);
    count = taiko_lane_collect(variable, 3, 0.25f, 1000, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].kind == 3 && visible[0].x == 37);

    /* Slow notes remain visible well beyond the old fixed-speed search window. */
    TjaNote slow[] = {
        {.time_ms = 1000, .kind = 1, .scroll_speed = 0.0625f},
        {.time_ms = 5000, .kind = 2, .scroll_speed = 0.0625f},
    };
    count = taiko_lane_collect(slow, 2, 0.0625f, 2000, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].x == 42);
    count = taiko_lane_collect(slow, 2, 0.0625f, 2000, 20, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].x == 66);

    TjaNote reverse = {.time_ms = 1000, .kind = 1, .scroll_speed = -1};
    count = taiko_lane_collect(&reverse, 1, 1, 900, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].x == 6);
    count = taiko_lane_collect(&reverse, 1, 1, 1000, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);
    count = taiko_lane_collect(&reverse, 1, 1, 1100, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);
    reverse.kind = 3;
    count = taiko_lane_collect(&reverse, 1, 1, 1900, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);

    /* Clip before converting to an integer, including overflowed float offsets. */
    TjaNote extreme[] = {
        {.time_ms = 0, .kind = 1, .scroll_speed = FLT_MAX},
        {.time_ms = 1000, .kind = 2, .scroll_speed = FLT_MAX},
    };
    count = taiko_lane_collect(extreme, 2, 1, 0, 40, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 0);
    count = taiko_lane_collect(slow, 2, FLT_MIN, 2000, 10, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1);
    puts("Lane HS, reverse scroll, visibility, global flow, clipping and time-jump tests passed");
    return 0;
}
