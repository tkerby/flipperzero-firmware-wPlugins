#define HUB_PLUGIN_BUILD
#include "hub_gitext.h"
#include "hub_plugin.h"
#include "hub_git.h"
#include "hub_gitobj.h"
#include "hub_gitdiff.h"
#include "hub_gitcommit.h"
#include "hub_gitsearch.h"
#include "hub_gitwrite.h"

#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hub_mem.h"

#define EXT_ROOT     "/ext/git"
#define EXT_MAX_OBJ  (16 * 1024)
#define EXT_DIFF_OBJ (12 * 1024)
#define EXT_CF_OBJ \
    (12 * 1024) /* commit-file: 2-3 trees at once */ /* diff holds two trees at once */
#define EXT_DEADLINE_MS 60000 /* RAM limit for one object */
#define EXT_MERGE_OBJ   (8 * 1024) /* merge holds 3 trees + output at once */
#define EXT_COMPARE_CAP 500
#define EXT_BODY_MAX    (64 * 1024)
#define EXT_MSG_MAX     1000
#define EXT_SEARCH_MAX  200
#define EXT_SEARCH_MS   20000
#define EXT_SEARCH_BLOB (64 * 1024) /* bigger files are not searched */
#define EXT_JSON_CAP    (24 * 1024) /* stop adding items above this many JSON bytes */
#define EXT_LOG_MSG     1024
#define EXT_LOG_OBJ     (8 * 1024) /* commit objects the log reads */
#define EXT_LOG_MAX     50
#define EXT_COMMIT_MSG  (16 * 1024)
#define EXT_ZERO        "0000000000000000000000000000000000000000"
#define EXT_REF_MAX     180

typedef struct {
    HubReq* r;
    HubGit* g;
    char name[80];
    char base[96]; /* /ext/git/<name>.git */
    char p1[210];
    char p2[210];
    char p3[210];
    char msg[EXT_MSG_MAX + 8];
    char author[100];
    char email[120];
    char path[320];
    char tmp[330];
    char ref[232];
    char nref[232];
    char nbranch[210];
    char action[12];
    char dm[260];
    char hexa[41];
    char hexb[41];
} Ext;

/* ---------- JSON text ---------- */

#define ext_jbytes hub_json_bytes

static __attribute__((unused)) void ext_jstr(FuriString* s, const char* v) {
    ext_jbytes(s, v, strlen(v));
}

/* ---------- common ---------- */

static __attribute__((unused)) void ext_err(Ext* e, int rc) {
    HubReq* r = e->r;
    switch(rc) {
    case HUB_GIT_ERR_NOTFOUND:
        hub_send_error(r, 404, "not found");
        break;
    case HUB_GIT_ERR_TOOMANY:
        hub_send_error(r, 503, "repository is being reorganised, try again in a few minutes");
        break;
    case HUB_GIT_ERR_TOOLARGE:
        hub_send_error(r, 413, "object too large");
        break;
    case HUB_GIT_ERR_BADARG:
    case HUB_GC_ERR_BADPATH:
        hub_send_error(r, 400, "bad path or argument");
        break;
    case HUB_GC_ERR_CONFLICT:
        hub_send_error(r, 409, "path conflicts with an existing file or folder");
        break;
    case HUB_GC_ERR_UNCHANGED:
        hub_send_error(r, 409, "nothing changed");
        break;
    case HUB_GIT_ERR_NOMEM:
        hub_send_error(r, 503, "low memory, try again");
        break;
    case HUB_GIT_ERR_CORRUPT:
        hub_send_error(r, 500, "corrupt repository");
        break;
    default:
        hub_send_error(r, 500, "storage error");
        break;
    }
}

