#include "hub_gitsearch.h"
#include "hub_gittreecur.h"

#include <stdlib.h>
#include <string.h>
#include "hub_mem.h"

typedef struct {
    HubOid tree;
    size_t pos;
    uint16_t prefix;
} SFrame;

typedef struct {
    HubGit* g;
    HubGitSearch* s;
    /* matcher: lower-case query and KMP failure table */
    uint8_t q[HUB_GS_QUERY_MAX];
    uint8_t fail[HUB_GS_QUERY_MAX];
    size_t qn;
    /* walk */
    SFrame* st;
    size_t cap, sp;
    char path[HUB_GS_PATH_MAX + 1];
    bool stop;
    /* current blob */
    uint8_t* head; /* first HUB_GS_BINARY_PROBE bytes, until probed */
    size_t head_n;
    bool probed; /* head checked, the scan runs */
    bool binary;
    uint32_t line_no;
    size_t kmp; /* matcher state in this line */
    bool matched; /* this line matched */
    char text[HUB_GS_TEXT_MAX + 1];
    size_t text_n;
    bool cut;
    bool pending; /* bytes of an unfinished line exist */
    size_t seen; /* bytes passed to the sink */
    uint32_t chunks;
    /* working copies, not on the 4 KB stack */
    SFrame fr_;
    HubTreeCur cur_;
} SCtx;

static uint8_t lc(uint8_t c) {
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c + 32) : c;
}

static void kmp_build(SCtx* c, const char* q) {
    c->qn = strlen(q);
    for(size_t i = 0; i < c->qn; i++)
        c->q[i] = lc((uint8_t)q[i]);
    size_t k = 0;
    c->fail[0] = 0;
    for(size_t i = 1; i < c->qn; i++) {
        while(k > 0 && c->q[i] != c->q[k])
            k = c->fail[k - 1];
        if(c->q[i] == c->q[k]) k++;
        c->fail[i] = (uint8_t)k;
    }
}

/* One matcher step. *found is set when the whole query matched. */
static size_t kmp_step(SCtx* c, size_t state, uint8_t ch, bool* found) {
    ch = lc(ch);
    while(state > 0 && (state == c->qn || ch != c->q[state]))
        state = c->fail[state - 1];
    if(ch == c->q[state]) state++;
    if(state == c->qn) *found = true;
    return state;
}

static bool name_matches(SCtx* c, const char* path, size_t n) {
    size_t st = 0;
    bool found = false;
    for(size_t i = 0; i < n && !found; i++)
        st = kmp_step(c, st, (uint8_t)path[i], &found);
    return found;
}

static bool want_stop(SCtx* c) {
    HubGitSearch* s = c->s;
    if(s->should_stop && s->should_stop(s->stop_ctx)) {
        s->timed_out = true;
        s->truncated = true;
        c->stop = true;
    }
    return c->stop;
}

/* Report one hit. The (max_hits + 1)th hit is not reported: it only sets
 * truncated, so truncated means "there is more". */
static void hit(SCtx* c, uint32_t line, const char* text, size_t n, bool cut) {
    HubGitSearch* s = c->s;
    if(s->max_hits && s->hits >= s->max_hits) {
        s->truncated = true;
        c->stop = true;
        return;
    }
    HubGitHit h = {c->path, line, text, n, cut};
    s->hits++;
    if(!s->on_hit(s->hit_ctx, &h)) {
        s->truncated = true;
        c->stop = true;
    }
}

/* ---- line scanner over the blob stream ---- */

static void line_end(SCtx* c) {
    if(c->matched) hit(c, c->line_no, c->text, c->text_n, c->cut);
    c->line_no++;
    c->kmp = 0;
    c->matched = false;
    c->text_n = 0;
    c->cut = false;
    c->pending = false;
}

static void scan_bytes(SCtx* c, const uint8_t* p, size_t n) {
    for(size_t i = 0; i < n && !c->stop; i++) {
        uint8_t ch = p[i];
        if(ch == '\n') {
            line_end(c);
            continue;
        }
        c->pending = true;
        if(c->text_n < HUB_GS_TEXT_MAX) {
            c->text[c->text_n++] = (char)ch;
        } else {
            c->cut = true;
        }
        if(!c->matched) c->kmp = kmp_step(c, c->kmp, ch, &c->matched);
    }
}

static bool sink(void* ctx, const uint8_t* data, size_t len) {
    SCtx* c = ctx;
    if((++c->chunks & 3) == 0 && want_stop(c)) return false;
    c->seen += len;
    if(!c->probed) {
        size_t take = HUB_GS_BINARY_PROBE - c->head_n;
        if(take > len) take = len;
        memcpy(c->head + c->head_n, data, take);
        c->head_n += take;
        data += take;
        len -= take;
        if(c->head_n < HUB_GS_BINARY_PROBE) return true; /* wait for more */
        c->probed = true;
        if(memchr(c->head, 0, c->head_n)) {
            c->binary = true;
            return false;
        }
        scan_bytes(c, c->head, c->head_n);
        if(c->stop) return false;
    }
    scan_bytes(c, data, len);
    return !c->stop;
}

