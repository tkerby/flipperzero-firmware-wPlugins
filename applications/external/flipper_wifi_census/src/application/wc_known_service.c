#include "include/application/wc_known_service.h"

#include "include/application/wc_files.h"

#include <stdlib.h>
#include <string.h>

// Max on-disk size of the registry: header + WC_KNOWN_MAX items. Bounds the read buffer.
#define WC_KNOWN_MAX_BYTES (8 + (size_t)WC_KNOWN_MAX * 65)

bool wc_known_service_load(const WcStorePort *store, WcKnownDb *db) {
    wc_known_init(db);
    // Size first. read_file reports 0 for a missing file AND for a failed read, so reading
    // alone files a registry that would not load under "never saved" - and the next mark then
    // writes one rule over all of them.
    size_t size = store->file_size(store->self, WC_KNOWN_FILE);
    if (size == 0) {
        return true; // never saved yet
    }
    // Heap, not stack: WC_KNOWN_MAX_BYTES (~4 KB) would blow the FAP's 4 KB stack.
    uint8_t *buf = malloc(WC_KNOWN_MAX_BYTES);
    if (!buf) {
        return false;
    }
    size_t n = store->read_file(store->self, WC_KNOWN_FILE, buf, WC_KNOWN_MAX_BYTES);
    bool ok = (n == size) && wc_known_read(db, buf, n);
    free(buf);
    if (!ok) {
        wc_known_init(db);
    }
    return ok;
}

bool wc_known_service_save(const WcStorePort *store, const WcKnownDb *db) {
    uint8_t *buf = malloc(WC_KNOWN_MAX_BYTES);
    if (!buf) {
        return false;
    }
    size_t n = wc_known_write(buf, WC_KNOWN_MAX_BYTES, db);
    bool ok = (n > 0) && store->write_file(store->self, WC_KNOWN_FILE, buf, n);
    free(buf);
    return ok;
}

bool wc_known_service_mark(const WcStorePort *store, const WcSignature *sig, const char *label) {
    WcKnownDb *db = malloc(sizeof(WcKnownDb));
    if (!db) {
        return false;
    }
    bool ok = false;
    if (wc_known_service_load(store, db)) {
        WcKnown rule;
        if (wc_known_rule_from_signature(&rule, sig, label) && wc_known_add(db, &rule)) {
            ok = wc_known_service_save(store, db);
        }
    }
    free(db);
    return ok;
}

bool wc_known_service_mark_ssid(const WcStorePort *store, const char *ssid, const char *label) {
    WcKnownDb *db = malloc(sizeof(WcKnownDb));
    if (!db) {
        return false;
    }
    bool ok = false;
    if (wc_known_service_load(store, db)) {
        WcKnown rule;
        if (wc_known_rule_ssid(&rule, ssid, label) && wc_known_add(db, &rule)) {
            ok = wc_known_service_save(store, db);
        }
    }
    free(db);
    return ok;
}

bool wc_known_service_remove(const WcStorePort *store, uint16_t index) {
    WcKnownDb *db = malloc(sizeof(WcKnownDb));
    if (!db) {
        return false;
    }
    bool ok = false;
    if (wc_known_service_load(store, db) && wc_known_remove(db, index)) {
        ok = wc_known_service_save(store, db);
    }
    free(db);
    return ok;
}

bool wc_known_service_rename(const WcStorePort *store, uint16_t index, const char *label) {
    if (!label || label[0] == '\0') {
        return false;
    }
    WcKnownDb *db = malloc(sizeof(WcKnownDb));
    if (!db) {
        return false;
    }
    bool ok = false;
    if (wc_known_service_load(store, db) && index < db->count) {
        strncpy(db->items[index].label, label, WC_KNOWN_LABEL_MAX);
        db->items[index].label[WC_KNOWN_LABEL_MAX] = '\0';
        ok = wc_known_service_save(store, db);
    }
    free(db);
    return ok;
}
