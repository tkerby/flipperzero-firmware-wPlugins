#include "hub_gitdiff.h"
#include "hub_gittreecur.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "hub_mem.h"

/* ========== tree diff ========== */

typedef struct {
    HubOid a, b;
    bool ha, hb;
    size_t pa, pb; /* resume offsets in the two trees */
    uint16_t prefix; /* length of "dir/" in the path buffer */
} DFrame;

typedef struct {
    HubGit* g;
    HubGitDiffCb cb;
    void* ctx;
    size_t max, count;
    bool trunc, stop;
    DFrame* st;
    size_t cap, sp;
    char path[HUB_GD_PATH_MAX + 1];
    DFrame fr_; /* working copies live here, not on the 4 KB stack */
    HubTreeCur ca_, cb_;
} DCtx;

static int kind_of(uint32_t mode) {
    if(mode == 0120000) return 1;
    if(mode == 0160000) return 2;
    return 0;
}

static bool push(DCtx* d, const HubOid* a, const HubOid* b, size_t prefix) {
    if(d->sp == HUB_GD_MAX_DEPTH) return false;
    if(d->sp == d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 8;
        DFrame* n = realloc(d->st, nc * sizeof(DFrame));
        if(!n) return false;
        d->st = n;
        d->cap = nc;
    }
    DFrame* f = &d->st[d->sp++];
    memset(f, 0, sizeof(*f));
    if(a) {
        f->a = *a;
        f->ha = true;
    }
    if(b) {
        f->b = *b;
        f->hb = true;
    }
    f->prefix = (uint16_t)prefix;
    return true;
}

/* Report one file. Returns false when the walk must stop. */
static bool emit(DCtx* d, size_t plen, const HubTreeEntry* ea, const HubTreeEntry* eb) {
    if(d->count >= d->max) {
        d->trunc = true;
        d->stop = true;
        return false;
    }
    const HubTreeEntry* n = ea ? ea : eb;
    memcpy(d->path + plen, n->name, n->name_len);
    d->path[plen + n->name_len] = '\0';
    HubGitDiffFile f;
    memset(&f, 0, sizeof(f));
    f.path = d->path;
    if(ea) {
        f.old_oid = *ea->oid;
        f.old_mode = ea->mode;
    }
    if(eb) {
        f.new_oid = *eb->oid;
        f.new_mode = eb->mode;
    }
    f.status = !ea ? 'A' : !eb ? 'D' : kind_of(ea->mode) != kind_of(eb->mode) ? 'T' : 'M';
    d->count++;
    if(!d->cb(d->ctx, &f)) {
        d->stop = true;
        return false;
    }
    return true;
}

#define fr  (d->fr_)
#define ca  (d->ca_)
#define cb2 (d->cb_)

int hub_git_diff_trees(
    HubGit* g,
    const HubOid* a,
    const HubOid* b,
    HubGitDiffCb cb,
    void* ctx,
    size_t max_files,
    bool* truncated) {
    DCtx* d = calloc(1, sizeof(DCtx));
    if(!d) return HUB_GIT_ERR_NOMEM;
    d->g = g;
    d->cb = cb;
    d->ctx = ctx;
    d->max = max_files ? max_files : HUB_GD_MAX_FILES;
    if(truncated) *truncated = false;
    int rc = HUB_GIT_OK;

    if(!push(d, a, b, 0)) rc = HUB_GIT_ERR_NOMEM;
    if(rc == HUB_GIT_OK && a && b && memcmp(a, b, 20) == 0) d->sp = 0; /* same tree */

    while(rc == HUB_GIT_OK && d->sp > 0 && !d->stop) {
        size_t fi = d->sp - 1;
        fr = d->st[fi];
        rc = cur_open(g, fr.ha ? &fr.a : NULL, fr.pa, &ca);
        if(rc == HUB_GIT_OK) {
            rc = cur_open(g, fr.hb ? &fr.b : NULL, fr.pb, &cb2);
            if(rc != HUB_GIT_OK) cur_close(&ca);
        }
        if(rc != HUB_GIT_OK) break;
        size_t plen = fr.prefix;
        bool pushed = false;

        while((ca.valid || cb2.valid) && !pushed && !d->stop) {
            int c = !ca.valid ? 1 : !cb2.valid ? -1 : cur_cmp(&ca, &cb2);
            const HubTreeEntry* ea = c <= 0 ? &ca.e : NULL;
            const HubTreeEntry* eb = c >= 0 ? &cb2.e : NULL;
            bool tree = ea ? cur_is_tree(&ca) : cur_is_tree(&cb2);
            bool skip = false;
            if(ea && eb && memcmp(ea->oid, eb->oid, 20) == 0 && ea->mode == eb->mode) skip = true;

            if(tree && !skip) {
                size_t nl = (ea ? ea : eb)->name_len;
                if(plen + nl + 1 > HUB_GD_PATH_MAX) {
                    rc = HUB_GIT_ERR_TOOLARGE;
                    break;
                }
                memcpy(d->path + plen, (ea ? ea : eb)->name, nl);
                d->path[plen + nl] = '/';
                HubOid ta, tb;
                if(ea) ta = *ea->oid;
                if(eb) tb = *eb->oid;
                if(ea) cur_advance(&ca);
                if(eb) cur_advance(&cb2);
                d->st[fi].pa = ca.pos;
                d->st[fi].pb = cb2.pos;
                if(!push(d, ea ? &ta : NULL, eb ? &tb : NULL, plen + nl + 1)) {
                    rc = d->sp >= HUB_GD_MAX_DEPTH ? HUB_GIT_ERR_TOOLARGE : HUB_GIT_ERR_NOMEM;
                    break;
                }
                pushed = true;
                continue;
            }
            if(!tree && !skip) {
                if(plen + (ea ? ea : eb)->name_len > HUB_GD_PATH_MAX) {
                    rc = HUB_GIT_ERR_TOOLARGE;
                    break;
                }
                emit(d, plen, ea, eb);
            }
            if(ea) cur_advance(&ca);
            if(eb) cur_advance(&cb2);
        }
        if(rc == HUB_GIT_OK && !pushed && !d->stop) {
            if(ca.valid || cb2.valid) rc = HUB_GIT_ERR_CORRUPT;
            d->sp--;
        }
        cur_close(&ca);
        cur_close(&cb2);
    }
    if(rc == HUB_GIT_OK && d->stop && !d->trunc) rc = HUB_GIT_ERR_WRITE;
    if(truncated) *truncated = d->trunc;
    free(d->st);
    free(d);
    return rc;
}

