#pragma once

#include "include/domain/wc_census.h"
#include "include/ports/wc_clock_port.h"

#include <stddef.h>

// Live scan session: turns Marauder serial lines into a deduplicated census. Bridges the
// serial port (which calls wc_scan_on_line per line) to the domain, stamping each device
// with the clock's time.
typedef struct {
    WcCensus census;
    WcClockPort clock;
} WcScanService;

void wc_scan_init(WcScanService *svc, WcClockPort clock);

// WcSerialLineFn-compatible: `ctx` is a WcScanService*. Parses the line and folds it in.
void wc_scan_on_line(void *ctx, const char *line, size_t len);

WcCensusStats wc_scan_stats(const WcScanService *svc);
