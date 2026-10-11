#include "hub_git.h"
#include "hub_gitdoc.h"
#include "hub_gitobj.h"
#include "hub_plugin.h"
#include "hub_gitext.h"
#include "hub_gitpack.h"
#include "hub_gitgraph.h"
#include "hub_mem.h"

/* Git hosting, phase 1: bare repositories on the SD card, served over the git
 * dumb HTTP protocol. The PC side does all the real git work (see
 * docs/git-design.md). Here we only store files and keep refs consistent. */

#define GIT_ROOT       "/ext/git"
#define GIT_NAME_MAX   64
#define GIT_REF_MAX    180
#define GIT_PATH_MAX   320
#define GIT_TEXT_MAX   200
#define GIT_REFS_MAX   256
#define GIT_REPOS_MAX  128
#define GIT_FILE_MAX   (64 * 1024)
#define GIT_ZERO       "0000000000000000000000000000000000000000"
#define GIT_SKELETON_CONFIG "[core]\n\trepositoryformatversion = 0\n\tfilemode = false\n\tbare = true\n"

typedef struct {
    HubReq* r;
    char name[GIT_NAME_MAX + 16]; /* a few spare bytes: a too long name must not look valid */
    char to[GIT_NAME_MAX + 16];
    char ref[GIT_REF_MAX + 20];
    char osha[48];
    char nsha[48];
    char text[GIT_TEXT_MAX + 56];
    char base[GIT_NAME_MAX + 32]; /* /ext/git/<name>.git */
    char path[GIT_PATH_MAX];
    char tmp[GIT_PATH_MAX + 8];
} GitCtx;

typedef struct {
    char* name;
    char sha[41];
} GitRef;

