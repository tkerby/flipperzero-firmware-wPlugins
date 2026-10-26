#include "include/application/wc_capture_service.h"

#include "include/application/wc_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool wc_capture_service_save(const WcStorePort *store, const char *basename,
                             const WcCaptureMeta *meta, const WcCensus *c) {
    if (basename == NULL || strlen(basename) == 0 || strlen(basename) > WC_BASENAME_MAX) {
        return false;
    }
    char name[WC_BASENAME_MAX + 8];

    // Binary capture, streamed record-by-record so a big census never needs a whole-file
    // buffer (which would double its RAM and OOM the FAP).
    snprintf(name, sizeof(name), "%s%s", basename, WC_CAP_EXT);
    WcFileWriter *w = store->open_write(store->self, name);
    if (!w) {
        return false;
    }
    uint8_t rec[256]; // >= header size and record size
    bool ok = true;
    wc_capture_put_header(rec, meta, c->count);
    ok = store->write(w, rec, wc_capture_header_size());
    for (uint16_t i = 0; ok && i < c->count; i++) {
        wc_capture_put_record(rec, &c->devices[i]);
        ok = store->write(w, rec, wc_capture_record_size());
    }
    ok = store->close(w) && ok;
    if (!ok) {
        return false;
    }

    // CSV sidecar, streamed row-by-row.
    snprintf(name, sizeof(name), "%s%s", basename, WC_CSV_EXT);
    w = store->open_write(store->self, name);
    if (!w) {
        return false;
    }
    char row[512];
    wc_capture_csv_header(row, sizeof(row));
    ok = store->write(w, (const uint8_t *)row, strlen(row));
    for (uint16_t i = 0; ok && i < c->count; i++) {
        wc_capture_csv_row(row, sizeof(row), &c->devices[i]);
        ok = store->write(w, (const uint8_t *)row, strlen(row));
    }
    ok = store->close(w) && ok;
    return ok;
}

bool wc_capture_service_exists(const WcStorePort *store, const char *basename) {
    char name[WC_BASENAME_MAX + 8];
    snprintf(name, sizeof(name), "%s%s", basename, WC_CAP_EXT);
    return store->file_size(store->self, name) > 0;
}

uint16_t wc_capture_service_device_count(const WcStorePort *store, const char *filename) {
    uint8_t head[64];
    const size_t hdr = wc_capture_header_size();
    // The header alone, never the file: this is asked precisely about captures too big to hold.
    if (hdr > sizeof(head) || store->read_range(store->self, filename, 0, head, hdr) != hdr) {
        return 0;
    }
    return wc_capture_peek_count(head, hdr);
}

bool wc_capture_service_stream(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                               uint16_t max_devices, WcCaptureDeviceFn on_device, void *ctx) {
    uint8_t head[64];
    const size_t hdr = wc_capture_header_size();
    if (hdr > sizeof(head) || store->read_range(store->self, filename, 0, head, hdr) != hdr) {
        return false;
    }
    uint16_t count = 0, version = 0;
    if (!wc_capture_get_header(head, hdr, meta, &count, &version) || count > max_devices) {
        return false;
    }
    const size_t rec = wc_capture_record_size_of(version);
    // Records are read in blocks, not one by one: the store opens, seeks, reads and closes on
    // every call, and a 320-device capture is 320 of those against the SD where a handful do.
    uint8_t buf[1024];
    if (rec > sizeof(buf)) {
        return false;
    }
    const uint16_t per_block = (uint16_t)(sizeof(buf) / rec);
    // The declared count must account for the file exactly, as the buffered reader also checks.
    if (store->file_size(store->self, filename) != hdr + (size_t)count * rec) {
        return false;
    }
    for (uint16_t i = 0; i < count;) {
        const uint16_t want = (count - i < per_block) ? (uint16_t)(count - i) : per_block;
        const size_t n = (size_t)want * rec;
        if (store->read_range(store->self, filename, hdr + (size_t)i * rec, buf, n) != n) {
            return false;
        }
        for (uint16_t j = 0; j < want; j++, i++) {
            WcSignature d;
            if (!wc_capture_get_record(buf + (size_t)j * rec, rec, version, &d) ||
                !on_device(ctx, &d)) {
                return false;
            }
        }
    }
    return true;
}

static bool load_one(void *ctx, const WcSignature *d) {
    return wc_census_add(ctx, d) != NULL;
}

// Streamed, not buffered: holding a capture whole alongside the census it fills is two large
// contiguous blocks at once, and a failed allocation reboots this hardware.
bool wc_capture_service_load(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                             WcCensus *c) {
    const uint16_t ceiling = c->max;
    wc_census_free(c);
    wc_census_set_max(c, ceiling);
    // One allocation for the whole capture: growing by reallocs needs the array twice over
    // while it copies, which is what exhausted the heap before.
    wc_census_reserve(c, wc_capture_service_device_count(store, filename));
    if (!wc_capture_service_stream(store, filename, meta, ceiling, load_one, c)) {
        wc_census_free(c);
        wc_census_set_max(c, ceiling);
        return false;
    }
    return true;
}
