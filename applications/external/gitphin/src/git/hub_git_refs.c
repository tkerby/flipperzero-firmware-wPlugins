/* Git routes: refs, packed-refs, info/refs and objects/info/packs.
 * Part of hub_git.c: included there, in this order, not built alone. */
#include "hub_git_int.h"

/* ---------- refs ---------- */

static void git_refs_free(GitRefs* l) {
    for(size_t i = 0; i < l->n; i++)
        free(l->v[i].name);
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
                       (strncmp(nm, "refs/heads/", 11) == 0 ||
                        strncmp(nm, "refs/tags/", 10) == 0) &&
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
    bool ok = git_mkdirs(g, g->path) &&
              git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
    furi_string_free(s);
    return ok;
}

/* objects/info/packs: "P pack-<hash>.pack\n" per pack, then an empty line */
static bool git_write_info_packs(GitCtx* g) {
    FuriString* s = furi_string_alloc();
    git_packs(g, s);
    furi_string_push_back(s, '\n');

    snprintf(g->path, sizeof(g->path), "%s/objects/info/packs", g->base);
    bool ok = git_mkdirs(g, g->path) &&
              git_write(g, g->path, furi_string_get_cstr(s), furi_string_size(s));
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
            for(size_t i = 0; i < len; i++)
                furi_string_push_back(out, p[i]);
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
