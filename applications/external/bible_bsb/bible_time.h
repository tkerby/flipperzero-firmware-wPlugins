#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Write an ISO-8601 timestamp, YYYY-MM-DDTHH:MM:SSZ.
 *
 * Returns false when the buffer is shorter than 21 bytes or the clock fields
 * are out of range. On failure, out is left unchanged.
 */
bool bible_format_iso8601(
    char* out,
    size_t out_len,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second);
