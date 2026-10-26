#pragma once

#include "include/domain/wc_marauder_parse.h" // WC_LINE_MAX

#include <stddef.h>
#include <stdint.h>

// Called with one complete line (newline stripped), valid only for the call.
typedef void (*WcLineFn)(void *ctx, const char *line, size_t len);

// Reassembles a byte stream (as it arrives in arbitrary chunks from the UART) into complete
// lines. Pure and host-testable: the furi serial adapter just feeds it received bytes. A
// line longer than WC_LINE_MAX is dropped whole (marked overflow until the next newline) so
// a flood of bytes without a newline can never overrun the buffer.
typedef struct {
    char buf[WC_LINE_MAX];
    size_t len;
    bool overflow;
} WcLineAsm;

void wc_lineasm_init(WcLineAsm *a);

// Feed `n` bytes; for each newline-terminated line, invoke `cb`. Carriage returns are
// stripped; empty lines are skipped.
void wc_lineasm_feed(WcLineAsm *a, const uint8_t *data, size_t n, WcLineFn cb, void *ctx);