typedef struct {
    GitRef* v;
    size_t n, cap;
} GitRefs;

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
            if(file_info_is_dir(&info) || len < 6 || strcmp(name + len - 5, ".pack") != 0) continue;
            count++;
            if(list) furi_string_cat_printf(list, "P %s\n", name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

/* ---------- refs ---------- */

static void git_refs_free(GitRefs* l) {
    for(size_t i = 0; i < l->n; i++) free(l->v[i].name);
    free(l->v);
}

static GitRef* git_refs_find(GitRefs* l, const char* name) {
    for(size_t i = 0; i < l->n; i++) {
        if(strcmp(l->v[i].name, name) == 0) return &l->v[i];
    }
    return NULL;
}

static bool git_refs_add(GitRefs* l, const char* name, size_t name_len, const char* sha) {
    if(l->n >= GIT_REFS_MAX) return false;
    if(l->n == l->cap) { /* grow by doubling: most repos have few refs */
        size_t nc = l->cap ? l->cap * 2 : 16;
        GitRef* nv = realloc(l->v, nc * sizeof(GitRef));
        if(!nv) return false;
        l->v = nv;
        l->cap = nc;
    }
    char* nm = malloc(name_len + 1);
    if(!nm) return false;
    GitRef* e = &l->v[l->n++];
    e->name = nm;
    memcpy(e->name, name, name_len);
    e->name[name_len] = '\0';
    memcpy(e->sha, sha, 40);
    e->sha[40] = '\0';
    return true;
}

/* qsort is not in the firmware API, and the lists are short: insertion sort. */
static void git_refs_sort(GitRefs* l) {
    for(size_t i = 1; i < l->n; i++) {
        GitRef t = l->v[i];
        size_t j = i;
        while(j > 0 && strcmp(l->v[j - 1].name, t.name) > 0) {
            l->v[j] = l->v[j - 1];
            j--;
        }
        l->v[j] = t;
    }
}

/* Find one ref in packed-refs. */
static bool git_packed_find(GitCtx* g, const char* ref, char* sha_out) {
    snprintf(g->path, sizeof(g->path), "%s/packed-refs", g->base);
    FuriString* s = git_slurp(g, g->path, GIT_FILE_MAX);
    if(!s) return false;
    bool found = false;
    size_t ref_len = strlen(ref);
    for(const char* p = furi_string_get_cstr(s); *p;) {
        const char* e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if(len == 41 + ref_len && p[40] == ' ' && memcmp(p + 41, ref, ref_len) == 0) {
            memcpy(sha_out, p, 40);
            sha_out[40] = '\0';
            found = git_sha_ok(sha_out);
            break;
        }
        p += len + (e ? 1 : 0);
    }
    furi_string_free(s);
    return found;
}

/* The loose refs are walked folder by folder with a queue, not by recursion. */
static bool git_refs_load(GitCtx* g, GitRefs* l) {
    l->v = NULL;
    l->n = l->cap = 0;
    bool ok = true;

    FuriString* queue = furi_string_alloc_set("refs/heads\nrefs/tags\n");
    FuriString* dirref = furi_string_alloc();
    FuriString* child = furi_string_alloc();
    File* dir = storage_file_alloc(g->r->storage);
    File* file = storage_file_alloc(g->r->storage);
    size_t pos = 0;
    char name[128];
    char sha[48];

    while(ok && pos < furi_string_size(queue)) {
        size_t end = furi_string_search_char(queue, '\n', pos);
        furi_string_set_n(dirref, queue, pos, end - pos);
        pos = end + 1;

        hub_beat();
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, furi_string_get_cstr(dirref));
        if(!storage_dir_open(dir, g->path)) {
            /* never reuse a File after a failed open */
            storage_file_free(dir);
            dir = storage_file_alloc(g->r->storage);
            continue;
        }
        FileInfo info;
        while(ok && storage_dir_read(dir, &info, name, sizeof(name))) {
            furi_string_printf(child, "%s/%s", furi_string_get_cstr(dirref), name);
            if(furi_string_size(child) > GIT_REF_MAX) continue;
            if(file_info_is_dir(&info)) {
                furi_string_cat_printf(queue, "%s\n", furi_string_get_cstr(child));
                continue;
            }
            snprintf(g->path, sizeof(g->path), "%s/%s", g->base, furi_string_get_cstr(child));
            size_t n = 0;
            if(storage_file_open(file, g->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                n = storage_file_read(file, sha, 40);
                storage_file_close(file);
            } else {
                storage_file_free(file); /* never reuse a File after a failed open */
                file = storage_file_alloc(g->r->storage);
            }
            sha[n] = '\0';
            if(n == 40 && git_sha_ok(sha)) {
                ok = git_refs_add(l, furi_string_get_cstr(child), furi_string_size(child), sha);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(file);
    storage_file_free(dir);
    furi_string_free(child);
    furi_string_free(dirref);
    furi_string_free(queue);

    /* packed-refs: only the refs that have no loose file */
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/packed-refs", g->base);
        FuriString* s = git_slurp(g, g->path, GIT_FILE_MAX);
        if(s) {
            for(const char* p = furi_string_get_cstr(s); ok && *p;) {
                const char* e = strchr(p, '\n');
                size_t len = e ? (size_t)(e - p) : strlen(p);
                if(len > 41 && p[40] == ' ' && len - 41 <= GIT_REF_MAX) {
                    memcpy(sha, p, 40);
                    sha[40] = '\0';
                    char* nm = (char*)p + 41;
                    char saved = nm[len - 41];
                    nm[len - 41] = '\0'; /* the string is ours to cut */
                    if(git_sha_ok(sha) &&
                       (strncmp(nm, "refs/heads/", 11) == 0 || strncmp(nm, "refs/tags/", 10) == 0) &&
                       !git_refs_find(l, nm)) {
                        ok = git_refs_add(l, nm, len - 41, sha);
                    }
                    nm[len - 41] = saved;
                }
                p += len + (e ? 1 : 0);
            }
            furi_string_free(s);
        }
    }

    if(ok) git_refs_sort(l);
    return ok;
}

/* info/refs: "<sha>\t<ref>\n" for every ref, sorted */
static bool git_write_info_refs(GitCtx* g) {
    hub_git_mark_base(g->base); /* a ref changed: the commit graph may need an update */
    GitRefs l;
    if(!git_refs_load(g, &l)) {
        git_refs_free(&l);
        return false;
    }
    FuriString* s = furi_string_alloc();
    for(size_t i = 0; i < l.n; i++) {
        furi_string_cat_printf(s, "%s\t%s\n", l.v[i].sha, l.v[i].name);
    }
    git_refs_free(&l);

    snprintf(g->path, sizeof(g->path), "%s/info/refs", g->base);
    bool ok = git_mkdirs(g, g->path) && git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    furi_string_free(s);
    return ok;
}

/* objects/info/packs: "P pack-<hash>.pack\n" per pack, then an empty line */
static bool git_write_info_packs(GitCtx* g) {
    FuriString* s = furi_string_alloc();
    git_packs(g, s);
    furi_string_push_back(s, '\n');

    snprintf(g->path, sizeof(g->path), "%s/objects/info/packs", g->base);
    bool ok = git_mkdirs(g, g->path) && git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    furi_string_free(s);
    return ok;
}

/* Current value of a ref: loose file first, then packed-refs. */
static bool git_ref_current(GitCtx* g, const char* ref, char* sha_out) {
    snprintf(g->path, sizeof(g->path), "%s/%s", g->base, ref);
    FuriString* s = git_slurp(g, g->path, 128);
    if(s) {
        const char* c = furi_string_get_cstr(s);
        bool ok = strlen(c) >= 40;
        if(ok) {
            memcpy(sha_out, c, 40);
            sha_out[40] = '\0';
            ok = git_sha_ok(sha_out);
        }
        furi_string_free(s);
        if(ok) return true;
    }
    return git_packed_find(g, ref, sha_out);
}

/* Remove the ref line (and its "^" peeled line) from packed-refs. */
static bool git_packed_drop(GitCtx* g, const char* ref) {
    snprintf(g->path, sizeof(g->path), "%s/packed-refs", g->base);
    FuriString* s = git_slurp(g, g->path, GIT_FILE_MAX);
    if(!s) return true;

    FuriString* out = furi_string_alloc();
    size_t ref_len = strlen(ref);
    bool dropped = false, skip_peeled = false;
    for(const char* p = furi_string_get_cstr(s); *p;) {
        const char* e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        bool drop = false;
        if(skip_peeled && p[0] == '^') {
            drop = true;
        } else if(len == 41 + ref_len && p[40] == ' ' && memcmp(p + 41, ref, ref_len) == 0) {
            drop = true;
            dropped = true;
        }
        skip_peeled = drop && p[0] != '^';
        if(!drop) {
            for(size_t i = 0; i < len; i++) furi_string_push_back(out, p[i]);
            furi_string_push_back(out, '\n');
        }
        p += len + (e ? 1 : 0);
    }
    bool ok = true;
    if(dropped) {
        ok = git_write(g, g->path, furi_string_get_cstr(out), furi_string_size(out));
    }
    furi_string_free(out);
    furi_string_free(s);
    return ok;
}

/* info/refs and objects/info/packs of one repo are stale: they are rebuilt
 * when they are next read. Only one repo is tracked; a second one forces the
 * first to be rebuilt now. */
static char git_dirty[GIT_NAME_MAX + 1];

/* ---------- routes: dumb HTTP files ---------- */

static void git_route_file(HubReq* r, GitCtx* g) {
    /* /git/<name>.git/<rest> */
    const char* p = r->path + 5;
    const char* slash = strchr(p, '/');
    size_t seg = slash ? (size_t)(slash - p) : strlen(p);
    if(!slash || seg < 5 || seg > GIT_NAME_MAX + 4 || strncmp(p + seg - 4, ".git", 4) != 0) {
        hub_send_error(r, 404, "not found");
        return;
    }
    memcpy(g->name, p, seg - 4);
    g->name[seg - 4] = '\0';
    if(!git_name_ok(g->name)) {
        hub_send_error(r, 404, "not found");
        return;
    }

    /* no empty segments, no "." or ".." */
    const char* rest = slash + 1;
    size_t rest_len = strlen(rest);
    if(rest_len == 0 || rest_len > 150) {
        hub_send_error(r, 404, "not found");
        return;
    }
    for(const char* s = rest; *s;) {
        size_t n = strcspn(s, "/");
        if(n == 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) {
            hub_send_error(r, 400, "bad path");
            return;
        }
        s += n + (s[n] == '/' ? 1 : 0);
    }
    if(strchr(rest, '\\') || rest[rest_len - 1] == '/') {
        hub_send_error(r, 400, "bad path");
        return;
    }

    git_set_base(g, g->name);
    /* push to create: the receive-pack discovery of a missing repo makes it */
    if(strcmp(rest, "info/refs") == 0 && r->query && strstr(r->query, "service=git-receive-pack") &&
       !git_is_dir(g, g->base)) {
        storage_simply_mkdir(r->storage, GIT_ROOT);
        strcpy(g->ref, "refs/heads/main");
        strcpy(g->text, "Unnamed repository");
        if(!git_create(g)) {
            hub_send_error(r, 500, "could not make the repository");
            return;
        }
    }
    /* stale after a WebDAV push: rebuild what the reader is about to get */
    if(strcmp(g->name, git_dirty) == 0 &&
       (strcmp(rest, "info/refs") == 0 || strcmp(rest, "objects/info/packs") == 0)) {
        git_dirty[0] = '\0';
        git_write_info_refs(g);
        git_write_info_packs(g);
    }
    snprintf(g->path, sizeof(g->path), "%s/%s", g->base, rest);
    bool text = strcmp(rest, "info/refs") == 0 || strcmp(rest, "HEAD") == 0 ||
                strcmp(rest, "objects/info/packs") == 0;
    hub_serve_file_as(r, g->path, text ? "text/plain" : "application/octet-stream", false);
}

/* ---------- routes: API ---------- */

static void git_route_repos(GitCtx* g) {
    HubReq* r = g->r;
    char* names = malloc(GIT_REPOS_MAX * (GIT_NAME_MAX + 1));
    if(!names) {
        hub_send_error(r, 503, "low memory, try again");
        return;
    }
    size_t count = 0;

    File* dir = storage_file_alloc(r->storage);
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(count < GIT_REPOS_MAX && storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(!file_info_is_dir(&info) || len < 5 || strcmp(name + len - 4, ".git") != 0) continue;
            name[len - 4] = '\0';
            if(!git_name_ok(name)) continue;
            strcpy(names + count * (GIT_NAME_MAX + 1), name);
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    char tmp_name[GIT_NAME_MAX + 1];
    for(size_t i = 1; i < count; i++) {
        size_t w = GIT_NAME_MAX + 1;
        memcpy(tmp_name, names + i * w, w);
        size_t j = i;
        while(j > 0 && strcmp(names + (j - 1) * w, tmp_name) > 0) {
            memcpy(names + j * w, names + (j - 1) * w, w);
            j--;
        }
        memcpy(names + j * w, tmp_name, w);
    }

    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"repos\":[");
    char head[GIT_REF_MAX];
    for(size_t i = 0; i < count; i++) {
        const char* name = names + i * (GIT_NAME_MAX + 1);
        git_set_base(g, name);

        snprintf(g->tmp, sizeof(g->tmp), "%s/description", g->base);
        FuriString* d = git_slurp(g, g->tmp, 1024);
        char desc[GIT_TEXT_MAX + 1] = {0};
        if(d) {
            size_t n = strcspn(furi_string_get_cstr(d), "\r\n");
            if(n > GIT_TEXT_MAX) n = GIT_TEXT_MAX;
            memcpy(desc, furi_string_get_cstr(d), n);
            furi_string_free(d);
        }

        git_head_ref(g, head, sizeof(head));
        const char* branch = strncmp(head, "refs/heads/", 11) == 0 ? head + 11 : "";

        if(i) furi_string_push_back(s, ',');
        furi_string_cat_str(s, "{\"name\":");
        hub_json_str(s, name);
        furi_string_cat_str(s, ",\"description\":");
        hub_json_str(s, desc);
        furi_string_cat_str(s, ",\"head\":");
        hub_json_str(s, branch);
        furi_string_cat_printf(s, ",\"packs\":%d}", git_packs(g, NULL));
    }
    furi_string_cat_str(s, "]}");
    free(names);
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

/* Make the bare skeleton. g->base, g->ref (full HEAD branch) and g->text
 * (description) are set. Removes the half made repo on failure. */
static bool git_create(GitCtx* g) {
    static const char* const dirs[] = {
        "", "/refs", "/refs/heads", "/refs/tags", "/objects", "/objects/pack", "/objects/info", "/info"};
    bool ok = storage_simply_mkdir(g->r->storage, GIT_ROOT);
    for(size_t i = 0; ok && i < COUNT_OF(dirs); i++) {
        snprintf(g->path, sizeof(g->path), "%s%s", g->base, dirs[i]);
        ok = storage_simply_mkdir(g->r->storage, g->path);
    }

    FuriString* s = furi_string_alloc();
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/HEAD", g->base);
        furi_string_printf(s, "ref: %s\n", g->ref);
        ok = git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    }
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/config", g->base);
        ok = git_write(g, g->path, GIT_SKELETON_CONFIG, sizeof(GIT_SKELETON_CONFIG) - 1);
    }
    if(ok) {
        snprintf(g->path, sizeof(g->path), "%s/description", g->base);
        furi_string_printf(s, "%s\n", g->text);
        ok = git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    }
    furi_string_free(s);
    if(ok) ok = git_write_info_refs(g);
    if(ok) ok = git_write_info_packs(g);

    if(!ok) storage_simply_remove_recursive(g->r->storage, g->base);
    return ok;
}

static void git_route_create(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", false)) return;
    if(storage_common_stat(r->storage, g->base, NULL) == FSE_OK) {
        hub_send_error(r, 409, "repository exists");
        return;
    }

    char branch[GIT_REF_MAX];
    if(!hub_query_get(r->query, "branch", branch, sizeof(branch)) || branch[0] == '\0') {
        strcpy(branch, "main");
    }
    snprintf(g->ref, sizeof(g->ref), "refs/heads/%s", branch);
    if(!git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad branch name");
        return;
    }
    if(!hub_query_get(r->query, "description", g->text, sizeof(g->text)) || g->text[0] == '\0') {
        strcpy(g->text, "Unnamed repository");
    }
    for(char* c = g->text; *c; c++) {
        if(*c == '\r' || *c == '\n') *c = ' ';
    }

    if(git_create(g)) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "could not make the repository");
    }
}

