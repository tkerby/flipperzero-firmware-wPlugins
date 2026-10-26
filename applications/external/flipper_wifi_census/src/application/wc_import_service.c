#include "include/application/wc_import_service.h"

#include "include/application/wc_capture_service.h"
#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"
#include "include/domain/wc_pcap_reader.h"
#include "include/domain/wc_probe_frame.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    WcCensus *census;
    uint32_t now;
} ImportCtx;

typedef struct {
    const WcStorePort *store;
    const char *path;
} PullCtx;

static size_t pull_from_store(const void *ctx, size_t offset, uint8_t *buf, size_t cap) {
    const PullCtx *p = ctx;
    return p->store->read_range_path(p->store->self, p->path, offset, buf, cap);
}

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        ImportCtx *ic = ctx;
        wc_census_observe(ic->census, &o, ic->now);
    }
}

bool wc_import_service_run(const WcStorePort *store, WcClockPort clock, const char *pcap_path,
                           const char *out_basename, uint16_t *out_dropped) {
    // Streamed, never held whole: reading the file into one allocation its own size rebooted
    // the Flipper on a 37 KB capture.
    if (store->file_size_path(store->self, pcap_path) == 0) {
        return false;
    }
    WcCensus *census = malloc(sizeof(WcCensus));
    if (!census) {
        return false;
    }
    wc_census_init(census);
    ImportCtx ic = {.census = census, .now = wc_clock_now(&clock)};
    PullCtx pc = {.store = store, .path = pcap_path};
    bool ok = false;
    if (wc_pcap_stream(pull_from_store, &pc, on_frame, &ic)) {
        WcCaptureMeta meta;
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.label, out_basename, WC_LABEL_MAX);
        meta.epoch = ic.now;
        ok = wc_capture_service_save(store, out_basename, &meta, census);
        if (ok && out_dropped) {
            *out_dropped = census->dropped;
        }
    }
    wc_census_free(census);
    free(census);
    return ok;
}
