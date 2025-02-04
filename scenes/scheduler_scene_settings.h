#pragma once

typedef enum SchedulerMode {
    SchedulerModeNormal,
    SchedulerModeImmediate,
    SchedulerModeOneShot,
    SchedulerModeSettingsNum,
} SchedulerMode;

static const char* const mode_text[SchedulerModeSettingsNum] = {
    "Normal",
    "Immed.",
    "1-Shot",
};

enum Intervals {
    Interval10Sec,
    Interval30Sec,
    Interval1Min,
    Interval2Min,
    Interval5Min,
    Interval10Min,
    Interval20Min,
    Interval30Min,
    Interval45Min,
    Interval1Hr,
    Interval2Hrs,
    Interval4Hrs,
    Interval8Hrs,
    Interval12Hrs,
    IntervalSettingsNum
};

#define INTERVAL_COUNT IntervalSettingsNum
static const char* const interval_text[INTERVAL_COUNT] = {
    "10 sec",
    "30 sec",
    "1 min",
    "2 min",
    "5 min",
    "10 min",
    "20 min",
    "30 min",
    "45 min",
    "1 hr",
    "2 hrs",
    "4 hrs",
    "8 hrs",
    "12 hrs"};

#define REPEATS_COUNT 6
static const char* const tx_repeats_text[REPEATS_COUNT] = {
    "  ",
    "x2",
    "x3",
    "x4",
    "x5",
    "x6",
};