static void git_route_delete(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(storage_simply_remove_recursive(g->r->storage, g->base)) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "could not delete the repository");
    }
}

static void git_route_rename(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    char from[sizeof(g->base)];
    strcpy(from, g->base);
    if(!git_need_repo(g, "to", false)) return;
    if(storage_common_stat(r->storage, g->base, NULL) == FSE_OK) {
        hub_send_error(r, 409, "repository exists");
        return;
    }
    FS_Error err = storage_common_rename(r->storage, from, g->base);
    if(err == FSE_OK) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, storage_error_get_desc(err));
    }
}

static void git_route_describe(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_query_get(g->r->query, "text", g->text, sizeof(g->text))) {
        hub_send_error(g->r, 400, "missing text");
        return;
    }
    for(char* c = g->text; *c; c++) {
        if(*c == '\r' || *c == '\n') *c = ' ';
    }
    strcat(g->text, "\n");
    snprintf(g->path, sizeof(g->path), "%s/description", g->base);
    if(git_write(g, g->path, g->text, strlen(g->text))) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "write failed");
    }
}

static void git_route_refs(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    GitRefs l;
    if(!git_refs_load(g, &l)) {
        git_refs_free(&l);
        hub_send_error(g->r, 503, "low memory or too many refs, try again");
        return;
    }
    git_head_ref(g, g->ref, sizeof(g->ref));

    /* annotated tags get "peeled": the commit they point to */
    HubGit* og = NULL;
    for(size_t i = 0; i < l.n && !og; i++) {
        if(strncmp(l.v[i].name, "refs/tags/", 10) == 0) {
            og = hub_git_open(g->r->storage, g->base);
            if(og) hub_git_set_limit(og, 4 * 1024); /* tags are small */
        }
    }

    HubChunk* ck = malloc(sizeof(HubChunk));
    if(!ck || !hub_chunk_begin(ck, g->r, "application/json", NULL)) {
        if(!ck) hub_send_error(g->r, 503, "low memory, try again");
        free(ck);
        if(og) hub_git_close(og);
        git_refs_free(&l);
        return;
    }
    hub_chunk_lit(ck, "{\"ok\":true,\"head\":");
    hub_chunk_json(ck, g->ref, strlen(g->ref));
    hub_chunk_lit(ck, ",\"refs\":[");
    char line[160];
    for(size_t i = 0; i < l.n && ck->ok; i++) {
        if(i) hub_chunk_lit(ck, ",");
        hub_chunk_lit(ck, "{\"name\":");
        hub_chunk_json(ck, l.v[i].name, strlen(l.v[i].name));
        int n = snprintf(line, sizeof(line), ",\"sha\":\"%s\"", l.v[i].sha);
        HubOid oid;
        uint8_t type;
        size_t size;
        if(og && strncmp(l.v[i].name, "refs/tags/", 10) == 0 && hub_oid_from_hex(l.v[i].sha, &oid) &&
           hub_git_info(og, &oid, &type, &size) == HUB_GIT_OK && type == HUB_OBJ_TAG &&
           hub_git_peel(og, &oid) == HUB_GIT_OK) {
            char hex[41];
            hub_oid_to_hex(&oid, hex);
            n += snprintf(line + n, sizeof(line) - (size_t)n, ",\"peeled\":\"%s\"", hex);
        }
        line[n++] = '}';
        hub_chunk_write(ck, line, (size_t)n);
    }
    hub_chunk_lit(ck, "]}");
    hub_chunk_end(ck, ck->ok);
    free(ck);
    if(og) hub_git_close(og);
    git_refs_free(&l);
}