static __attribute__((unused)) bool ext_sha_ok(const char* s) {
    for(size_t i = 0; i < 40; i++) {
        char c = s[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return s[40] == '\0';
}

/* Full ref under refs/heads/ or refs/tags/, with the git rules (same as hub_git.c). */
static __attribute__((unused)) bool ext_ref_ok(const char* ref) {
    size_t len = strlen(ref);
    size_t pre;
    if(strncmp(ref, "refs/heads/", 11) == 0) {
        pre = 11;
    } else if(strncmp(ref, "refs/tags/", 10) == 0) {
        pre = 10;
    } else {
        return false;
    }
    if(len == pre || len > EXT_REF_MAX) return false;
    if(ref[len - 1] == '/' || ref[len - 1] == '.') return false;
    if(ref[pre] == '.') return false;
    for(size_t i = 0; i < len; i++) {
        char c = ref[i];
        if((unsigned char)c <= 0x20 || c == 0x7f || strchr("~^:?*[\\", c)) return false;
        char n = ref[i + 1];
        if(c == '.' && n == '.') return false;
        if(c == '/' && (n == '/' || n == '.')) return false;
        if(c == '@' && n == '{') return false;
        if((c == '/' || n == '\0') && i >= 4) {
            const char* end = (n == '\0') ? ref + i + 1 : ref + i;
            if(strncmp(end - 5, ".lock", 5) == 0) return false;
        }
    }
    return true;
}

/* Query value: 1 = found, 0 = missing, -1 = too long (error sent). */
static __attribute__((unused)) int ext_q(Ext* e, const char* key, char* out, size_t size) {
    if(!hub_query_get(e->r->query, key, out, size)) {
        out[0] = '\0';
        return 0;
    }
    if(strlen(out) + 1 >= size) {
        hub_send_error(e->r, 400, "parameter too long");
        return -1;
    }
    return 1;
}

/* Required, non-empty parameter. Sends the error itself. */
static __attribute__((unused)) bool ext_need(Ext* e, const char* key, char* out, size_t size) {
    int q = ext_q(e, key, out, size);
    if(q < 0) return false;
    if(q == 0 || out[0] == '\0') {
        char m[40];
        snprintf(m, sizeof(m), "missing %s", key);
        hub_send_error(e->r, 400, m);
        return false;
    }
    return true;
}

/* Check "name", open the repository. Sends the error itself. */
static __attribute__((unused)) bool ext_open(Ext* e, size_t limit, size_t need) {
    if(!hub_heap_ok(e->r, need)) return false;
    if(!ext_need(e, "name", e->name, sizeof(e->name))) return false;
    if(!hub_git_name_ok(e->name)) {
        hub_send_error(e->r, 400, "bad repo name");
        return false;
    }
    snprintf(e->base, sizeof(e->base), EXT_ROOT "/%s.git", e->name);
    FileInfo info;
    if(storage_common_stat(e->r->storage, e->base, &info) != FSE_OK || !file_info_is_dir(&info)) {
        hub_send_error(e->r, 404, "repository not found");
        return false;
    }
    e->g = hub_git_open(e->r->storage, e->base);
    if(!e->g) {
        hub_send_error(e->r, 500, "cannot open repository");
        return false;
    }
    hub_git_set_limit(e->g, limit);
    return true;
}

static __attribute__((unused)) bool ext_resolve(Ext* e, const char* ref, HubOid* oid) {
    int rc = hub_git_resolve(e->g, ref, oid);
    if(rc == HUB_GIT_OK) return true;
    if(rc == HUB_GIT_ERR_NOTFOUND) {
        hub_send_error(e->r, 404, "unknown ref or commit");
    } else {
        ext_err(e, rc);
    }
    return false;
}

/* The commit of a ref ("refs/heads/x"), or ZERO-like not found. */
static __attribute__((unused)) int ext_ref_value(Ext* e, const char* ref, HubOid* oid) {
    int rc = hub_git_ref_read(e->g, ref, oid);
    return rc;
}

/* Write the loose ref file via .part. */
static __attribute__((unused)) bool ext_ref_write(Ext* e, const char* ref, const char* hex) {
    Storage* st = e->r->storage;
    snprintf(e->path, sizeof(e->path), "%s/%s", e->base, ref);
    snprintf(e->tmp, sizeof(e->tmp), "%s", e->path);
    for(char* p = e->tmp + strlen(e->base) + 1; *p; p++) {
        if(*p != '/') continue;
        *p = '\0';
        bool ok = storage_simply_mkdir(st, e->tmp);
        *p = '/';
        if(!ok) return false;
    }
    snprintf(e->tmp, sizeof(e->tmp), "%s.part", e->path);
    File* f = storage_file_alloc(st);
    bool ok = storage_file_open(f, e->tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        ok = storage_file_write(f, hex, 40) == 40 && storage_file_write(f, "\n", 1) == 1;
    }
    storage_file_close(f);
    storage_file_free(f);
    if(ok) {
        storage_common_remove(st, e->path);
        ok = storage_common_rename(st, e->tmp, e->path) == FSE_OK;
    }
    if(!ok) storage_common_remove(st, e->tmp);
    return ok;
}

/* Compare-and-swap of a ref: expected is a 40 hex sha or ZERO (must not exist). */
static __attribute__((unused)) bool
    ext_ref_cas(Ext* e, const char* ref, const char* expected, const char* value) {
    HubOid cur;
    char hex[41];
    int rc = ext_ref_value(e, ref, &cur);
    if(rc == HUB_GIT_OK) {
        hub_oid_to_hex(&cur, hex);
    } else if(rc == HUB_GIT_ERR_NOTFOUND) {
        strcpy(hex, EXT_ZERO);
    } else {
        ext_err(e, rc);
        return false;
    }
    if(strcmp(hex, expected) != 0) {
        hub_send_error(e->r, 409, "branch changed");
        return false;
    }
    if(!ext_ref_write(e, ref, value)) {
        hub_send_error(e->r, 409, "ref conflicts with another ref");
        return false;
    }
    return true;
}

#ifdef HUB_EXT_QUERY
static void ext_chunk_ident(HubChunk* c, FuriString* t, const HubGitIdent* id) {
    hub_chunk_lit(c, "{\"name\":");
    hub_chunk_json(c, id->name ? id->name : "", id->name ? id->name_len : 0);
    hub_chunk_lit(c, ",\"email\":");
    hub_chunk_json(c, id->email ? id->email : "", id->email ? id->email_len : 0);
    furi_string_printf(t, ",\"time\":%lld,\"tz\":\"%s\"}", (long long)id->time, id->tz);
    hub_chunk_fs(c, t);
}

/* One commit as JSON "C", written straight to the chunks (t is a small scratch string). */
static void ext_chunk_commit(
    HubChunk* c,
    FuriString* t,
    const HubOid* oid,
    const HubGitCommit* cm,
    size_t msg_cap,
    bool open_brace) {
    char hex[41], hex2[41];
    hub_oid_to_hex(oid, hex);
    hub_oid_to_hex(&cm->tree, hex2);
    furi_string_printf(
        t, "%s\"sha\":\"%s\",\"tree\":\"%s\",\"parents\":[", open_brace ? "{" : "", hex, hex2);
    unsigned np = cm->nparents < HUB_GIT_MAX_PARENTS ? cm->nparents : HUB_GIT_MAX_PARENTS;
    for(unsigned i = 0; i < np; i++) {
        hub_oid_to_hex(&cm->parent[i], hex);
        furi_string_cat_printf(t, "%s\"%s\"", i ? "," : "", hex);
    }
    furi_string_cat_str(t, "],\"author\":");
    hub_chunk_fs(c, t);
    ext_chunk_ident(c, t, &cm->author);
    hub_chunk_lit(c, ",\"committer\":");
    ext_chunk_ident(c, t, &cm->committer);
    size_t n = cm->msg_len;
    if(n > msg_cap) {
        n = msg_cap;
        while(n > 0 && ((unsigned char)cm->msg[n] & 0xC0) == 0x80)
            n--; /* not inside a character */
    }
    hub_chunk_lit(c, ",\"message\":");
    hub_chunk_json(c, cm->msg ? cm->msg : "", cm->msg ? n : 0);
    hub_chunk_lit(c, "}");
}

/* Read a commit object and parse it. data must be freed with hub_git_free. */
static int ext_read_commit(Ext* e, const HubOid* oid, uint8_t** data, HubGitCommit* c) {
    uint8_t type;
    size_t size;
    int rc = hub_git_read(e->g, oid, &type, data, &size);
    if(rc != HUB_GIT_OK) return rc;
    if(type != HUB_OBJ_COMMIT)
        rc = HUB_GIT_ERR_BADARG;
    else
        rc = hub_git_parse_commit(*data, size, c);
    if(rc != HUB_GIT_OK) {
        hub_git_free(*data);
        *data = NULL;
    }
    return rc;
}

/* ref (or sha) parameter into p1, HEAD when there is none; resolved to a commit */
static bool ext_ref_param(Ext* e, const char* k1, const char* k2, HubOid* oid) {
    int q = ext_q(e, k1, e->p1, sizeof(e->p1));
    if(q == 0) q = ext_q(e, k2, e->p1, sizeof(e->p1));
    if(q < 0) return false;
    if(q == 0 || e->p1[0] == '\0') strcpy(e->p1, "HEAD");
    return ext_resolve(e, e->p1, oid);
}

/* Are p1 and p2 both full ids? Then the answer for them never changes. */
static bool ext_both_ids(Ext* e) {
    HubOid t;
    return hub_oid_from_hex(e->p1, &t) && hub_oid_from_hex(e->p2, &t);
}

/* ---------- diff ---------- */

typedef struct {
    HubChunk* ck;
    FuriString* line;
    bool first;
} DiffOut;

static void diff_sha(FuriString* s, const char* key, const HubOid* oid, uint32_t mode) {
    if(mode == 0) {
        furi_string_cat_printf(s, ",\"%s\":null", key);
    } else {
        char hex[41];
        hub_oid_to_hex(oid, hex);
        furi_string_cat_printf(s, ",\"%s\":\"%s\"", key, hex);
    }
}

static bool diff_cb(void* ctx, const HubGitDiffFile* f) {
    DiffOut* d = ctx;
    FuriString* s = d->line;
    furi_string_set_str(s, d->first ? "" : ",");
    furi_string_cat_str(s, "{\"path\":");
    ext_jstr(s, f->path);
    furi_string_cat_printf(s, ",\"status\":\"%c\"", f->status);
    diff_sha(s, "old_sha", &f->old_oid, f->old_mode);
    diff_sha(s, "new_sha", &f->new_oid, f->new_mode);
    if(f->old_mode) {
        furi_string_cat_printf(s, ",\"old_mode\":\"%06lo\"", (unsigned long)f->old_mode);
    } else {
        furi_string_cat_str(s, ",\"old_mode\":null");
    }
    if(f->new_mode) {
        furi_string_cat_printf(s, ",\"new_mode\":\"%06lo\"}", (unsigned long)f->new_mode);
    } else {
        furi_string_cat_str(s, ",\"new_mode\":null}");
    }
    d->first = false;
    hub_chunk_fs(d->ck, s);
    return d->ck->ok;
}

/* Send the diff of two trees as JSON, one chunked pass: only one file line is in RAM.
 * cache_key is what addresses the answer ("" = not an id). */
static void diff_send(
    Ext* e,
    const HubOid* base_commit,
    const HubOid* head_commit,
    const HubOid* ta,
    const HubOid* tb,
    const char* cache_key) {
    HubChunk* ck = malloc(sizeof(HubChunk));
    FuriString* line = furi_string_alloc();
    if(!ck) {
        furi_string_free(line);
        return hub_send_error(e->r, 503, "low memory, try again");
    }
    if(hub_chunk_begin(ck, e->r, "application/json", hub_cache_hdr(cache_key))) {
        char hex[41];
        furi_string_set_str(line, "{\"ok\":true,\"base\":");
        if(base_commit) {
            hub_oid_to_hex(base_commit, hex);
            furi_string_cat_printf(line, "\"%s\"", hex);
        } else {
            furi_string_cat_str(line, "null");
        }
        hub_oid_to_hex(head_commit, hex);
        furi_string_cat_printf(line, ",\"head\":\"%s\",\"files\":[", hex);
        hub_chunk_fs(ck, line);
        DiffOut d = {.ck = ck, .line = line, .first = true};
        bool trunc = false;
        int rc = hub_git_diff_trees(e->g, ta, tb, diff_cb, &d, HUB_GD_MAX_FILES, &trunc);
        const char* tail = trunc ? "],\"truncated\":true}" : "],\"truncated\":false}";
        if(rc == HUB_GIT_OK) hub_chunk_write(ck, tail, strlen(tail));
        hub_chunk_end(ck, rc == HUB_GIT_OK && ck->ok);
    }
    furi_string_free(line);
    free(ck);
}

/* tree of a commit's first parent (NULL for a root commit) */
static int ext_first_parent(Ext* e, const HubOid* commit, HubOid* parent, bool* has) {
    HubCommitMeta m;
    int rc = hub_git_commit_meta(e->g, commit, &m);
    *has = rc == HUB_GIT_OK && m.nparents > 0;
    if(*has) *parent = m.parent[0];
    return rc;
}

static void route_diff(Ext* e) {
    if(!ext_open(e, EXT_DIFF_OBJ, 32 * 1024)) return;
    HubOid head, base, ta, tb;
    bool has_base = true;
    int q = ext_q(e, "sha", e->p1, sizeof(e->p1));
    if(q < 0) return;
    if(q == 1 && e->p1[0]) {
        if(!ext_resolve(e, e->p1, &head)) return;
        int rc = ext_first_parent(e, &head, &base, &has_base);
        if(rc != HUB_GIT_OK) return ext_err(e, rc);
    } else {
        if(!ext_need(e, "base", e->p1, sizeof(e->p1)) ||
           !ext_need(e, "head", e->p2, sizeof(e->p2)))
            return;
        if(!ext_resolve(e, e->p1, &base) || !ext_resolve(e, e->p2, &head)) return;
    }
    int rc = hub_git_tree_of(e->g, &head, &tb);
    if(rc == HUB_GIT_OK && has_base) rc = hub_git_tree_of(e->g, &base, &ta);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    const char* key = "";
    if(q == 1 && e->p1[0]) {
        key = e->p1; /* sha= : a full id never changes */
    } else if(ext_both_ids(e)) {
        key = e->p1;
    }
    diff_send(e, has_base ? &base : NULL, &head, has_base ? &ta : NULL, &tb, key);
}

/* ---------- compare ---------- */

static void route_compare(Ext* e) {
    if(!ext_open(e, EXT_MAX_OBJ, 30 * 1024))
        return; /* host peak 23.2 KB (cap 500 arrays, Ext) +25 % */
    HubOid base, head;
    if(!ext_need(e, "base", e->p1, sizeof(e->p1)) || !ext_need(e, "head", e->p2, sizeof(e->p2)))
        return;
    if(!ext_resolve(e, e->p1, &base) || !ext_resolve(e, e->p2, &head)) return;
    HubGitCompare* c = calloc(1, sizeof(HubGitCompare));
    if(!c) return hub_send_error(e->r, 503, "low memory, try again");
    c->cap = EXT_COMPARE_CAP;
    int rc = hub_git_compare(e->g, &base, &head, c);
    if(rc != HUB_GIT_OK) {
        free(c);
        return ext_err(e, rc);
    }
    char hex[41];
    HubChunk* ck = malloc(sizeof(HubChunk));
    HubGitCommit* cm = malloc(sizeof(HubGitCommit));
    FuriString* s = furi_string_alloc_set("{\"ok\":true,\"base\":");
    if(!ck || !cm ||
       !hub_chunk_begin(
           ck, e->r, "application/json", hub_cache_hdr(ext_both_ids(e) ? e->p1 : ""))) {
        if(!ck || !cm) hub_send_error(e->r, 503, "low memory, try again");
        furi_string_free(s);
        free(cm);
        free(ck);
        free(c);
        return;
    }
    hub_oid_to_hex(&base, hex);
    furi_string_cat_printf(s, "\"%s\",\"head\":\"", hex);
    hub_oid_to_hex(&head, hex);
    furi_string_cat_printf(s, "%s\",\"merge_base\":", hex);
    if(c->has_merge_base) {
        hub_oid_to_hex(&c->merge_base, hex);
        furi_string_cat_printf(s, "\"%s\"", hex);
    } else {
        furi_string_cat_str(s, "null");
    }
    furi_string_cat_str(s, ",\"ahead\":[");
    hub_chunk_fs(ck, s); /* the head part goes out before the commits reuse s */
    bool first = true;
    for(unsigned i = 0; i < c->n_ahead && ck->ok; i++) {
        uint8_t type;
        uint8_t* data;
        size_t size;
        if(hub_git_read(e->g, &c->ahead[i], &type, &data, &size) != HUB_GIT_OK) continue;
        if(type == HUB_OBJ_COMMIT && hub_git_parse_commit(data, size, cm) == HUB_GIT_OK) {
            furi_string_set_str(s, first ? "" : ",");
            first = false;
            hub_chunk_fs(ck, s); /* the comma */
            ext_chunk_commit(ck, s, &c->ahead[i], cm, EXT_LOG_MSG, true);
        }
        hub_git_free(data);
    }
    free(cm);
    furi_string_printf(
        s,
        "],\"ahead_count\":%lu,\"behind_count\":%lu,\"ff\":%s,\"capped\":%s}",
        (unsigned long)c->ahead_count,
        (unsigned long)c->behind_count,
        c->ff ? "true" : "false",
        c->capped ? "true" : "false");
    free(c);
    hub_chunk_fs(ck, s);
    hub_chunk_end(ck, ck->ok);
    furi_string_free(s);
    free(ck);
}

/* ---------- search ---------- */

typedef struct {
    HubChunk* ck;
    FuriString* s; /* one hit at a time */
    uint32_t start;
    bool first;
} SearchOut;

static bool search_hit(void* ctx, const HubGitHit* h) {
    SearchOut* o = ctx;
    furi_string_set_str(o->s, o->first ? "" : ",");
    o->first = false;
    furi_string_cat_str(o->s, "{\"path\":");
    ext_jstr(o->s, h->path);
    furi_string_cat_printf(o->s, ",\"line\":%lu,\"text\":", (unsigned long)h->line);
    ext_jbytes(o->s, h->text, h->text_len);
    furi_string_push_back(o->s, '}');
    hub_chunk_fs(o->ck, o->s);
    return o->ck->ok;
}

static bool search_stop(void* ctx) {
    SearchOut* o = ctx;
    return (furi_get_tick() - o->start) > furi_ms_to_ticks(EXT_SEARCH_MS);
}

static void route_search(Ext* e) {
    if(!ext_open(e, EXT_MAX_OBJ, 30 * 1024)) return; /* streamed: no hit list in RAM */
    int q = ext_q(e, "q", e->p2, sizeof(e->p2));
    if(q < 0) return;
    if(q == 0 || e->p2[0] == '\0' || strlen(e->p2) > HUB_GS_QUERY_MAX) {
        hub_send_error(e->r, 400, "q must be 1 to 100 characters");
        return;
    }
    q = ext_q(e, "ref", e->p1, sizeof(e->p1));
    if(q < 0) return;
    if(q == 0 || e->p1[0] == '\0') strcpy(e->p1, "HEAD");
    size_t max = 50;
    if(ext_q(e, "max", e->p3, sizeof(e->p3)) > 0) {
        long m = strtol(e->p3, NULL, 10);
        if(m > 0) max = (size_t)m;
    }
    if(max > EXT_SEARCH_MAX) max = EXT_SEARCH_MAX;
    HubOid commit, tree;
    if(!ext_resolve(e, e->p1, &commit)) return;
    int rc = hub_git_tree_of(e->g, &commit, &tree);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);

    SearchOut o = {
        .ck = malloc(sizeof(HubChunk)),
        .s = furi_string_alloc(),
        .start = furi_get_tick(),
        .first = true};
    HubGitSearch* s = calloc(1, sizeof(HubGitSearch));
    if(!s || !o.ck || !hub_chunk_begin(o.ck, e->r, "application/json", NULL)) {
        if(!s || !o.ck) hub_send_error(e->r, 503, "low memory, try again");
        furi_string_free(o.s);
        free(o.ck);
        free(s);
        return;
    }
    hub_chunk_lit(o.ck, "{\"ok\":true,\"hits\":[");
    s->query = e->p2;
    s->max_hits = max;
    s->max_blob = EXT_SEARCH_BLOB;
    s->on_hit = search_hit;
    s->hit_ctx = &o;
    s->should_stop = search_stop;
    s->stop_ctx = &o;
    rc = hub_git_search(e->g, &tree, s);
    if(rc == HUB_GIT_OK) {
        furi_string_printf(
            o.s,
            "],\"truncated\":%s,\"timed_out\":%s,\"scanned\":%lu}",
            s->truncated ? "true" : "false",
            s->timed_out ? "true" : "false",
            (unsigned long)s->scanned);
        hub_chunk_fs(o.ck, o.s);
    }
    hub_chunk_end(o.ck, rc == HUB_GIT_OK && o.ck->ok);
    free(s);
    furi_string_free(o.s);
    free(o.ck);
}

#endif /* HUB_EXT_QUERY */
#ifdef HUB_EXT_WRITE
/* ---------- commit-file ---------- */

/* Read the whole body into a malloc'd buffer (size + 1). NULL on error (sent). */
static uint8_t* ext_body(Ext* e, size_t* len) {
    HubReq* r = e->r;
    if(r->content_length > EXT_BODY_MAX) {
        hub_send_error(r, 413, "file too large (max 64 KB)");
        return NULL;
    }
    uint8_t* buf = malloc(r->content_length + 1);
    if(!buf) {
        hub_send_error(r, 503, "low memory, try again");
        return NULL;
    }
    size_t got = 0;
    while(got < r->content_length) {
        size_t n = hub_body_read(r, buf + got, r->content_length - got);
        if(n == 0) break;
        got += n;
    }
    if(got != r->content_length) {
        free(buf);
        hub_send_error(r, 400, "upload was cut off");
        return NULL;
    }
    buf[got] = '\0';
    *len = got;
    return buf;
}

/* author / email / message from the query, with defaults */
static bool ext_ident_params(Ext* e, const char* default_msg) {
    int q = ext_q(e, "author", e->author, sizeof(e->author));
    if(q < 0) return false;
    if(q == 0 || e->author[0] == '\0') strcpy(e->author, "me");
    q = ext_q(e, "email", e->email, sizeof(e->email));
    if(q < 0) return false;
    if(q == 0 || e->email[0] == '\0') strcpy(e->email, "me@flipper.local");
    q = ext_q(e, "message", e->msg, sizeof(e->msg));
    if(q < 0) return false;
    if(q == 0 || e->msg[0] == '\0') snprintf(e->msg, sizeof(e->msg), "%s", default_msg);
    return true;
}

static void route_commit_file(Ext* e) {
    HubReq* r = e->r;
    if(!ext_open(e, EXT_CF_OBJ, 42 * 1024)) return;
    if(!hub_sd_ok(r, (uint64_t)r->content_length + 256 * 1024)) return;
    /* p1 = branch, p2 = old, p3 = path; the new branch name goes to e->tmp (not used by the ref writer until later) */
    if(!ext_need(e, "branch", e->p1, sizeof(e->p1)) || !ext_need(e, "old", e->p2, sizeof(e->p2)) ||
       !ext_need(e, "path", e->p3, sizeof(e->p3)))
        return;
    int q = ext_q(e, "action", e->action, sizeof(e->action));
    if(q < 0) return;
    if(q == 0 || e->action[0] == '\0') strcpy(e->action, "put");
    bool del = strcmp(e->action, "delete") == 0;
    if(!del && strcmp(e->action, "put") != 0)
        return hub_send_error(r, 400, "action must be put or delete");
    q = ext_q(e, "new_branch", e->nbranch, sizeof(e->nbranch));
    if(q < 0) return;
    bool create = q == 1 && e->nbranch[0];
    if(!ext_sha_ok(e->p2)) return hub_send_error(r, 400, "bad sha in old");
    snprintf(e->dm, sizeof(e->dm), "%s %.200s", del ? "Delete" : "Update", e->p3);
    if(!ext_ident_params(e, e->dm)) return;

    char* ref = e->ref;
    char* nref = e->nref;
    snprintf(ref, sizeof(e->ref), "refs/heads/%s", e->p1);
    if(!ext_ref_ok(ref)) return hub_send_error(r, 400, "bad branch name");
    if(create) {
        snprintf(nref, sizeof(e->nref), "refs/heads/%s", e->nbranch);
        if(!ext_ref_ok(nref)) return hub_send_error(r, 400, "bad new branch name");
    }

    size_t blen = 0;
    uint8_t* body = ext_body(e, &blen);
    if(!body) return;

    /* the branch must still be at "old" (also the base of a new branch: the page
     * edited that version) */
    bool root = strcmp(e->p2, EXT_ZERO) == 0;
    HubOid cur, tip;
    char* hex = e->hexa;
    int rc = ext_ref_value(e, ref, &cur);
    if(rc == HUB_GIT_OK) {
        hub_oid_to_hex(&cur, hex);
    } else if(rc == HUB_GIT_ERR_NOTFOUND) {
        strcpy(hex, EXT_ZERO);
    } else {
        free(body);
        return ext_err(e, rc);
    }
    if(!create && strcmp(hex, e->p2) != 0) {
        free(body);
        return hub_send_error(r, 409, "branch changed");
    }
    if(!root && !hub_oid_from_hex(e->p2, &tip)) {
        free(body);
        return hub_send_error(r, 400, "bad sha in old");
    }

    HubGitWriteIo io;
    hub_gitwrite_io_furi(&io, r->storage);
    HubGwIdent who = {e->author, e->email, (int64_t)furi_hal_rtc_get_timestamp(), "+0000"};
    HubOid commit;
    rc = hub_git_commit_file(
        e->g,
        &io,
        e->base,
        root ? NULL : &tip,
        e->p3,
        del ? NULL : body,
        blen,
        e->msg,
        &who,
        &commit,
        NULL);
    free(body);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);

    char* chex = e->hexb;
    hub_oid_to_hex(&commit, chex);
    if(!ext_ref_cas(e, create ? nref : ref, create ? EXT_ZERO : e->p2, chex)) return;
    hub_git_changed(r, e->name);

    FuriString* s = furi_string_alloc_printf("{\"ok\":true,\"commit\":\"%s\",\"branch\":", chex);
    ext_jstr(s, create ? e->nbranch : e->p1);
    furi_string_push_back(s, '}');
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

/* ---------- merge ---------- */

static bool merge_path(void* ctx, const char* path) {
    FuriString* s = ctx;
    if(furi_string_size(s) > EXT_JSON_CAP) return false;
    if(furi_string_get_char(s, furi_string_size(s) - 1) != '[') furi_string_push_back(s, ',');
    ext_jstr(s, path);
    return true;
}

static void route_merge(Ext* e) {
    HubReq* r = e->r;
    if(!ext_open(e, EXT_MAX_OBJ, 35 * 1024)) return;
    if(!ext_need(e, "base", e->p1, sizeof(e->p1)) || !ext_need(e, "head", e->p2, sizeof(e->p2)))
        return;
    char* ref = e->ref;
    if(strncmp(e->p1, "refs/heads/", 11) == 0) {
        snprintf(ref, sizeof(e->ref), "%s", e->p1);
    } else {
        snprintf(ref, sizeof(e->ref), "refs/heads/%s", e->p1);
    }
    if(!ext_ref_ok(ref)) return hub_send_error(r, 400, "bad base branch");
    snprintf(e->dm, sizeof(e->dm), "Merge %.100s into %.100s", e->p2, e->p1);
    if(!ext_ident_params(e, e->dm)) return;
    HubOid base, head;
    int rc = ext_ref_value(e, ref, &base);
    if(rc == HUB_GIT_ERR_NOTFOUND) return hub_send_error(r, 404, "base branch not found");
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    rc = hub_git_peel(e->g, &base);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    if(!ext_resolve(e, e->p2, &head)) return;

    HubGitCompare* c = calloc(1, sizeof(HubGitCompare));
    if(!c) return hub_send_error(r, 503, "low memory, try again");
    c->cap = EXT_COMPARE_CAP;
    rc = hub_git_compare(e->g, &base, &head, c);
    bool capped = c->capped, ff = c->ff, has_mb = c->has_merge_base;
    uint32_t ahead = c->ahead_count;
    HubOid mb = c->merge_base;
    free(c);
    if(rc != HUB_GIT_OK) return ext_err(e, rc);
    if(capped) return hub_send_error(r, 409, "history too long to merge on the device");
    if(ahead == 0) return hub_send_error(r, 409, "already up to date");

    char* bhex = e->hexa;
    char* chex = e->hexb;
    hub_oid_to_hex(&base, bhex);
    HubOid result;
    bool fast = ff;
    if(ff) {
        result = head; /* fast-forward: no merge commit */
    } else {
        FuriString* paths = furi_string_alloc_set("[");
        HubGitWriteIo io;
        hub_gitwrite_io_furi(&io, r->storage);
        HubGwIdent who = {e->author, e->email, (int64_t)furi_hal_rtc_get_timestamp(), "+0000"};
        unsigned nconf = 0;
        hub_git_set_limit(e->g, EXT_MERGE_OBJ);
        rc = hub_git_merge_commits(
            e->g,
            &io,
            e->base,
            &base,
            &head,
            has_mb ? &mb : NULL,
            e->msg,
            &who,
            &result,
            NULL,
            merge_path,
            paths,
            &nconf);
        hub_git_set_limit(e->g, EXT_MAX_OBJ);
        if(rc == HUB_GC_ERR_MERGE) {
            furi_string_cat_str(paths, "]");
            FuriString* s =
                furi_string_alloc_set("{\"ok\":false,\"error\":\"conflict\",\"paths\":");
            furi_string_cat(s, paths);
            furi_string_push_back(s, '}');
            hub_send_json(r, 409, s);
            furi_string_free(s);
            furi_string_free(paths);
            return;
        }
        furi_string_free(paths);
        if(rc != HUB_GIT_OK) return ext_err(e, rc);
    }
    hub_oid_to_hex(&result, chex);
    if(!ext_ref_cas(e, ref, bhex, chex)) return;
    hub_git_changed(r, e->name);
    FuriString* s = furi_string_alloc_printf(
        "{\"ok\":true,\"commit\":\"%s\",\"ff\":%s}", chex, fast ? "true" : "false");
    hub_send_json(r, 200, s);
    furi_string_free(s);
}

#endif /* HUB_EXT_WRITE */

/* ---------- log, commit, storage (streamed, the browse routes that live in this plugin) ---------- */

#ifdef HUB_EXT_QUERY
static void route_log(Ext* e) {
    if(!ext_open(e, EXT_LOG_OBJ, 20 * 1024))
        return; /* streamed: 8 KB object + 4.5 KB inflate + chunk + scratch */
    HubReq* r = e->r;
    HubOid oid;
    int rc;
    if(!ext_ref_param(e, "ref", "sha", &oid)) return;

    char num[12];
    unsigned skip = 0, n = 30;
    if(hub_query_get(r->query, "skip", num, sizeof(num))) skip = strtoul(num, NULL, 10);
    if(hub_query_get(r->query, "n", num, sizeof(num))) n = strtoul(num, NULL, 10);
    if(n < 1) n = 1;
    if(n > EXT_LOG_MAX) n = EXT_LOG_MAX;

    /* the commits that are skipped need only their first parent: the graph has it */
    HubCommitMeta* m = malloc(sizeof(HubCommitMeta));
    HubChunk* ck = malloc(sizeof(HubChunk));
    uint8_t* data = NULL;
    HubGitCommit* cm = malloc(sizeof(HubGitCommit));
    FuriString* t = furi_string_alloc();
    if(!m || !ck || !cm) {
        hub_send_error(r, 503, "low memory, try again");
        goto out;
    }
    bool end = false;
    for(unsigned i = 0; i < skip && !end; i++) {
        if(hub_git_commit_meta(e->g, &oid, m) != HUB_GIT_OK || m->nparents == 0) {
            end = true;
        } else {
            oid = m->parent[0];
        }
    }
    unsigned done = 0;
    bool more = false, started = false, good = true;
    while(!end && done < n) {
        rc = ext_read_commit(e, &oid, &data, cm);
        if(rc != HUB_GIT_OK) {
            if(!started && skip == 0) {
                ext_err(e, rc);
                goto out;
            }
            break; /* the page asks again */
        }
        if(!started) {
            if(!hub_chunk_begin(ck, r, "application/json", hub_cache_hdr(e->p1))) goto out;
            hub_chunk_lit(ck, "{\"ok\":true,\"commits\":[");
            started = true;
        }
        if(done) hub_chunk_lit(ck, ",");
        ext_chunk_commit(ck, t, &oid, cm, EXT_LOG_MSG, true);
        done++;
        bool has_parent = cm->nparents > 0;
        if(has_parent) oid = cm->parent[0];
        hub_git_free(data);
        data = NULL;
        more = has_parent;
        if(!has_parent) end = true;
        hub_beat();
    }
    if(!started) { /* skip went past the end of the history */
        if(!hub_chunk_begin(ck, r, "application/json", hub_cache_hdr(e->p1))) goto out;
        hub_chunk_lit(ck, "{\"ok\":true,\"commits\":[");
    }
    furi_string_printf(t, "],\"more\":%s,\"next_skip\":%u}", more ? "true" : "false", skip + done);
    hub_chunk_fs(ck, t);
    good = ck->ok;
    hub_chunk_end(ck, good);
out:
    furi_string_free(t);
    free(cm);
    free(ck);
    free(m);
}

static void route_commit(Ext* e) {
    if(!ext_open(e, 16 * 1024, 28 * 1024))
        return; /* 16 KB object + 4.5 KB inflate + chunk + scratch */
    HubOid oid;
    int rc;
    if(!ext_ref_param(e, "sha", "ref", &oid)) return;
    uint8_t* data;
    HubGitCommit* cm = malloc(sizeof(HubGitCommit));
    HubChunk* ck = malloc(sizeof(HubChunk));
    FuriString* t = furi_string_alloc();
    if(!cm || !ck) {
        hub_send_error(e->r, 503, "low memory, try again");
    } else if((rc = ext_read_commit(e, &oid, &data, cm)) != HUB_GIT_OK) {
        ext_err(e, rc);
    } else {
        if(hub_chunk_begin(ck, e->r, "application/json", hub_cache_hdr(e->p1))) {
            hub_chunk_lit(ck, "{\"ok\":true,");
            ext_chunk_commit(ck, t, &oid, cm, EXT_COMMIT_MSG, false);
            hub_chunk_end(ck, ck->ok);
        }
        hub_git_free(data);
    }
    furi_string_free(t);
    free(ck);
    free(cm);
}

/* GET /api/git/storage?name= : pack and loose object counts. Opens the 256
 * object folders, so it takes a few seconds on a big repo. */
static void route_storage(Ext* e) {
    HubReq* r = e->r;
    if(!hub_heap_ok(r, 8 * 1024)) return;
    char dir[128];
    char* name = e->name;
    if(!hub_query_get(r->query, "name", name, sizeof(e->name)) || !hub_git_name_ok(name)) {
        hub_send_error(r, 400, "bad repo name");
        return;
    }
    snprintf(dir, sizeof(dir), "/ext/git/%s.git/objects", name);
    FileInfo fi;
    if(storage_common_stat(r->storage, dir, &fi) != FSE_OK) {
        hub_send_error(r, 404, "repository not found");
        return;
    }
    File* d = storage_file_alloc(r->storage);
    unsigned packs = 0, loose = 0;
    uint64_t pack_bytes = 0;
    char fname[64];
    char sub[160];

    snprintf(sub, sizeof(sub), "%s/pack", dir);
    if(storage_dir_open(d, sub)) {
        while(storage_dir_read(d, &fi, fname, sizeof(fname))) {
            size_t n = strlen(fname);
            if(file_info_is_dir(&fi)) continue;
            if(n > 5 && strcmp(fname + n - 5, ".pack") == 0) packs++;
            if((n > 5 && strcmp(fname + n - 5, ".pack") == 0) ||
               (n > 4 && strcmp(fname + n - 4, ".idx") == 0))
                pack_bytes += fi.size;
        }
    }
    storage_dir_close(d);

    for(unsigned i = 0; i < 256; i++) {
        hub_beat();
        snprintf(sub, sizeof(sub), "%s/%02x", dir, i);
        if(!storage_dir_open(d, sub)) {
            storage_file_free(d); /* never reuse a File after a failed open */
            d = storage_file_alloc(r->storage);
            continue;
        }
        while(storage_dir_read(d, &fi, fname, sizeof(fname))) {
            if(!file_info_is_dir(&fi)) loose++;
        }
        storage_dir_close(d);
    }
    storage_file_free(d);

    char out[96];
    int n = snprintf(
        out,
        sizeof(out),
        "{\"ok\":true,\"packs\":%u,\"pack_kib\":%lu,\"loose\":%u}",
        packs,
        (unsigned long)(pack_bytes / 1024),
        loose);
    hub_send_body(r, 200, "application/json", out, (size_t)n);
}

#endif /* HUB_EXT_QUERY */

/* ---------- dispatcher ---------- */

bool hub_gitext_route(HubReq* r) {
    if(strncmp(r->path, "/api/git/", 9) != 0) return false;
    const char* name = r->path + 9;
    bool get = strcmp(r->method, "GET") == 0;
    bool post = strcmp(r->method, "POST") == 0;
    UNUSED(get);
    UNUSED(post);

    void (*fn)(Ext*) = NULL;
    bool allowed = false;
#ifdef HUB_EXT_QUERY
    if(strcmp(name, "diff") == 0) {
        fn = route_diff;
        allowed = get;
    } else if(strcmp(name, "compare") == 0) {
        fn = route_compare;
        allowed = get;
    } else if(strcmp(name, "search") == 0) {
        fn = route_search;
        allowed = get;
    } else if(strcmp(name, "log") == 0) {
        fn = route_log;
        allowed = get;
    } else if(strcmp(name, "commit") == 0) {
        fn = route_commit;
        allowed = get;
    } else if(strcmp(name, "storage") == 0) {
        fn = route_storage;
        allowed = get;
    }
#endif
#ifdef HUB_EXT_WRITE
    if(strcmp(name, "commit-file") == 0) {
        fn = route_commit_file;
        allowed = post;
    } else if(strcmp(name, "merge") == 0) {
        fn = route_merge;
        allowed = post;
    }
#endif
    if(!fn) return false;
    if(!allowed) {
        hub_send_error(r, 405, "method not allowed");
        return true;
    }
    Ext* e = malloc(sizeof(Ext));
    if(!e) {
        hub_send_error(r, 503, "low memory, try again");
        return true;
    }
    memset(e, 0, sizeof(Ext));
    e->r = r;
#ifdef HUB_EXT_QUERY
    bool timed = fn != route_search && fn != route_storage;
#else
    bool timed = fn == route_merge;
#endif
    if(timed) hub_deadline_start(EXT_DEADLINE_MS);
    fn(e);
    if(timed) hub_deadline_clear();
    if(e->g) hub_git_close(e->g);
#ifdef HUB_EXT_WRITE
    if(e->name[0]) hub_git_mark_loose(e->name);
#endif
    free(e);
    return true;
}

static const HubPluginRoute ext_ops = {.route = hub_gitext_route};
#ifdef HUB_EXT_QUERY
HUB_PLUGIN_ENTRY(hub_pl_query_ep, &ext_ops)
#else
HUB_PLUGIN_ENTRY(hub_pl_write_ep, &ext_ops)
#endif
