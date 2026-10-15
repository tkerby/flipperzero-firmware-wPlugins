#include "taiko_hits.h"

#include <math.h>

int32_t taiko_hit_run_time(const TjaHitRun* run, uint32_t index) {
    return (int32_t)lround((double)run->first_ms + (double)index * run->interval_ms);
}

static uint32_t run_at(const TjaHitRun* run, int64_t time_ms) {
    uint32_t low = 0, high = run->count;
    while(low < high) {
        uint32_t mid = low + (high - low) / 2;
        if(taiko_hit_run_time(run, mid) < time_ms)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

static void skip_markers(const TjaChart* chart, TaikoHitCursor* cursor) {
    while(cursor->note < chart->note_count &&
          (chart->notes[cursor->note].kind < 1 || chart->notes[cursor->note].kind > 4))
        cursor->note++;
}

void taiko_hits_seek(const TjaChart* chart, TaikoHitCursor* cursor, int64_t time_ms) {
    size_t low = 0, high = chart->note_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(chart->notes[mid].time_ms < time_ms)
            low = mid + 1;
        else
            high = mid;
    }
    *cursor = (TaikoHitCursor){.note = low, .ordinal = low};
    while(cursor->run < chart->hit_run_count) {
        const TjaHitRun* run = &chart->hit_runs[cursor->run];
        cursor->repeat = run_at(run, time_ms);
        cursor->ordinal += cursor->repeat;
        if(cursor->repeat < run->count) break;
        cursor->run++;
        cursor->repeat = 0;
    }
    skip_markers(chart, cursor);
}

static bool use_roll(const TjaChart* chart, const TaikoHitCursor* cursor) {
    return cursor->run < chart->hit_run_count &&
           (cursor->note == chart->note_count ||
            taiko_hit_run_time(&chart->hit_runs[cursor->run], cursor->repeat) <
                chart->notes[cursor->note].time_ms);
}

bool taiko_hits_peek(const TjaChart* chart, const TaikoHitCursor* cursor, TaikoHit* hit) {
    if(cursor->note == chart->note_count && cursor->run == chart->hit_run_count) return false;
    bool roll = use_roll(chart, cursor);
    *hit = (TaikoHit){
        .time_ms = roll ? taiko_hit_run_time(&chart->hit_runs[cursor->run], cursor->repeat) :
                          chart->notes[cursor->note].time_ms,
        .rim = !roll &&
               (chart->notes[cursor->note].kind == 2 || chart->notes[cursor->note].kind == 4),
        .right = (cursor->ordinal % 2) == 0,
        .roll = roll};
    return true;
}

void taiko_hits_advance(const TjaChart* chart, TaikoHitCursor* cursor) {
    if(use_roll(chart, cursor)) {
        if(++cursor->repeat == chart->hit_runs[cursor->run].count) {
            cursor->run++;
            cursor->repeat = 0;
        }
    } else if(cursor->note < chart->note_count) {
        cursor->note++;
        skip_markers(chart, cursor);
    } else {
        return;
    }
    cursor->ordinal++;
}

uint32_t taiko_balloon_remaining(const TjaChart* chart, const TjaRoll* roll, uint32_t time_ms) {
    if(!roll->balloon_count || (int64_t)time_ms < roll->time_ms ||
       (int64_t)time_ms >= roll->end_ms)
        return 0;
    uint32_t hits = 0;
    for(size_t i = 0; i < roll->run_count; i++)
        hits += run_at(&chart->hit_runs[roll->first_run + i], (int64_t)time_ms + 1);
    return roll->balloon_count - hits;
}
