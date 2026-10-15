#pragma once

#include "tja_parser.h"

typedef struct {
    size_t note;
    size_t run;
    uint32_t repeat;
    uint32_t ordinal;
} TaikoHitCursor;

typedef struct {
    int32_t time_ms;
    bool rim;
    bool right;
} TaikoHit;

int32_t taiko_hit_run_time(const TjaHitRun* run, uint32_t index);
/* Seek is deterministic: rewinding also restores hand alternation and balloons. */
void taiko_hits_seek(const TjaChart* chart, TaikoHitCursor* cursor, int64_t time_ms);
bool taiko_hits_peek(const TjaChart* chart, const TaikoHitCursor* cursor, TaikoHit* hit);
void taiko_hits_advance(const TjaChart* chart, TaikoHitCursor* cursor);
uint32_t taiko_balloon_remaining(const TjaChart* chart, const TjaRoll* roll, uint32_t time_ms);
