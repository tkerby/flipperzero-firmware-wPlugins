#include "include/application/wc_merge_service.h"

#include "include/application/wc_capture_service.h"
#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"

#include <stdlib.h>
#include <string.h>

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}
static uint32_t max_u32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

static bool merge_one(void *ctx, const WcSignature *d) {
    wc_census_merge_one(ctx, d); // the ceiling is reported through dropped, not refused here
    return true;
}

bool wc_merge_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                          const char *out_basename, uint16_t *out_devices, uint16_t *out_dropped) {
    WcCensus *a = malloc(sizeof(WcCensus));
    bool ok = false;
    if (a) {
        wc_census_init(a);
        WcCaptureMeta ma, mb;
        bool loaded = wc_capture_service_load(store, name_a, &ma, a);
        if (loaded) {
            // Room for both up front: growing by reallocs needs the array twice over mid-copy.
            const uint16_t incoming = wc_capture_service_device_count(store, name_b);
            wc_census_reserve(a, (uint16_t)(a->count + incoming));
        }
        if (loaded && wc_capture_service_stream(store, name_b, &mb, a->max, merge_one, a)) {

            WcCaptureMeta out;
            memset(&out, 0, sizeof(out));
            strncpy(out.label, out_basename, WC_LABEL_MAX);
            out.epoch = min_u32(ma.epoch, mb.epoch);
            uint32_t end = max_u32(ma.epoch + ma.duration_s, mb.epoch + mb.duration_s);
            out.duration_s = (end > out.epoch) ? (end - out.epoch) : 0;
            out.channels_mask = ma.channels_mask | mb.channels_mask;
            out.mode = 0;
            ok = wc_capture_service_save(store, out_basename, &out, a);
            if (ok && out_devices) {
                *out_devices = a->count;
            }
            if (ok && out_dropped) {
                *out_dropped = a->dropped;
            }
        }
    }
    if (a) {
        wc_census_free(a);
    }
    free(a);
    return ok;
}
