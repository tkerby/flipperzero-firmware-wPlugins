#pragma once

#include "taiko_lane.h"
#include <gui/canvas.h>

typedef struct {
    char title[128];
    char course[24];
    uint32_t position_ms;
    uint32_t hit_until;
    bool hit_right;
    bool hit_rim;
    bool loading;
    bool finished;
    size_t lane_count;
    TaikoLaneNote lane_notes[TAIKO_LANE_MAX_NOTES];
} TaikoPlayerView;

void taiko_player_view_draw(Canvas* canvas, const TaikoPlayerView* view);