/* Remove the empty folders a deleted ref leaves behind (refs/heads/a/b -> a). */
static void git_prune_dirs(GitCtx* g, const char* ref) {
    size_t blen = strlen(g->base);
    snprintf(g->tmp, sizeof(g->tmp), "%s/%s", g->base, ref);
    char* slash;
    while((slash = strrchr(g->tmp, '/')) != NULL) {
        *slash = '\0';
        if(strcmp(g->tmp + blen, "/refs/heads") == 0 || strcmp(g->tmp + blen, "/refs/tags") == 0)
            break;
        /* fails while the folder still has files in it, which is what we want */
        if(storage_common_remove(g->r->storage, g->tmp) != FSE_OK) break;
    }
}

static void git_route_ref(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_query_get(r->query, "ref", g->ref, sizeof(g->ref)) || !git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad ref name");
        return;
    }
    if(!hub_query_get(r->query, "old", g->osha, sizeof(g->osha)) || !git_sha_ok(g->osha) ||
       !hub_query_get(r->query, "new", g->nsha, sizeof(g->nsha)) || !git_sha_ok(g->nsha)) {
        hub_send_error(r, 400, "bad sha");
        return;
    }

    char cur[41];
    if(!git_ref_current(g, g->ref, cur)) strcpy(cur, GIT_ZERO);
    if(strcmp(cur, g->osha) != 0) {
        hub_send_error(r, 409, "ref changed");
        return;
    }

    bool ok = true;
    if(strcmp(g->nsha, GIT_ZERO) == 0) {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->ref);
        storage_common_remove(r->storage, g->path);
        ok = git_packed_drop(g, g->ref);
        git_prune_dirs(g, g->ref);
    } else {
        snprintf(g->path, sizeof(g->path), "%s/%s", g->base, g->ref);
        if(!git_mkdirs(g, g->path)) {
            hub_send_error(r, 409, "ref conflicts with another ref");
            return;
        }
        g->nsha[40] = '\n';
        if(!git_write(g, g->path, g->nsha, 41)) {
            hub_send_error(r, 409, "ref conflicts with another ref");
            return;
        }
    }
    if(ok) ok = git_write_info_refs(g);
    if(ok) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "write failed");
    }
}

