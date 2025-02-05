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

#define TX_DELAY_COUNT 4
typedef enum SchedulerTxDelay {
    SchedulerTxDelay100 = 100,
    SchedulerTxDelay250 = 250,
    SchedulerTxDelay500 = 500,
    SchedulerTxDelay1000 = 1000
} SchedulerTxDelay;
static const char* const tx_delay_text[TX_DELAY_COUNT] = {"100ms", "250ms", "500ms", "1000ms"};
static const uint16_t tx_delay_value[TX_DELAY_COUNT] =
    {SchedulerTxDelay100, SchedulerTxDelay250, SchedulerTxDelay500, SchedulerTxDelay1000};

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

static const uint32_t interval_second_value[] =
    {10, 30, 60, 120, 300, 600, 1200, 1800, 2700, 3600, 7200, 14400, 28800, 43200};

#define REPEATS_COUNT 6
static const char* const tx_repeats_text[REPEATS_COUNT] = {
    "x1",
    "x2",
    "x3",
    "x4",
    "x5",
    "x6",
};
