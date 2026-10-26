#include "include/platform/wc_store_furi.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdio.h>

#define WC_DIR STORAGE_APP_DATA_PATH_PREFIX // ".../apps_data/flipper_wifi_census"

// A bare file name must not contain path separators; reject any that does so a name can
// never escape the app's data directory.
static bool name_is_safe(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\') {
            return false;
        }
    }
    return true;
}

static void full_path(char *out, size_t cap, const char *name) {
    snprintf(out, cap, "%s/%s", WC_DIR, name);
}

static bool store_write(void *self, const char *name, const uint8_t *data, size_t len) {
    (void)self;
    if (!name_is_safe(name)) {
        return false;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, WC_DIR);
    char path[256];
    full_path(path, sizeof(path), name);
    File *file = storage_file_alloc(storage);
    bool ok = false;
    if (storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = (len == 0) || (storage_file_write(file, data, len) == len);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static size_t store_read(void *self, const char *name, uint8_t *buf, size_t cap) {
    (void)self;
    if (!name_is_safe(name)) {
        return 0;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    char path[256];
    full_path(path, sizeof(path), name);
    File *file = storage_file_alloc(storage);
    size_t read = 0;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if (size <= cap) {
            read = storage_file_read(file, buf, (size_t)size);
            if (read != (size_t)size) {
                read = 0; // short read -> treat as failure
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return read;
}

static size_t store_file_size(void *self, const char *name) {
    (void)self;
    if (!name_is_safe(name)) {
        return 0;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    char path[256];
    full_path(path, sizeof(path), name);
    File *file = storage_file_alloc(storage);
    size_t size = 0;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size = (size_t)storage_file_size(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return size;
}

// Shared by both range readers: open, seek, read a slice. `verbatim` paths come from the file
// browser; named files live under the app dir.
static size_t store_read_range_at(const char *path, size_t offset, uint8_t *buf, size_t cap) {
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    size_t read = 0;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if (storage_file_seek(file, (uint32_t)offset, true)) {
            read = storage_file_read(file, buf, cap);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return read;
}

static size_t store_read_range_path(void *self, const char *path, size_t offset, uint8_t *buf,
                                    size_t cap) {
    (void)self;
    return store_read_range_at(path, offset, buf, cap);
}

static size_t store_read_range(void *self, const char *name, size_t offset, uint8_t *buf,
                               size_t cap) {
    (void)self;
    if (!name_is_safe(name)) {
        return 0;
    }
    char path[256];
    full_path(path, sizeof(path), name);
    return store_read_range_at(path, offset, buf, cap);
}

static size_t store_file_size_path(void *self, const char *path) {
    (void)self;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);
    size_t size = 0;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size = (size_t)storage_file_size(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return size;
}

static bool store_rename(void *self, const char *from, const char *to) {
    (void)self;
    if (!name_is_safe(from) || !name_is_safe(to)) {
        return false;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    char pf[256], pt[256];
    full_path(pf, sizeof(pf), from);
    full_path(pt, sizeof(pt), to);
    bool ok = (storage_common_rename(storage, pf, pt) == FSE_OK);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool store_delete(void *self, const char *name) {
    (void)self;
    if (!name_is_safe(name)) {
        return false;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    char path[256];
    full_path(path, sizeof(path), name);
    bool ok = (storage_common_remove(storage, path) == FSE_OK);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

struct WcFileWriter {
    Storage *storage;
    File *file;
};

static WcFileWriter *store_open_write(void *self, const char *name) {
    (void)self;
    if (!name_is_safe(name)) {
        return NULL;
    }
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, WC_DIR);
    char path[256];
    full_path(path, sizeof(path), name);
    File *file = storage_file_alloc(storage);
    if (!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    WcFileWriter *w = malloc(sizeof(WcFileWriter));
    if (!w) {
        // Every caller already handles NULL here, and leaking the open file and the record
        // would be worse than the refusal.
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    w->storage = storage;
    w->file = file;
    return w;
}

static bool store_write_chunk(WcFileWriter *w, const uint8_t *data, size_t len) {
    return storage_file_write(w->file, data, len) == len;
}

static bool store_close_write(WcFileWriter *w) {
    // The port documents this as "whether the file closed cleanly". Returning true regardless
    // let a capture whose tail never reached the card be reported as saved - and the scan was
    // freed on the next line.
    const bool closed = storage_file_close(w->file);
    storage_file_free(w->file);
    furi_record_close(RECORD_STORAGE);
    free(w);
    return closed;
}

static uint16_t store_list(void *self, WcStoreNameFn cb, void *ctx) {
    (void)self;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *dir = storage_file_alloc(storage);
    uint16_t count = 0;
    if (storage_dir_open(dir, WC_DIR)) {
        FileInfo info;
        char name[128];
        while (storage_dir_read(dir, &info, name, sizeof(name))) {
            if (!(info.flags & FSF_DIRECTORY)) {
                cb(ctx, name);
                count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return count;
}

WcStorePort wc_store_furi_port(void) {
    WcStorePort p = {
        .self = NULL,
        .write_file = store_write,
        .read_file = store_read,
        .file_size = store_file_size,
        .file_size_path = store_file_size_path,
        .read_range = store_read_range,
        .read_range_path = store_read_range_path,
        .rename_file = store_rename,
        .delete_file = store_delete,
        .list = store_list,
        .open_write = store_open_write,
        .write = store_write_chunk,
        .close = store_close_write,
    };
    return p;
}