#undef fr
#undef ca
#undef cb2

/* ========== compare ========== */

enum {
    F_BASE = 1, /* ancestor of base (or base) */
    F_HEAD = 2, /* ancestor of head (or head) */
    F_EXP_A = 4, /* parents read in the base walk */
    F_EXP_B = 8, /* parents read in the head walk */
    F_NOTBEST = 16 /* ancestor of a common commit: not a best common ancestor */
};

typedef struct {
    HubGit* g;
    size_t cap, n;
    uint8_t (*ids)[20];
    uint8_t* fl;
    uint32_t* tm;
    HubCommitMeta* meta;
    bool capped;
} Set;

/* index of id, or where it would go (found = false) */
static size_t set_find(const Set* s, const uint8_t* id, bool* found) {
    size_t lo = 0, hi = s->n;
    while(lo < hi) {
        size_t mid = (lo + hi) / 2;
        int c = memcmp(s->ids[mid], id, 20);
        if(c == 0) {
            *found = true;
            return mid;
        }
        if(c < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    *found = false;
    return lo;
}

/* Add flag bits to id, inserting it when new. false when the cap is hit. */
static bool set_mark(Set* s, const uint8_t* id, uint8_t flag) {
    bool found;
    size_t i = set_find(s, id, &found);
    if(found) {
        s->fl[i] |= flag;
        return true;
    }
    if(s->n >= s->cap) {
        s->capped = true;
        return false;
    }
    memmove(&s->ids[i + 1], &s->ids[i], (s->n - i) * 20);
    memmove(&s->fl[i + 1], &s->fl[i], s->n - i);
    memmove(&s->tm[i + 1], &s->tm[i], (s->n - i) * sizeof(uint32_t));
    memcpy(s->ids[i], id, 20);
    s->fl[i] = flag;
    s->tm[i] = 0;
    s->n++;
    return true;
}

static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Mark one parent; a parent of a common commit is not a best common ancestor. */
static void mark_parent(Set* s, const uint8_t* pid, uint8_t flag, bool common) {
    if(set_mark(s, pid, flag) && common) {
        bool f3;
        size_t pi = set_find(s, pid, &f3);
        if(f3) s->fl[pi] |= F_NOTBEST;
    }
}

/* More parents than the meta holds: read them from the object. */
static int walk_octopus(Set* s, const HubOid* id, uint8_t flag, bool common) {
    uint8_t type;
    uint8_t* data;
    size_t size;
    int rc = hub_git_read(s->g, id, &type, &data, &size);
    if(rc != HUB_GIT_OK) return rc;
    const uint8_t* p = data;
    const uint8_t* end = data + size;
    while(p < end && *p != '\n')
        p++; /* skip the tree line */
    while(p < end) {
        p++;
        if(end - p < 48 || memcmp(p, "parent ", 7) != 0) break;
        uint8_t pid[20];
        bool ok = true;
        for(int j = 0; j < 20 && ok; j++) {
            int h = hexval((char)p[7 + 2 * j]), l = hexval((char)p[8 + 2 * j]);
            if(h < 0 || l < 0) ok = false;
            pid[j] = (uint8_t)((h << 4) | (l < 0 ? 0 : l));
        }
        if(!ok) {
            hub_git_free(data);
            return HUB_GIT_ERR_CORRUPT;
        }
        mark_parent(s, pid, flag, common);
        p += 47;
        if(*p != '\n') break;
    }
    hub_git_free(data);
    return HUB_GIT_OK;
}

/* Walk everything reachable from start. flag marks the commits, expbit the
 * ones whose parents were read. */
static int set_walk(Set* s, const HubOid* start, uint8_t flag, uint8_t expbit) {
    set_mark(s, start->b, flag);
    for(;;) {
        size_t i = 0;
        while(i < s->n && !((s->fl[i] & flag) && !(s->fl[i] & expbit)))
            i++;
        if(i == s->n) return HUB_GIT_OK;
        HubOid id;
        memcpy(id.b, s->ids[i], 20);
        bool common = (s->fl[i] & F_BASE) && (s->fl[i] & F_HEAD);
        s->fl[i] |= expbit;

        HubCommitMeta* m = s->meta;
        int rc = hub_git_commit_meta(s->g, &id, m);
        if(rc != HUB_GIT_OK) return rc;
        bool f2;
        size_t k = set_find(s, id.b, &f2);
        if(f2) s->tm[k] = m->time;
        if(m->nparents > HUB_GIT_MAX_PARENTS) {
            rc = walk_octopus(s, &id, flag, common);
            if(rc != HUB_GIT_OK) return rc;
            continue;
        }
        for(unsigned j = 0; j < m->nparents; j++)
            mark_parent(s, m->parent[j].b, flag, common);
    }
}

int hub_git_compare(HubGit* g, const HubOid* base, const HubOid* head, HubGitCompare* out) {
    size_t cap = out->cap ? out->cap : HUB_GD_COMPARE_CAP;
    memset(&out->has_merge_base, 0, sizeof(*out) - offsetof(HubGitCompare, has_merge_base));
    Set s;
    memset(&s, 0, sizeof(s));
    s.g = g;
    s.cap = cap;
    s.ids = malloc(cap * 20);
    s.fl = malloc(cap);
    s.tm = malloc(cap * sizeof(uint32_t));
    s.meta = malloc(sizeof(HubCommitMeta));
    if(!s.ids || !s.fl || !s.tm || !s.meta) {
        free(s.meta);
        free(s.ids);
        free(s.fl);
        free(s.tm);
        return HUB_GIT_ERR_NOMEM;
    }
    int rc = set_walk(&s, base, F_BASE, F_EXP_A);
    if(rc == HUB_GIT_OK) rc = set_walk(&s, head, F_HEAD, F_EXP_B);
    if(rc == HUB_GIT_OK) {
        bool f;
        size_t bi = set_find(&s, base->b, &f);
        out->ff = f && (s.fl[bi] & F_HEAD);
        out->capped = s.capped;
        bool have = false;
        uint32_t best = 0;
        for(size_t i = 0; i < s.n; i++) {
            uint8_t x = s.fl[i];
            if((x & (F_BASE | F_HEAD)) == (F_BASE | F_HEAD)) {
                if(!(x & F_NOTBEST) && (!have || s.tm[i] > best)) {
                    have = true;
                    best = s.tm[i];
                    out->has_merge_base = true;
                    memcpy(out->merge_base.b, s.ids[i], 20);
                }
            } else if(x & F_HEAD) {
                out->ahead_count++;
                /* keep the newest HUB_GD_AHEAD_MAX, sorted newest first */
                unsigned pos = out->n_ahead;
                while(pos > 0) {
                    bool g2;
                    size_t pj = set_find(&s, out->ahead[pos - 1].b, &g2);
                    if(s.tm[pj] >= s.tm[i]) break;
                    pos--;
                }
                if(pos < HUB_GD_AHEAD_MAX) {
                    unsigned last = out->n_ahead < HUB_GD_AHEAD_MAX ? out->n_ahead :
                                                                      HUB_GD_AHEAD_MAX - 1;
                    for(unsigned m = last; m > pos; m--)
                        out->ahead[m] = out->ahead[m - 1];
                    memcpy(out->ahead[pos].b, s.ids[i], 20);
                    if(out->n_ahead < HUB_GD_AHEAD_MAX) out->n_ahead++;
                }
            } else if(x & F_BASE) {
                out->behind_count++;
            }
        }
    }
    free(s.meta);
    free(s.ids);
    free(s.fl);
    free(s.tm);
    return rc;
}
