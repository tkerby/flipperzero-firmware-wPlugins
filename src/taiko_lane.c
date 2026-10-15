#include "taiko_lane.h"

#include <math.h>

size_t taiko_lane_collect(
    const TjaNote* notes,
    size_t count,
    float min_note_speed,
    uint32_t position_ms,
    uint8_t scroll_tenths,
    TaikoLaneNote* visible,
    size_t capacity) {
    if(!scroll_tenths || !capacity || !count) return 0;
    if(!isfinite(min_note_speed) || min_note_speed <= 0) return 0;
    /* Bound both directions so reverse-scroll notes can enter from the left.
     * Cap before casting: very small positive HS can produce an infinite window. */
    float window = (127 + TAIKO_LANE_BIG_RADIUS - TAIKO_LANE_HIT_X + 1) * 10000.0f /
                   (TAIKO_LANE_PIXELS_PER_SECOND * scroll_tenths * min_note_speed);
    int64_t span = window < UINT32_MAX ? (int64_t)window + 1 : (int64_t)UINT32_MAX;
    const int64_t earliest = (int64_t)position_ms - span;
    const int64_t latest = (int64_t)position_ms + span;
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
        if(notes[i].time_ms > latest) break;
        uint8_t kind = notes[i].kind;
        if(kind < 1 || kind > 4) continue;
        float speed = notes[i].scroll_speed ? notes[i].scroll_speed : 1;
        float offset =
            (float)delta * speed * scroll_tenths * (TAIKO_LANE_PIXELS_PER_SECOND / 10000.0f);
        int radius = kind >= 3 ? TAIKO_LANE_BIG_RADIUS : TAIKO_LANE_SMALL_RADIUS;
        if(!isfinite(offset) || offset <= -TAIKO_LANE_HIT_X - radius - 1 ||
           offset >= 127 + radius - TAIKO_LANE_HIT_X + 1)
            continue;
        int x = TAIKO_LANE_HIT_X + (int)offset;
        if(x < -radius || x > 127 + radius) continue;
        visible[result++] = (TaikoLaneNote){.x = (int16_t)x, .kind = kind};
    }
    return result;
}
