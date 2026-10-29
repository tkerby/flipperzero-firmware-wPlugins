/* Host helpers: load files/movepools.bin into memory as a PkbBlobReader. */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pkb/team.h>

typedef struct {
    uint8_t* data;
    size_t len;
} HostBlob;

static bool host_blob_read(void* ctx, uint32_t offset, void* buf, uint32_t len) {
    HostBlob* b = ctx;
    if((size_t)offset + len > b->len) return false;
    memcpy(buf, b->data + offset, len);
    return true;
}

static bool host_blob_load(HostBlob* b, const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b->data = malloc((size_t)n);
    b->len = (size_t)n;
    bool ok = fread(b->data, 1, b->len, f) == b->len;
    fclose(f);
    return ok;
}

static PkbBlobReader host_movepools(HostBlob* storage) {
    if(!storage->data && !host_blob_load(storage, PKB_SOURCE_DIR "/files/movepools.bin")) {
        fprintf(stderr, "cannot read files/movepools.bin\n");
        exit(2);
    }
    PkbBlobReader r = {host_blob_read, storage};
    return r;
}