static void git_route_head(GitCtx* g) {
    HubReq* r = g->r;
    if(!git_need_repo(g, "name", true)) return;
    char branch[GIT_REF_MAX];
    if(!hub_query_get(r->query, "branch", branch, sizeof(branch))) {
        hub_send_error(r, 400, "missing branch");
        return;
    }
    snprintf(g->ref, sizeof(g->ref), "refs/heads/%s", branch);
    if(!git_ref_ok(g->ref)) {
        hub_send_error(r, 400, "bad branch name");
        return;
    }
    snprintf(g->text, sizeof(g->text), "ref: %s\n", g->ref);
    snprintf(g->path, sizeof(g->path), "%s/HEAD", g->base);
    if(git_write(g, g->path, g->text, strlen(g->text))) {
        hub_send_ok(r);
    } else {
        hub_send_error(r, 500, "write failed");
    }
}

static void git_route_server_info(GitCtx* g) {
    if(!git_need_repo(g, "name", true)) return;
    if(git_write_info_refs(g) && git_write_info_packs(g)) {
        hub_send_ok(g->r);
    } else {
        hub_send_error(g->r, 500, "write failed");
    }
}

/* ---------- packing loose objects ---------- */

#define PACK_BATCH     128
#define PACK_RAM       (16 * 1024)
#define PACK_MARKS     16
#define PACK_QUIET_MS  30000 /* no auto-pack this long after a write to the repo */

typedef struct {
    char name[GIT_NAME_MAX + 1];
    uint32_t last_write; /* tick */
    HubGitPackCursor cursor; /* where the last idle slice stopped */
    HubGitMergeCursor* merge; /* allocated only while a pack merge job runs */
    bool graph;  /* the commit graph may be out of date */
    bool rescan; /* loose objects came in while a pack was open: scan again when it is finished */
} PackMark;

static PackMark* pack_marks; /* malloc'd on first use */

static PackMark* pack_find(const char* name) {
    if(!pack_marks) return NULL;
    for(size_t i = 0; i < PACK_MARKS; i++) {
        if(strcmp(pack_marks[i].name, name) == 0) return &pack_marks[i];
    }
    return NULL;
}

void hub_git_touch(const char* name) {
    PackMark* m = pack_find(name);
    if(m) m->last_write = furi_get_tick();
}

static PackMark* pack_get(const char* name) {
    if(!pack_marks) {
        pack_marks = malloc(sizeof(PackMark) * PACK_MARKS);
        if(!pack_marks) return NULL;
        memset(pack_marks, 0, sizeof(PackMark) * PACK_MARKS);
    }
    PackMark* m = pack_find(name);
    if(!m) {
        for(size_t i = 0; i < PACK_MARKS && !m; i++) {
            if(pack_marks[i].name[0] == '\0') m = &pack_marks[i];
        }
        if(!m) return NULL; /* full: the next start marks it again */
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->cursor.dir = 0;
    }
    return m;
}

void hub_git_mark_loose(const char* name) {
    PackMark* m = pack_get(name);
    if(!m) return;
    m->last_write = furi_get_tick();
    m->graph = true;
    if(m->cursor.job) {
        m->rescan = true; /* the open pack must not list a folder twice */
    } else {
        m->cursor.dir = 0; /* new objects may be in folders already passed */
    }
}

/* name of "/ext/git/<name>.git" */
void hub_git_mark_base(const char* base) {
    size_t root = sizeof(GIT_ROOT);
    size_t len = strlen(base);
    if(len <= root + 4) return;
    char name[GIT_NAME_MAX + 1];
    len -= root + 4;
    if(len > GIT_NAME_MAX) return;
    memcpy(name, base + root, len);
    name[len] = '\0';
    hub_git_mark_loose(name);
}

/* A read had to open commit objects the graph did not know: only the graph needs work. */
static void graph_stale(const char* base) {
    size_t root = sizeof(GIT_ROOT);
    size_t len = strlen(base);
    if(len <= root + 4 || len - root - 4 > GIT_NAME_MAX) return;
    char name[GIT_NAME_MAX + 1];
    memcpy(name, base + root, len - root - 4);
    name[len - root - 4] = '\0';
    PackMark* m = pack_get(name);
    if(!m) return;
    m->graph = true;
    m->last_write = furi_get_tick();
}

