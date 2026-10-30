#pragma once

/* Pure logic, deliberately free of any Furi/Flipper SDK dependency, so it
 * can be compiled and unit-tested on a normal host (see tests/). This is
 * NOT a substitute for on-device testing -- it only proves the math and
 * string formatting are correct, not GUI/radio/timing behavior. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Threshold = baseline + fixed margin above the calibrated noise floor. */
float bs_compute_threshold(float baseline_dbm, float margin_db);

/* An alert is active whenever RSSI rises above the calibrated threshold. */
bool bs_should_alert(float rssi_dbm, float threshold_dbm);

/* Wrap an index forward/backward within [0, count), count > 0. */
size_t bs_next_index(size_t index, size_t count);
size_t bs_prev_index(size_t index, size_t count);

/* Seconds elapsed between two RTC timestamps; clamps to 0 instead of
 * underflowing if end_ts is somehow earlier than start_ts (e.g. midnight
 * rollover edge case not yet handled upstream). */
uint32_t bs_alert_duration(uint32_t start_ts, uint32_t end_ts);

/* Format one CSV log line (no trailing content beyond the \n). Returns the
 * number of characters written, same convention as snprintf. */
int bs_format_log_line(
    char* buf,
    size_t buf_size,
    uint32_t timestamp,
    uint32_t freq_hz,
    int baseline_dbm,
    int threshold_dbm,
    int peak_rssi_dbm,
    uint32_t duration_s);
