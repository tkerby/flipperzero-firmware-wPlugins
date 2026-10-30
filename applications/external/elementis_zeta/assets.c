#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-ira-hoist-pressure,no-tree-dce") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "assets.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void heap_stats_now(HeapStats* out) {
    out->free = (uint32_t)memmgr_get_free_heap();
    out->max_blk = (uint32_t)memmgr_heap_get_max_free_block();
    out->min_free = (uint32_t)memmgr_get_minimum_free_heap();
}

uint32_t asset_tick(void) {
    return furi_get_tick();
}

bool heap_can_alloc(uint32_t bytes) {
    return (uint32_t)memmgr_heap_get_max_free_block() >= bytes + 1024u;
}

int heap_format(const HeapStats* h, char* out, size_t cap) {
    unsigned f = h->free * 10u / 1024u, m = h->max_blk * 10u / 1024u,
             n = h->min_free * 10u / 1024u;
    return snprintf(
        out, cap, "F%u.%u M%u.%u m%u.%u", f / 10, f % 10, m / 10, m % 10, n / 10, n % 10);
}

static void resolve(const char* path, char* out, size_t cap) {
    if(path[0] == '/')
        snprintf(out, cap, "%s", path);
    else
        snprintf(out, cap, "%s/%s", STORAGE_APP_ASSETS_PATH_PREFIX, path);
}

static File* open_read(Storage* st, const char* path) {
    char full[96];
    resolve(path, full, sizeof(full));
    File* f = storage_file_alloc(st);
    if(!storage_file_open(f, full, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return NULL;
    }
    return f;
}

bool read_at(const char* path, uint32_t offset, void* buf, uint32_t len) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    bool ok = false;
    File* f = open_read(st, path);
    if(f) {
        if(storage_file_seek(f, offset, true)) ok = storage_file_read(f, buf, len) == len;
        storage_file_close(f);
        storage_file_free(f);
    }
    furi_record_close(RECORD_STORAGE);
    return ok;
}

int32_t file_size(const char* path) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    int32_t sz = -1;
    File* f = open_read(st, path);
    if(f) {
        sz = (int32_t)storage_file_size(f);
        storage_file_close(f);
        storage_file_free(f);
    }
    furi_record_close(RECORD_STORAGE);
    return sz;
}

struct AssetFile {
    Storage* st;
    File* f;
};

// Every AssetFile still open, so assets_shutdown() can close what a code path forgot (game thread only).
#define ASSET_MAX_OPEN 4
static AssetFile* open_list[ASSET_MAX_OPEN];

// NOTE storage_file_open() never fails with FSE_ALREADY_OPEN: when the path is in Storage's open
// list it WAITS FOREVER until some file is closed (storage_external_api.c), and Storage never
// cleans up after a dead app. So callers keep a file open only for the duration of one call and
// assets_shutdown() runs on exit. KNOWN LIMITATION: a handle leaked by an OLDER build (or a crash)
// stays open until the Flipper reboots, and opening that file again hangs until then.
AssetFile* asset_open(const char* path) {
    char full[96];
    resolve(path, full, sizeof(full));
    int slot = -1;
    for(int i = 0; i < ASSET_MAX_OPEN; i++)
        if(!open_list[i]) {
            slot = i;
            break;
        }
    if(slot < 0) return NULL; // a leak in our own code: refuse instead of piling up handles
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    AssetFile* a = NULL;
    if(storage_file_open(f, full, FSAM_READ, FSOM_OPEN_EXISTING)) a = malloc(sizeof(AssetFile));
    if(!a) {
        if(storage_file_is_open(f)) storage_file_close(f);
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    a->st = st;
    a->f = f;
    open_list[slot] = a;
    return a;
}

bool asset_read(AssetFile* a, uint32_t offset, void* buf, uint32_t len) {
    if(!a || !storage_file_seek(a->f, offset, true)) return false;
    return storage_file_read(a->f, buf, len) == len;
}

void asset_close(AssetFile* a) {
    if(!a) return;
    for(int i = 0; i < ASSET_MAX_OPEN; i++)
        if(open_list[i] == a) open_list[i] = NULL;
    storage_file_close(a->f);
    storage_file_free(a->f);
    furi_record_close(RECORD_STORAGE);
    free(a);
}

void assets_shutdown(void) {
    for(int i = 0; i < ASSET_MAX_OPEN; i++)
        if(open_list[i]) asset_close(open_list[i]);
}