static void pack_unmark(const char* name) {
    PackMark* m = pack_find(name);
    if(m) {
        m->name[0] = '\0';
        free(m->merge);
        free(m->cursor.job);
        m->cursor.job = NULL;
        m->merge = NULL;
        m->graph = m->rescan = false;
    }
}

void hub_git_free_state(void) {
    if(!pack_marks) return;
    for(size_t i = 0; i < PACK_MARKS; i++) {
        free(pack_marks[i].merge);
        free(pack_marks[i].cursor.job);
    }
    free(pack_marks);
    pack_marks = NULL;
}

/* At start every repo may have loose objects: mark them all once. */
void hub_git_mark_all(Storage* storage) {
    hub_gitobj_stale = graph_stale;
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(!file_info_is_dir(&info) || len < 5 || strcmp(name + len - 4, ".git") != 0) continue;
            name[len - 4] = '\0';
            if(git_name_ok(name)) hub_git_mark_loose(name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    /* the start is not a write: the packer may begin as soon as it is idle */
    if(pack_marks) {
        for(size_t i = 0; i < PACK_MARKS; i++) pack_marks[i].last_write = 0;
    }
}

/* objects/info/packs lists the new pack (needed after every batch). */
static void pack_regen_packs(HubReq* r, const char* name) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    git_write_info_packs(g);
    free(g);
}

/* The packer keeps what it packed when it is stopped, so a slice is just a
 * call with a deadline. Idle slices are 3 s, the API route loops up to 25 s. */
/* Idle packer switch: false keeps the marks but never packs by itself
 * (POST /api/git/pack still works). */
#define PACK_AUTO     true
/* Object writing per slice. The finish (SHA-1 over the pack, idx, renames) and the
 * deletion of the packed loose files come on top: measured 1.0-2.2 s, more for big objects. */
#define PACK_SLICE_MS 1000
#define PACK_API_MS   25000

typedef struct {
    uint32_t budget;   /* ms of object writing, counted from the first object */
    uint32_t deadline; /* tick */
    uint32_t first; /* tick of the first stop callback */
} PackClock;

static bool pack_stop(void* ctx) {
    hub_beat();
    PackClock* k = ctx;
    if(!k->first) {
        k->first = furi_get_tick();
        k->deadline = k->first + k->budget; /* the scan for the first object does not count */
    }
    return (int32_t)(furi_get_tick() - k->deadline) >= 0;
}

/* One slice for the repo. Returns true while more work is likely. */
static bool pack_slice(
    HubReq* r,
    const char* name,
    HubGitPackCursor* cursor,
    uint32_t budget,
    HubGitPackResult* res,
    bool* nomem) {
    char dir[GIT_NAME_MAX + 24];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    PackClock k = {.budget = budget};
    memset(res, 0, sizeof(*res));
    uint16_t dir_before = cursor ? cursor->dir : 0;
    int rc = hub_git_pack_loose(r->storage, dir, PACK_BATCH, PACK_RAM, cursor, NULL, pack_stop, &k, res);
    if(rc == HUB_GIT_ERR_NOMEM) *nomem = true;
    if(res->finished) pack_regen_packs(r, name);
    /* a slice that only scanned folders is progress too, if the cursor moved */
    bool progress = res->packed > 0 || res->finished || res->stopped || (cursor && (cursor->dir != dir_before || cursor->job));
    return rc == 0 && res->more && progress;
}

/* One slice of the commit graph (in the pack plugin). Returns true while more is pending. */
static bool graph_slice(HubReq* r, const char* name, PackMark* m, bool* blocked) {
    if(!hub_plugin_fits(HUB_PL_PACK)) {
        *blocked = true;
        return false;
    }
    char dir[GIT_NAME_MAX + 24];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    PackClock k = {.budget = PACK_SLICE_MS};
    HubGitGraphResult gr;
    int rc = hub_git_graph_update(r->storage, dir, pack_stop, &k, &gr);
    if(rc == HUB_GIT_ERR_NOMEM) *blocked = true;
    if(rc != 0 || !gr.more) {
        if(m) m->graph = false; /* done, or failed: the next mark tries again */
        return false;
    }
    return true;
}

/* One slice of work for a repo: merge small packs first (when 4 or more, or a
 * merge job is running), else pack loose objects, else update the commit graph.
 * Returns true while more work is likely. m is the repo's idle state (NULL:
 * nothing is kept between calls). */
#define PACK_HARD_CAP 60 /* no new packs above this many; merging goes on */
#define PACK_MERGE_AT 4

