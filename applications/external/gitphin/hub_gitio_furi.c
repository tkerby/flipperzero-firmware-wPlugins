#include "hub_gitio.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

/* Flipper implementation of hub_gitio.h. io is a Storage*. */

struct HubGitFile {
    File* f;
};

struct HubGitDir {
    File* f;
};

HubGitFile* hub_gitio_open(void* io, const char* path) {
    HubGitFile* h = malloc(sizeof(HubGitFile));
    if(!h) return NULL;
    h->f = storage_file_alloc((Storage*)io);
    if(!h->f) {
        free(h);
        return NULL;
    }
    if(!storage_file_open(h->f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* close is required even after a failed open */
        storage_file_close(h->f);
        storage_file_free(h->f);
        free(h);
        return NULL;
    }
    return h;
}

void hub_gitio_close(HubGitFile* h) {
    if(!h) return;
    storage_file_close(h->f);
    storage_file_free(h->f);
    free(h);
}

uint64_t hub_gitio_size(HubGitFile* h) {
    return storage_file_size(h->f);
}

bool hub_gitio_seek(HubGitFile* h, uint64_t offset) {
    if(offset > 0xFFFFFFFFull) return false; /* the SDK takes a uint32_t */
    return storage_file_seek(h->f, (uint32_t)offset, true);
}

size_t hub_gitio_read(HubGitFile* h, void* buf, size_t len) {
    return storage_file_read(h->f, buf, len);
}

HubGitDir* hub_gitio_dir_open(void* io, const char* path) {
    HubGitDir* d = malloc(sizeof(HubGitDir));
    if(!d) return NULL;
    d->f = storage_file_alloc((Storage*)io);
    if(!d->f) {
        free(d);
        return NULL;
    }
    if(!storage_dir_open(d->f, path)) {
        storage_dir_close(d->f);
        storage_file_free(d->f);
        free(d);
        return NULL;
    }
    return d;
}

bool hub_gitio_dir_read(HubGitDir* d, char* name, size_t cap, bool* is_dir) {
    return hub_gitio_dir_read_ex(d, name, cap, is_dir, NULL);
}

bool hub_gitio_dir_read_ex(HubGitDir* d, char* name, size_t cap, bool* is_dir, uint64_t* size) {
    FileInfo info;
    for(;;) {
        if(cap > 0xFFFF) cap = 0xFFFF;
        if(!storage_dir_read(d->f, &info, name, (uint16_t)cap)) return false;
        if(name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        if(is_dir) *is_dir = file_info_is_dir(&info);
        if(size) *size = info.size;
        return true;
    }
}

void hub_gitio_dir_close(HubGitDir* d) {
    if(!d) return;
    storage_dir_close(d->f);
    storage_file_free(d->f);
    free(d);
}

HubGitFile* hub_gitio_create(void* io, const char* path) {
    HubGitFile* h = malloc(sizeof(HubGitFile));
    if(!h) return NULL;
    h->f = storage_file_alloc((Storage*)io);
    if(!h->f) {
        free(h);
        return NULL;
    }
    if(!storage_file_open(h->f, path, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(h->f);
        storage_file_free(h->f);
        free(h);
        return NULL;
    }
    return h;
}

size_t hub_gitio_write(HubGitFile* h, const void* buf, size_t len) {
    return storage_file_write(h->f, buf, len);
}

bool hub_gitio_mkdir(void* io, const char* path) {
    FS_Error e = storage_common_mkdir((Storage*)io, path);
    return e == FSE_OK || e == FSE_EXIST;
}

bool hub_gitio_rename(void* io, const char* from, const char* to) {
    storage_common_remove((Storage*)io, to); /* rename does not replace */
    return storage_common_rename((Storage*)io, from, to) == FSE_OK;
}

bool hub_gitio_remove(void* io, const char* path) {
    FS_Error e = storage_common_remove((Storage*)io, path);
    return e == FSE_OK || e == FSE_NOT_EXIST;
}

HubGitFile* hub_gitio_open_rw(void* io, const char* path) {
    HubGitFile* h = malloc(sizeof(HubGitFile));
    if(!h) return NULL;
    h->f = storage_file_alloc((Storage*)io);
    if(!h->f) {
        free(h);
        return NULL;
    }
    if(!storage_file_open(h->f, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        storage_file_close(h->f);
        storage_file_free(h->f);
        free(h);
        return NULL;
    }
    return h;
}
