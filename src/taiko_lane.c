#include "taiko_lane.h"

size_t taiko_lane_collect(
    const TjaNote* notes,
    size_t count,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneNote* visible,
    size_t capacity) {
    if(!scroll_tenths) return 0;
    const int64_t earliest =
        (int64_t)position_ms - (TAIKO_LANE_HIT_X + TAIKO_LANE_BIG_RADIUS + 1) * 10000 /
                                   (TAIKO_LANE_PIXELS_PER_SECOND * scroll_tenths);
    size_t low = 0, high = count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(notes[mid].time_ms < earliest)
            low = mid + 1;
        else
            high = mid;
    }
    size_t result = 0;
    for(size_t i = low; i < count && result < capacity; i++) {
        int64_t delta = (int64_t)notes[i].time_ms - position_ms;
        int64_t x =
            TAIKO_LANE_HIT_X + delta * TAIKO_LANE_PIXELS_PER_SECOND * scroll_tenths / 10000;
        if(x > 127 + TAIKO_LANE_BIG_RADIUS) break;
        uint8_t kind = notes[i].kind;
        if(kind < 1 || kind > 4) continue;
        int radius = kind >= 3 ? TAIKO_LANE_BIG_RADIUS : TAIKO_LANE_SMALL_RADIUS;
        if(x < -radius || x > 127 + radius) continue;
        visible[result++] = (TaikoLaneNote){.x = (int16_t)x, .kind = kind};
    }
    return result;
}
