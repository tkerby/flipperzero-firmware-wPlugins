#pragma once

#include "tja_parser.h"

#define TAIKO_LANE_MAX_NOTES         64U
#define TAIKO_LANE_HIT_X             18
#define TAIKO_LANE_PIXELS_PER_SECOND 128
#define TAIKO_LANE_SMALL_RADIUS      4
#define TAIKO_LANE_BIG_RADIUS        6

typedef struct {
    int16_t x;
    uint8_t kind;
} TaikoLaneNote;

/* Collect only visible normal notes (1..4), using absolute playback time.
 * The chart is sorted by time. A binary search avoids scanning past notes. */
size_t taiko_lane_collect(
    const TjaNote* notes,
    size_t count,
    uint32_t position_ms,
    TaikoLaneNote* visible,
    size_t capacity);
