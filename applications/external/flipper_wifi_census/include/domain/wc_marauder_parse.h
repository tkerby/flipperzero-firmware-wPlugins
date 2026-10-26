#pragma once

#include "include/domain/wc_observation.h"

#include <stddef.h>

#define WC_LINE_MAX 160 // longest serial line we will parse; longer input is safely truncated

// Parse one line of ESP32 Marauder (v1.17.0) probe-sniff serial output into an observation.
// The input is untrusted device output, so the line is copied into a bounded local buffer
// and every scan stays within it. Tolerant by design: a source MAC is required (the return
// value); RSSI, channel and a directed SSID are filled when present and left at their zero
// defaults otherwise. `len` is the number of bytes at `line` to consider (need not be
// NUL-terminated). Returns true when a MAC was found and `out` is populated.
bool wc_parse_summary_line(const char *line, size_t len, WcObservation *out);
