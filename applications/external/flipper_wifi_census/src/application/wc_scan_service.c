#include "include/application/wc_scan_service.h"

#include "include/domain/wc_marauder_parse.h"

void wc_scan_init(WcScanService *svc, WcClockPort clock) {
    wc_census_init(&svc->census);
    svc->clock = clock;
}

void wc_scan_on_line(void *ctx, const char *line, size_t len) {
    WcScanService *svc = ctx;
    WcObservation obs;
    if (wc_parse_summary_line(line, len, &obs)) {
        wc_census_observe(&svc->census, &obs, wc_clock_now(&svc->clock));
    }
}

WcCensusStats wc_scan_stats(const WcScanService *svc) {
    return wc_census_stats(&svc->census);
}
