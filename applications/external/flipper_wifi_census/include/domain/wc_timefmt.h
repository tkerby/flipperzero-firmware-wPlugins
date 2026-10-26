#pragma once

#include <stddef.h>
#include <stdint.h>

// Build a default, filesystem-safe name from a Unix epoch (UTC): "<prefix>_YYYYMMDD_HHMM"
// (no separators a FAT filename dislikes). Writes into `out`. Pure and host-testable.
void wc_default_name(uint32_t epoch, const char *prefix, char *out, size_t cap);

// Convenience: a capture name, prefix "cap".
void wc_default_capture_name(uint32_t epoch, char *out, size_t cap);
