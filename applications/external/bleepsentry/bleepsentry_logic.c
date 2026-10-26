#include "bleepsentry_logic.h"

#include <stdio.h>

float bs_compute_threshold(float baseline_dbm, float margin_db) {
    return baseline_dbm + margin_db;
}

bool bs_should_alert(float rssi_dbm, float threshold_dbm) {
    return rssi_dbm > threshold_dbm;
}

size_t bs_next_index(size_t index, size_t count) {
    return (index + 1) % count;
}

size_t bs_prev_index(size_t index, size_t count) {
    return (index + count - 1) % count;
}

uint32_t bs_alert_duration(uint32_t start_ts, uint32_t end_ts) {
    return end_ts >= start_ts ? end_ts - start_ts : 0;
}

int bs_format_log_line(
    char* buf,
    size_t buf_size,
    uint32_t timestamp,
    uint32_t freq_hz,
    int baseline_dbm,
    int threshold_dbm,
    int peak_rssi_dbm,
    uint32_t duration_s) {
    return snprintf(
        buf,
        buf_size,
        "%lu,%lu,%d,%d,%d,%lu\n",
        (unsigned long)timestamp,
        (unsigned long)freq_hz,
        baseline_dbm,
        threshold_dbm,
        peak_rssi_dbm,
        (unsigned long)duration_s);
}
