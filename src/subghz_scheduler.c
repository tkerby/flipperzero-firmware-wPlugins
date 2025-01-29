#include "subghz_scheduler.h"
#include <furi_hal.h>

#define TAG "SubGHzScheduler"

struct Scheduler {
    uint32_t previous_run_time;
    uint32_t countdown;
    uint8_t interval;
    uint8_t tx_repeats;
    uint8_t tx_delay;
    char* file_name;
    FileTxType file_type;
};

static const uint32_t interval_second_value[] =
    {10, 30, 60, 120, 300, 600, 1200, 1800, 2700, 3600, 7200, 14400, 28800, 43200};

Scheduler* scheduler_alloc() {
    Scheduler* scheduler = malloc(sizeof(Scheduler));
    return scheduler;
}

void scheduler_free(Scheduler* scheduler) {
    furi_assert(scheduler);
    free(scheduler);
}

void scheduler_set_interval(Scheduler* scheduler, uint8_t interval) {
    furi_assert(scheduler);
    scheduler->interval = interval;
    scheduler->countdown = interval_second_value[scheduler->interval];
}

void scheduler_set_tx_repeats(Scheduler* scheduler, uint8_t tx_repeats) {
    furi_assert(scheduler);
    scheduler->tx_repeats = tx_repeats;
}

static const char* extract_filename(const char* filepath) {
    const char* filename = strrchr(filepath, '/');
    return (filename != NULL) ? (filename + 1) : filepath;
}

void scheduler_set_file_name(Scheduler* scheduler, const char* file_name) {
    furi_assert(scheduler);
    const char* name = extract_filename(file_name);
    scheduler->file_name = (char*)name;
}

void scheduler_set_file_type(Scheduler* scheduler, FileTxType file_type) {
    furi_assert(scheduler);
    scheduler->file_type = file_type;
}

bool scheduler_time_to_trigger(Scheduler* scheduler) {
    furi_assert(scheduler);
    uint32_t current_time = furi_hal_rtc_get_timestamp();
    uint32_t interval = interval_second_value[scheduler->interval];
    if((current_time - scheduler->previous_run_time) >= interval) {
        scheduler->countdown = interval;
        scheduler->previous_run_time = furi_hal_rtc_get_timestamp();
        return true;
    }
    scheduler->countdown--;
    return false;
}

void scheduler_get_countdown_fmt(Scheduler* scheduler, char* buffer, uint8_t size) {
    furi_assert(scheduler);
    snprintf(
        buffer,
        size,
        "%02lu:%02lu:%02lu",
        scheduler->countdown / 60 / 60,
        scheduler->countdown / 60 % 60,
        scheduler->countdown % 60);
}

uint32_t scheduler_get_previous_time(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->previous_run_time;
}

uint8_t scheduler_get_interval(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->interval;
}

uint8_t scheduler_get_tx_repeats(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->tx_repeats;
}

const char* scheduler_get_file_name(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_name;
}

FileTxType scheduler_get_file_type(Scheduler* scheduler) {
    furi_assert(scheduler);
    return scheduler->file_type;
}

void scheduler_reset(Scheduler* scheduler) {
    scheduler->previous_run_time = 0;
    scheduler->countdown = 0;
}
