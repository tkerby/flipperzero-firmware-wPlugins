/* Git routes: name checks, file helpers, repo helpers.
 * Part of hub_git.c: included there, in this order, not built alone. */
#include "hub_git_int.h"

/* ---------- names ---------- */

/* [A-Za-z0-9._-]{1,64}, no leading ".", no ".git" at the end */
static bool git_name_ok(const char* n) {
    size_t len = strlen(n);
    if(len < 1 || len > GIT_NAME_MAX || n[0] == '.') return false;
    for(size_t i = 0; i < len; i++) {
        char c = n[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        if(!ok) return false;
    }
    return !(len >= 4 && strcmp(n + len - 4, ".git") == 0);
}

/* Full ref name under refs/heads/ or refs/tags/, with the git rules. */
static bool git_ref_ok(const char* ref) {
    size_t len = strlen(ref);
    size_t pre;
    if(strncmp(ref, "refs/heads/", 11) == 0) {
        pre = 11;
    } else if(strncmp(ref, "refs/tags/", 10) == 0) {
        pre = 10;
    } else {
        return false;
    }
    if(len == pre || len > GIT_REF_MAX) return false;
    if(ref[len - 1] == '/' || ref[len - 1] == '.') return false;
    if(ref[pre] == '.') return false;

    for(size_t i = 0; i < len; i++) {
        char c = ref[i];
        if((unsigned char)c <= 0x20 || c == 0x7f || strchr("~^:?*[\\", c)) return false;
        char n = ref[i + 1];
        if(c == '.' && n == '.') return false;
        if(c == '/' && (n == '/' || n == '.')) return false;
        if(c == '@' && n == '{') return false;
        /* a component ending in ".lock" */
        if((c == '/' || n == '\0') && i >= 4) {
            const char* end = (n == '\0') ? ref + i + 1 : ref + i;
            if(strncmp(end - 5, ".lock", 5) == 0) return false;
        }
    }
    return true;
}

/* 40 lowercase hex characters */
static bool git_sha_ok(const char* s) {
    for(size_t i = 0; i < 40; i++) {
        char c = s[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return s[40] == '\0';
}

/* ---------- files ---------- */

static bool git_is_dir(GitCtx* g, const char* path) {
    FileInfo info;
    return storage_common_stat(g->r->storage, path, &info) == FSE_OK && file_info_is_dir(&info);
}

/* Read a small text file whole. NULL if it is missing or bigger than max. */
static FuriString* git_slurp(GitCtx* g, const char* path, size_t max) {
    File* f = storage_file_alloc(g->r->storage);
    FuriString* s = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING) && storage_file_size(f) <= max) {
        s = furi_string_alloc();
        uint8_t* io = g->r->io;
        size_t n;
        while((n = storage_file_read(f, io, sizeof(g->r->io) - 1)) > 0) {
            io[n] = '\0';
            furi_string_cat_str(s, (const char*)io);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return s;
}

/* Write through "<path>.part" and rename, so a file is never half written. */
static bool git_create(GitCtx* g);
static bool git_write(GitCtx* g, const char* path, const char* data, size_t len) {
    /* a new file needs no swap: CREATE_NEW fails if the file exists */
    File* nf = storage_file_alloc(g->r->storage);
    if(storage_file_open(nf, path, FSAM_WRITE, FSOM_CREATE_NEW)) {
        bool done = storage_file_write(nf, data, len) == len;
        storage_file_close(nf);
        storage_file_free(nf);
        if(!done) storage_common_remove(g->r->storage, path);
        return done;
    }
    storage_file_free(nf);
    snprintf(g->tmp, sizeof(g->tmp), "%s.part", path);
    File* f = storage_file_alloc(g->r->storage);
    bool ok = storage_file_open(f, g->tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) ok = storage_file_write(f, data, len) == len;
    storage_file_close(f);
    storage_file_free(f);
    if(ok) {
        storage_common_remove(g->r->storage, path);
        ok = storage_common_rename(g->r->storage, g->tmp, path) == FSE_OK;
    }
    if(!ok) storage_common_remove(g->r->storage, g->tmp);
    return ok;
}

/* Make every folder above the last "/" in path, below the repo folder. */
static bool git_mkdirs(GitCtx* g, const char* path) {
    size_t from = strlen(g->base) + 1;
    snprintf(g->tmp, sizeof(g->tmp), "%s", path);
    for(char* p = g->tmp + from; *p; p++) {
        if(*p != '/') continue;
        *p = '\0';
        bool ok = storage_simply_mkdir(g->r->storage, g->tmp);
        *p = '/';
        if(!ok) return false;
    }
    return true;
}

/* ---------- repo helpers ---------- */

static void git_set_base(GitCtx* g, const char* name) {
    snprintf(g->base, sizeof(g->base), GIT_ROOT "/%s.git", name);
}

/* Read "name" from the query, check it, check the repo exists. Sends errors. */
static bool git_need_repo(GitCtx* g, const char* key, bool must_exist) {
    char* dst = (strcmp(key, "to") == 0) ? g->to : g->name;
    if(!hub_query_get(g->r->query, key, dst, GIT_NAME_MAX + 16)) {
        hub_send_error(g->r, 400, "missing name");
        return false;
    }
    if(!git_name_ok(dst)) {
        hub_send_error(g->r, 400, "bad repo name");
        return false;
    }
    git_set_base(g, dst);
    if(must_exist && !git_is_dir(g, g->base)) {
        hub_send_error(g->r, 404, "repository not found");
        return false;
    }
    return true;
}

/* "ref: refs/heads/main" in HEAD -> out = "refs/heads/main" (or empty) */
static void git_head_ref(GitCtx* g, char* out, size_t size) {
    out[0] = '\0';
    snprintf(g->path, sizeof(g->path), "%s/HEAD", g->base);
    FuriString* s = git_slurp(g, g->path, 512);
    if(!s) return;
    const char* c = furi_string_get_cstr(s);
    if(strncmp(c, "ref: ", 5) == 0) {
        c += 5;
        size_t n = strcspn(c, "\r\n");
        if(n >= size) n = size - 1;
        memcpy(out, c, n);
        out[n] = '\0';
    }
    furi_string_free(s);
}

/* Count the .pack files in objects/pack. If list is set, add "P <file>\n" lines. */
static int git_packs(GitCtx* g, FuriString* list) {
    int count = 0;
    snprintf(g->path, sizeof(g->path), "%s/objects/pack", g->base);
    File* dir = storage_file_alloc(g->r->storage);
    if(storage_dir_open(dir, g->path)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(file_info_is_dir(&info) || len < 6 || strcmp(name + len - 5, ".pack") != 0)
                continue;
            count++;
            if(list) furi_string_cat_printf(list, "P %s\n", name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}
