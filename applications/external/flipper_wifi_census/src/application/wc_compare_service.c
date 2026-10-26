#include "include/application/wc_compare_service.h"

#include "include/application/wc_capture_service.h"
#include "include/domain/wc_capture_codec.h"

#include <stdlib.h>

bool wc_compare_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                            WcCompareResult *out) {
    WcCensus *a = malloc(sizeof(WcCensus));
    WcCensus *b = malloc(sizeof(WcCensus));
    bool ok = false;
    // Initialized under the same guard that frees them: one malloc succeeding while the other
    // fails is the realistic shape here, and free() would then run on uninitialized bytes.
    if (a) {
        wc_census_init(a);
    }
    if (b) {
        wc_census_init(b);
    }
    if (a && b) {
        WcCaptureMeta ma, mb;
        if (wc_capture_service_load(store, name_a, &ma, a) &&
            wc_capture_service_load(store, name_b, &mb, b)) {
            wc_compare(out, a, b);
            ok = true;
        }
    }
    if (a) {
        wc_census_free(a);
    }
    if (b) {
        wc_census_free(b);
    }
    free(a);
    free(b);
    return ok;
}
