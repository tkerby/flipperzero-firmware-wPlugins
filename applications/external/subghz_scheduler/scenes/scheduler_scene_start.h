#pragma once
#include "src/scheduler_app_i.h"
#include "scheduler_scene_loadfile.h"

typedef uint8_t (*SchedulerGetIdxFn)(const Scheduler* scheduler);
typedef void (*SchedulerSetIdxFn)(Scheduler* scheduler, uint8_t idx);

inline uint8_t clamp_u8(uint8_t v, uint8_t max_exclusive) {
    return (max_exclusive == 0) ? 0 : (v < max_exclusive ? v : 0);
}

static uint8_t get_interval_idx(const Scheduler* s) {
    return scheduler_get_interval((Scheduler*)s);
}
static void set_interval_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_interval(s, idx);
}

static uint8_t get_timing_idx(const Scheduler* s) {
    return scheduler_get_timing_mode((Scheduler*)s);
}
static void set_timing_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_timing_mode(s, idx);
}

static uint8_t get_repeats_idx(const Scheduler* s) {
    return scheduler_get_tx_repeats((Scheduler*)s);
}
static void set_repeats_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_tx_repeats(s, idx);
}

static uint8_t get_mode_idx(const Scheduler* s) {
    return (uint8_t)scheduler_get_mode((Scheduler*)s);
}
static void set_mode_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_mode(s, (SchedulerTxMode)idx);
}

static uint8_t get_tx_delay_idx(const Scheduler* s) {
    return scheduler_get_tx_delay_index((Scheduler*)s);
}
static void set_tx_delay_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_tx_delay(s, idx);
}

static uint8_t get_radio_idx(const Scheduler* s) {
    return scheduler_get_radio((Scheduler*)s);
}
static void set_radio_idx(Scheduler* s, uint8_t idx) {
    scheduler_set_radio(s, idx);
}