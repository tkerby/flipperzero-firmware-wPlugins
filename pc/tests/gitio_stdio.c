/* Host implementation of flipper_app/hub_gitio.h (stdio + dirent). */
#define _FILE_OFFSET_BITS 64
#define _DEFAULT_SOURCE
#include "../../flipper_app/hub_gitio.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Fault injection for the tests: when >= 0 it counts down on every create,
 * write, read, mkdir and rename; at 0 and below those calls fail. */
long gitio_fault = -1;
long gitio_fault_hits;
static bool fault(void) {
    if(gitio_fault < 0) return false;
    if(gitio_fault == 0) {
        gitio_fault_hits++;
        return true;
    }
    gitio_fault--;
    return false;
}

/* Counters for the tests: file opens and read calls (the proxy for device time). */
unsigned long gitio_nopen, gitio_nread;

/* Route tests: HUB_ROOT maps "/ext/..." to a scratch folder (identity when unset). */
const char* host_map(const char* p, char* buf, size_t n) {
    const char* root = getenv("HUB_ROOT");
    if(!root || strncmp(p, "/ext/", 5) != 0) return p;
    snprintf(buf, n, "%s%s", root, p + 4);
    return buf;
}

struct HubGitFile {
    FILE* f;
};

struct HubGitDir {
    DIR* d;
    char* path;
};

HubGitFile* hub_gitio_open(void* io, const char* path) {
    (void)io;
    gitio_nopen++;
    char mb[1024];
    FILE* f = fopen(host_map(path, mb, sizeof mb), "rb");
    if(!f) return NULL;
    HubGitFile* h = malloc(sizeof(*h));
    if(!h) {
        fclose(f);
        return NULL;
    }
    h->f = f;
    return h;
}

void hub_gitio_close(HubGitFile* h) {
    if(!h) return;
    fclose(h->f);
    free(h);
}

uint64_t hub_gitio_size(HubGitFile* h) {
    long long cur = ftello(h->f);
    fseeko(h->f, 0, SEEK_END);
    long long end = ftello(h->f);
    fseeko(h->f, cur, SEEK_SET);
    return (uint64_t)end;
}

bool hub_gitio_seek(HubGitFile* h, uint64_t offset) {
    return fseeko(h->f, (off_t)offset, SEEK_SET) == 0;
}

size_t hub_gitio_read(HubGitFile* h, void* buf, size_t len) {
    gitio_nread++;
    if(fault()) return 0;
    return fread(buf, 1, len, h->f);
}

HubGitDir* hub_gitio_dir_open(void* io, const char* path) {
    (void)io;
    char mb[1024];
    DIR* d = opendir(host_map(path, mb, sizeof mb));
    if(!d) return NULL;
    HubGitDir* h = malloc(sizeof(*h));
    if(!h) {
        closedir(d);
        return NULL;
    }
    h->d = d;
    h->path = strdup(host_map(path, mb, sizeof mb));
    return h;
}

bool hub_gitio_dir_read(HubGitDir* h, char* name, size_t cap, bool* is_dir) {
    return hub_gitio_dir_read_ex(h, name, cap, is_dir, NULL);
}

bool hub_gitio_dir_read_ex(HubGitDir* h, char* name, size_t cap, bool* is_dir, uint64_t* size) {
    struct dirent* e;
    while((e = readdir(h->d)) != NULL) {
        if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if(strlen(e->d_name) + 1 > cap) continue;
        strcpy(name, e->d_name);
        if(is_dir || size) {
            char full[1024];
            struct stat st;
            snprintf(full, sizeof(full), "%s/%s", h->path, e->d_name);
            bool ok = stat(full, &st) == 0;
            if(is_dir) *is_dir = ok && S_ISDIR(st.st_mode);
            if(size) *size = ok ? (uint64_t)st.st_size : 0;
        }
        return true;
    }
    return false;
}

void hub_gitio_dir_close(HubGitDir* h) {
    if(!h) return;
    closedir(h->d);
    free(h->path);
    free(h);
}

HubGitFile* hub_gitio_create(void* io, const char* path) {
    (void)io;
    if(fault()) return NULL;
    char mb[1024];
    FILE* f = fopen(host_map(path, mb, sizeof mb), "w+b");
    if(!f) return NULL;
    HubGitFile* h = malloc(sizeof(*h));
    if(!h) {
        fclose(f);
        return NULL;
    }
    h->f = f;
    return h;
}

size_t hub_gitio_write(HubGitFile* h, const void* buf, size_t len) {
    if(fault()) return 0;
    return fwrite(buf, 1, len, h->f);
}

bool hub_gitio_mkdir(void* io, const char* path) {
    (void)io;
    if(fault()) return false;
    char mb[1024];
    path = host_map(path, mb, sizeof mb);
    return mkdir(path, 0755) == 0 || access(path, F_OK) == 0;
}

bool hub_gitio_rename(void* io, const char* from, const char* to) {
    (void)io;
    if(fault()) return false;
    char m1[1024], m2[1024];
    return rename(host_map(from, m1, sizeof m1), host_map(to, m2, sizeof m2)) == 0;
}

bool hub_gitio_remove(void* io, const char* path) {
    (void)io;
    char mb[1024];
    path = host_map(path, mb, sizeof mb);
    return remove(path) == 0 || access(path, F_OK) != 0;
}

HubGitFile* hub_gitio_open_rw(void* io, const char* path) {
    (void)io;
    char mb[1024];
    FILE* f = fopen(host_map(path, mb, sizeof mb), "r+b");
    if(!f) return NULL;
    HubGitFile* h = malloc(sizeof(*h));
    if(!h) {
        fclose(f);
        return NULL;
    }
    h->f = f;
    return h;
}