static bool work_slice(
    HubReq* r,
    const char* name,
    PackMark* m,
    unsigned* packed,
    unsigned* merged,
    bool* blocked) {
    char dir[GIT_NAME_MAX + 24];
    snprintf(dir, sizeof(dir), GIT_ROOT "/%s.git", name);
    unsigned total = 0, nsmall = 0;
    hub_git_pack_stats(r->storage, dir, 0, &total, &nsmall);

    if((m && m->merge) || nsmall >= PACK_MERGE_AT) {
        if(!hub_plugin_fits(HUB_PL_PACK)) {
            *blocked = true;
            return false;
        }
        HubGitMergeCursor* mc = m ? m->merge : NULL;
        if(!mc) {
            mc = calloc(1, sizeof(HubGitMergeCursor));
            if(!mc) return false;
            if(m) m->merge = mc;
        }
        PackClock k = {.budget = PACK_SLICE_MS};
        HubGitMergeResult mres;
        memset(&mres, 0, sizeof(mres));
        int rc = hub_git_pack_merge(r->storage, dir, mc, 0, 0, pack_stop, &k, &mres);
        if(rc != 0 || !mres.more) {
            free(mc);
            if(m) m->merge = NULL;
        }
        if(mres.merged_packs > 0) {
            *merged += mres.merged_packs;
            pack_regen_packs(r, name); /* the packs list changed */
        }
        if(rc == HUB_GIT_ERR_NOMEM) *blocked = true;
        if(rc != 0) return false;
        if(mres.more || mres.merged_packs > 0) return true;
        /* nothing could be merged (for example shared objects): go on to loose objects */
    }
    if(total >= PACK_HARD_CAP && !(m && m->cursor.job)) return false;
    if(!hub_plugin_fits(HUB_PL_PACK)) { /* the plugin and a pack slice (36 KB, host peak 24 KB) */
        *blocked = true;
        return false;
    }

    HubGitPackResult res;
    bool nomem = false;
    bool more = pack_slice(r, name, m ? &m->cursor : NULL, PACK_SLICE_MS, &res, &nomem);
    *packed += res.packed;
    if(nomem) {
        *blocked = true; /* the allocator refused a block: try again later */
        return false;
    }
    if(more) return true;
    if(m && m->rescan && !m->cursor.job) { /* objects came in during the job */
        m->rescan = false;
        m->cursor.dir = 0;
        return true;
    }
    if(m && m->cursor.job) return true; /* a pack is still open */
    if(m && m->graph) return graph_slice(r, name, m, blocked);
    return false;
}

/* Called by the worker when nothing has come in for a while. One slice. */
void hub_git_idle_tick(Storage* storage) {
    if(!PACK_AUTO || !pack_marks) return;
    if(furi_get_tick() < 60000) return; /* let the app settle after the start */
    if(!hub_sd_free_ok(storage, 0)) return;
    uint32_t now = furi_get_tick();
    for(size_t i = 0; i < PACK_MARKS; i++) {
        PackMark* m = &pack_marks[i];
        if(!m->name[0]) continue;
        if(m->last_write && now - m->last_write < PACK_QUIET_MS) continue;
        if(hub_dav_repo_locked(m->name)) continue;
        HubReq* r = malloc(sizeof(HubReq));
        if(!r) return;
        memset(r, 0, sizeof(HubReq));
        r->storage = storage;
        char name[GIT_NAME_MAX + 1];
        strcpy(name, m->name);
        unsigned packed = 0, merged = 0;
        bool blocked = false;
        hub_activity = HUB_ACT_PACK;
        bool more = work_slice(r, name, m, &packed, &merged, &blocked);
        hub_activity = HUB_ACT_IDLE;
        if(blocked) {
            m->last_write = furi_get_tick(); /* not enough heap now: look again in 30 s */
        } else if(!more) {
            pack_unmark(name);
        } /* done, nothing to do, or failed: retry at the next start */
        free(r);
        return; /* one slice only */
    }
}

/* POST /api/git/pack?name= : slices until done or the time is up.
 * Answer: {"ok":true,"packed":N,"merged":M,"remaining":0|1,"more":bool} */
static void git_route_pack(GitCtx* g) {
    if(!hub_plugin_fits(HUB_PL_PACK)) {
        hub_send_error(g->r, 503, "low memory, try again");
        return;
    }
    if(!git_need_repo(g, "name", true)) return;
    if(!hub_sd_ok(g->r, 0)) return;
    hub_git_mark_loose(g->name); /* make sure the idle state (cursors) exists */
    PackMark* mk = pack_find(g->name);
    uint32_t end = furi_get_tick() + PACK_API_MS;
    unsigned packed = 0, merged = 0;
    bool more, blocked = false;
    for(;;) {
        char name[GIT_NAME_MAX + 1];
        strcpy(name, g->name);
        more = work_slice(g->r, name, mk, &packed, &merged, &blocked);
        if(!more) break;
        hub_beat();
        if((int32_t)(furi_get_tick() - end) >= 0) break;
    }
    if(blocked && packed == 0 && merged == 0) {
        hub_send_error(g->r, 503, "low memory, try again");
        return;
    }
    if(!more && !blocked) pack_unmark(g->name);
    char out[110];
    int n = snprintf(
        out,
        sizeof(out),
        "{\"ok\":true,\"packed\":%u,\"merged\":%u,\"remaining\":%u,\"more\":%s}",
        packed,
        merged,
        more ? 1u : 0u,
        more ? "true" : "false");
    hub_send_body(g->r, 200, "application/json", out, (size_t)n);
}

/* ---------- temp file cleanup at start ---------- */

/* Delete "*.part" and "tmp_pack_*" files directly inside one folder. The names
 * are collected first: a folder is not changed while it is being read. */
static void clean_dir(Storage* storage, const char* path) {
    FuriString* names = furi_string_alloc();
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, path)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(file_info_is_dir(&info)) continue;
            if((len > 5 && strcmp(name + len - 5, ".part") == 0) || strncmp(name, "tmp_pack_", 9) == 0) {
                furi_string_cat_printf(names, "%s\n", name);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    char full[GIT_PATH_MAX + 140];
    const char* p = furi_string_get_cstr(names);
    while(*p) {
        size_t n = strcspn(p, "\n");
        if(n > 0 && n < 128) {
            snprintf(full, sizeof(full), "%s/%.*s", path, (int)n, p);
            storage_common_remove(storage, full);
        }
        p += n + (p[n] ? 1 : 0);
    }
    furi_string_free(names);
}

