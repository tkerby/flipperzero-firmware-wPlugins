#include "taiko_lane.h"

#include <assert.h>
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
    size_t count = taiko_lane_collect(notes, 4, 0, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 2 && visible[0].x == 82 && visible[1].x == 114);
    count = taiko_lane_collect(notes, 4, 500, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 4 && visible[0].x == TAIKO_LANE_HIT_X);
    assert(visible[2].kind == 2 && visible[2].x == 82);
    assert(visible[3].kind == 4 && visible[3].x == 114);

    /* A time jump skips old notes, without needing intermediate frame updates. */
    count = taiko_lane_collect(notes, 4, 1225, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].kind == 4 && visible[0].x == 21);
    assert(taiko_lane_collect(notes, 4, 1500, visible, TAIKO_LANE_MAX_NOTES) == 0);

    TjaNote mixed[] = {
        {.time_ms = 0, .kind = 5},
        {.time_ms = 1, .kind = 6},
        {.time_ms = 2, .kind = 7},
        {.time_ms = 3, .kind = 8},
        {.time_ms = 4, .kind = 9},
        {.time_ms = 5, .kind = 1},
        {.time_ms = 6, .kind = 4},
    };
    count = taiko_lane_collect(mixed, 7, 0, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 2 && visible[0].kind == 1 && visible[1].kind == 4);
    assert(taiko_lane_collect(mixed, 7, 0, visible, 1) == 1);

    /* A large note can remain partly visible after a normal note has left. */
    TjaNote edge[] = {{.time_ms = 1820, .kind = 1}, {.time_ms = 1820, .kind = 3}};
    count = taiko_lane_collect(edge, 2, 2000, visible, TAIKO_LANE_MAX_NOTES);
    assert(count == 1 && visible[0].kind == 3 && visible[0].x == -5);

    assert(taiko_lane_collect(NULL, 0, 0, visible, TAIKO_LANE_MAX_NOTES) == 0);
    assert(taiko_lane_collect(notes, 4, 0, NULL, 0) == 0);
    assert(taiko_lane_collect(notes, 4, UINT32_MAX, visible, TAIKO_LANE_MAX_NOTES) == 0);
    TjaNote very_old[] = {{.time_ms = INT32_MIN, .kind = 1}};
    assert(taiko_lane_collect(very_old, 1, 0, visible, TAIKO_LANE_MAX_NOTES) == 0);
    puts("Lane timing, visibility, large notes and time-jump tests passed");
    return 0;
}
