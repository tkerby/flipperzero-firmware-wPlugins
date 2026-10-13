/* Host side of HubGitWriteIo (stdio / POSIX). */
#include "../../flipper_app/hub_gitwrite.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static bool h_exists(void* ctx, const char* path) {
    (void)ctx;
    struct stat st;
    return stat(path, &st) == 0;
}

static bool h_mkdir(void* ctx, const char* path) {
    (void)ctx;
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static void* h_open(void* ctx, const char* path) {
    (void)ctx;
    return fopen(path, "wb");
}

static bool h_write(void* ctx, void* f, const void* buf, size_t len) {
    (void)ctx;
    return fwrite(buf, 1, len, (FILE*)f) == len;
}

static bool h_close(void* ctx, void* f) {
    (void)ctx;
    return fclose((FILE*)f) == 0;
}

static bool h_rename(void* ctx, const char* from, const char* to) {
    (void)ctx;
    return rename(from, to) == 0;
}

static void h_remove(void* ctx, const char* path) {
    (void)ctx;
    unlink(path);
}

void hub_gitwrite_io_stdio(HubGitWriteIo* io) {
    io->ctx = NULL;
    io->exists = h_exists;
    io->mkdir = h_mkdir;
    io->open = h_open;
    io->write = h_write;
    io->close = h_close;
    io->rename = h_rename;
    io->remove = h_remove;
}