void hub_git_cleanup_temp(Storage* storage) {
    static const char* const sub[] = {
        "", "/info", "/objects/info", "/objects/pack", "/refs/heads", "/refs/tags", "/hub"};
    File* dir = storage_file_alloc(storage);
    FuriString* repos = furi_string_alloc();
    if(storage_dir_open(dir, GIT_ROOT)) {
        FileInfo info;
        char name[128];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(file_info_is_dir(&info) && len >= 5 && strcmp(name + len - 4, ".git") == 0) {
                furi_string_cat_printf(repos, "%s\n", name);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    char path[GIT_PATH_MAX];
    const char* p = furi_string_get_cstr(repos);
    while(*p) {
        size_t n = strcspn(p, "\n");
        if(n > 0 && n < 100) {
            hub_beat();
            snprintf(path, sizeof(path), GIT_ROOT "/%.*s/hub/tmp", (int)n, p);
            storage_simply_remove_recursive(storage, path);
            for(size_t i = 0; i < COUNT_OF(sub); i++) {
                snprintf(path, sizeof(path), GIT_ROOT "/%.*s%s", (int)n, p, sub[i]);
                clean_dir(storage, path);
            }
        }
        p += n + (p[n] ? 1 : 0);
    }
    furi_string_free(repos);
}

/* ---------- dispatcher ---------- */

bool hub_git_route(HubReq* r) {
    bool file = strncmp(r->path, "/git/", 5) == 0;
    bool api = strncmp(r->path, "/api/git/", 9) == 0;
    if(!file && !api) return false;

    bool get = strcmp(r->method, "GET") == 0;
    bool post = strcmp(r->method, "POST") == 0;
    if(file && !get) {
        static const char* const dav_methods[] = {
            "OPTIONS", "HEAD", "PROPFIND", "MKCOL", "PUT", "MOVE", "DELETE", "LOCK", "UNLOCK"};
        for(size_t i = 0; i < sizeof(dav_methods) / sizeof(dav_methods[0]); i++) {
            if(strcmp(r->method, dav_methods[i]) == 0) return hub_plugin_dispatch(r, HUB_PL_DAV);
        }
        return false;
    }

    if(api) {
        HubPluginId pl = hub_plugin_for(r);
        if(pl != HUB_PL_COUNT) return hub_plugin_dispatch(r, pl);
        if(hub_gitview_route(r)) return true;
    }

    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(g, 0, sizeof(GitCtx));
    g->r = r;

    if(file) {
        git_route_file(r, g);
    } else {
        const char* name = r->path + 9;
        storage_simply_mkdir(r->storage, GIT_ROOT);
        if(strcmp(name, "repos") == 0 && get) {
            git_route_repos(g);
        } else if(strcmp(name, "refs") == 0 && get) {
            git_route_refs(g);
        } else if(post && strcmp(name, "create") == 0) {
            git_route_create(g);
        } else if(post && strcmp(name, "delete") == 0) {
            git_route_delete(g);
        } else if(post && strcmp(name, "rename") == 0) {
            git_route_rename(g);
        } else if(post && strcmp(name, "describe") == 0) {
            git_route_describe(g);
        } else if(post && strcmp(name, "ref") == 0) {
            git_route_ref(g);
        } else if(post && strcmp(name, "head") == 0) {
            git_route_head(g);
        } else if(post && strcmp(name, "pack") == 0) {
            git_route_pack(g);
        } else if(post && strcmp(name, "server-info") == 0) {
            git_route_server_info(g);
        } else if(strcmp(name, "repos") == 0 || strcmp(name, "refs") == 0 || strcmp(name, "create") == 0 ||
                  strcmp(name, "delete") == 0 || strcmp(name, "rename") == 0 ||
                  strcmp(name, "describe") == 0 || strcmp(name, "ref") == 0 ||
                  strcmp(name, "head") == 0 || strcmp(name, "server-info") == 0 ||
                  strcmp(name, "pack") == 0) {
            hub_send_error(r, 405, "method not allowed");
        } else {
            hub_send_error(r, 404, "not found");
        }
    }
    free(g);
    return true;
}

/* ---------- for hub_dav.c ---------- */

void hub_git_mark_clean(const char* name) {
    if(strcmp(git_dirty, name) == 0) git_dirty[0] = '\0';
}

void hub_git_mark_dirty(HubReq* r, const char* name) {
    if(git_dirty[0] && strcmp(git_dirty, name) != 0) {
        char old[GIT_NAME_MAX + 1];
        strcpy(old, git_dirty);
        git_dirty[0] = '\0';
        hub_git_changed(r, old);
    }
    snprintf(git_dirty, sizeof(git_dirty), "%s", name);
}


bool hub_git_name_ok(const char* name) {
    return git_name_ok(name);
}

/* Regenerate info/refs and objects/info/packs of a repo. */
bool hub_git_changed(HubReq* r, const char* name) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return false;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    bool ok = git_write_info_refs(g) && git_write_info_packs(g);
    free(g);
    return ok;
}

/* A ref was deleted by file: take it out of packed-refs too. */
bool hub_git_drop_packed(HubReq* r, const char* name, const char* ref) {
    GitCtx* g = malloc(sizeof(GitCtx));
    if(!g) return false;
    memset(g, 0, sizeof(GitCtx));
    g->r = r;
    git_set_base(g, name);
    bool ok = git_packed_drop(g, ref);
    free(g);
    return ok;
}

void hub_git_flush_r(HubReq* r) {
    if(!git_dirty[0]) return;
    char name[GIT_NAME_MAX + 1];
    strcpy(name, git_dirty);
    git_dirty[0] = '\0';
    hub_git_changed(r, name);
}

/* For the worker shutdown: there is no request, so make a bare one. */
void hub_git_flush(Storage* storage) {
    if(!git_dirty[0]) return;
    HubReq* r = malloc(sizeof(HubReq));
    if(!r) return; /* the next start regenerates it */
    memset(r, 0, sizeof(HubReq));
    r->storage = storage;
    hub_git_flush_r(r);
    free(r);
}
