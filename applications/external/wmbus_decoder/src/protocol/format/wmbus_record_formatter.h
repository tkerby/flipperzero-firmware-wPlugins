#pragma once

#include <furi.h>

#include "../model/wmbus_application_types.h"

#define WMBUS_RECORD_VALUE_TEXT_MAX 32U

/**
 * Format one application record value without allocating.
 * Returns false when the value has no printable representation.
 */
bool wmbus_record_formatter_format_value_text(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size);

/**
 * Format multiple application records separated by `delimiter`, grouped by
 * ascending storage number while retaining wire order within each group.
 * Resets `out`. Empty output is valid when no record produces text.
 * The caller owns `out`.
 */
bool wmbus_record_formatter_format_joined(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    char delimiter,
    FuriString* out);
