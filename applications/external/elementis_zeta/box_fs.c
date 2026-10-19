// The in-place file helpers of the PC box (box.c, contract 25): device only, PC plugin only (they were part of the resident assets.c; M5 memory recovery,
// contract 25.6). The host harness has its own in tools/host/shim_assets.c, so this file is NOT in the host Makefiles. ABSOLUTE paths.
#include "assets.h"
#include <furi.h>
#include <storage/storage.h>

bool write_at(const char* path, uint32_t off, const void* buf, uint32_t len) {
    if(path[0] != '/') return false;
    Storage* st = furi_record_open(RECORD_STORAGE);
    bool ok = false;
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_OPEN_ALWAYS)) { // open or create, never truncate
        ok = storage_file_seek(f, off, true) && storage_file_write(f, buf, len) == len;
        storage_file_close(f); // (flushes)
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool rename_file(const char* from, const char* to) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(st, to); // rename does not overwrite
    bool ok = storage_common_rename(st, from, to) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool remove_file(const char* path) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_remove(st, path);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
