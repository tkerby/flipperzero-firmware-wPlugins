/* Query plugin: diff, compare, search.
 * Part of hub_gitext.c: included there, in this order, not built alone. */
#include "hub_gitext_int.h"

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
