#include "hub_gitwrite.h"

#include <storage/storage.h>
#include "hub_mem.h"

/* Flipper side of HubGitWriteIo (Storage API). */

static bool wio_exists(void* ctx, const char* path) {
    return storage_common_stat((Storage*)ctx, path, NULL) == FSE_OK;
}

static bool wio_mkdir(void* ctx, const char* path) {
    return storage_simply_mkdir((Storage*)ctx, path);
}

static void* wio_open(void* ctx, const char* path) {
    File* f = storage_file_alloc((Storage*)ctx);
    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return NULL;
    }
    return f;
}

static bool wio_write(void* ctx, void* file, const void* buf, size_t len) {
    (void)ctx;
    return storage_file_write((File*)file, buf, len) == len;
}

static bool wio_close(void* ctx, void* file) {
    (void)ctx;
    bool ok = storage_file_sync((File*)file);
    storage_file_close((File*)file);
    storage_file_free((File*)file);
    return ok;
}

static bool wio_rename(void* ctx, const char* from, const char* to) {
    return storage_common_rename((Storage*)ctx, from, to) == FSE_OK;
}

static void wio_remove(void* ctx, const char* path) {
    storage_common_remove((Storage*)ctx, path);
}

/* io->ctx is the Storage*. The struct can live on the stack of the caller. */
void hub_gitwrite_io_furi(HubGitWriteIo* io, void* storage) {
    io->ctx = storage;
    io->exists = wio_exists;
    io->mkdir = wio_mkdir;
    io->open = wio_open;
    io->write = wio_write;
    io->close = wio_close;
    io->rename = wio_rename;
    io->remove = wio_remove;
}