/* Scan the content of one regular file. */
static int scan_blob(SCtx* c, const HubOid* oid) {
    HubGitSearch* s = c->s;
    if(s->max_blob) {
        uint8_t type;
        size_t size;
        int rc = hub_git_info(c->g, oid, &type, &size);
        if(rc != HUB_GIT_OK) return rc;
        if(size > s->max_blob) {
            s->skipped++;
            return HUB_GIT_OK;
        }
    }
    c->head_n = 0;
    c->probed = false;
    c->binary = false;
    c->line_no = 1;
    c->kmp = 0;
    c->matched = false;
    c->text_n = 0;
    c->cut = false;
    c->pending = false;
    c->seen = 0;
    uint8_t type;
    size_t size;
    s->scanned++;
    int rc = hub_git_stream(c->g, oid, sink, c, &type, &size);
    if(c->binary) {
        s->binary++;
        return HUB_GIT_OK;
    }
    if(rc == HUB_GIT_OK || rc == HUB_GIT_ERR_WRITE) {
        if(rc == HUB_GIT_OK && !c->stop) {
            if(!c->probed) {
                /* a blob shorter than the probe: check and scan what we have */
                c->probed = true;
                if(memchr(c->head, 0, c->head_n)) {
                    s->binary++;
                    return HUB_GIT_OK;
                }
                scan_bytes(c, c->head, c->head_n);
            }
            if(c->pending && !c->stop) line_end(c); /* last line without "\n" */
        }
        return HUB_GIT_OK;
    }
    /* a blob that cannot be read (too big for RAM and no space for temp
     * files, corrupt) must not end the whole search */
    if(rc == HUB_GIT_ERR_TOOLARGE || rc == HUB_GIT_ERR_IO || rc == HUB_GIT_ERR_NOMEM) {
        s->skipped++;
        return HUB_GIT_OK;
    }
    return rc;
}

/* ---- walk ---- */

static bool push(SCtx* c, const HubOid* t, size_t prefix) {
    if(c->sp == HUB_GS_MAX_DEPTH) return false;
    if(c->sp == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 8;
        SFrame* n = realloc(c->st, nc * sizeof(SFrame));
        if(!n) return false;
        c->st = n;
        c->cap = nc;
    }
    c->st[c->sp].tree = *t;
    c->st[c->sp].pos = 0;
    c->st[c->sp].prefix = (uint16_t)prefix;
    c->sp++;
    return true;
}

int hub_git_search(HubGit* g, const HubOid* tree, HubGitSearch* s) {
    size_t ql = s->query ? strlen(s->query) : 0;
    if(ql == 0 || ql > HUB_GS_QUERY_MAX || !s->on_hit) return HUB_GIT_ERR_BADARG;
    s->hits = 0;
    s->scanned = s->skipped = s->binary = 0;
    s->truncated = s->timed_out = false;

    SCtx* c = calloc(1, sizeof(SCtx));
    if(!c) return HUB_GIT_ERR_NOMEM;
    c->head = malloc(HUB_GS_BINARY_PROBE);
    if(!c->head) {
        free(c);
        return HUB_GIT_ERR_NOMEM;
    }
    c->g = g;
    c->s = s;
    kmp_build(c, s->query);
    int rc = push(c, tree, 0) ? HUB_GIT_OK : HUB_GIT_ERR_NOMEM;

    while(rc == HUB_GIT_OK && c->sp > 0 && !c->stop) {
        size_t fi = c->sp - 1;
        c->fr_ = c->st[fi];
        rc = cur_open(g, &c->fr_.tree, c->fr_.pos, &c->cur_);
        if(rc != HUB_GIT_OK) break;
        size_t plen = c->fr_.prefix;
        bool pushed = false;
        while(c->cur_.valid && !pushed && !c->stop) {
            const HubTreeEntry* e = &c->cur_.e;
            if(plen + e->name_len + 1 > HUB_GS_PATH_MAX) {
                rc = HUB_GIT_ERR_TOOLARGE;
                break;
            }
            memcpy(c->path + plen, e->name, e->name_len);
            if(cur_is_tree(&c->cur_)) {
                size_t tl = e->name_len;
                c->path[plen + tl] = '/';
                HubOid sub = *e->oid;
                cur_advance(&c->cur_);
                c->st[fi].pos = c->cur_.pos;
                if(!push(c, &sub, plen + tl + 1)) {
                    rc = c->sp >= HUB_GS_MAX_DEPTH ? HUB_GIT_ERR_TOOLARGE : HUB_GIT_ERR_NOMEM;
                    break;
                }
                pushed = true;
                continue;
            }
            c->path[plen + e->name_len] = '\0';
            size_t nl = e->name_len;
            if(!want_stop(c)) {
                if(!s->no_names && name_matches(c, c->path, plen + nl)) {
                    hit(c, 0, "", 0, false);
                }
                if(!c->stop && !s->no_content && (e->mode == 0100644 || e->mode == 0100755)) {
                    HubOid oid = *e->oid;
                    rc = scan_blob(c, &oid);
                    /* the path buffer may not be touched by the scan: it is ours */
                }
            }
            if(rc != HUB_GIT_OK) break;
            cur_advance(&c->cur_);
        }
        cur_close(&c->cur_);
        if(rc == HUB_GIT_OK && !pushed && !c->stop) c->sp--;
    }
    free(c->st);
    free(c->head);
    free(c);
    return rc;
}
