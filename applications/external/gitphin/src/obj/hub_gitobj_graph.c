/* Object reader: the commit graph file and the tree walk.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

/* ---------- commit graph ---------- */

static bool graph_open(HubGit* g) {
    if(g->gstate) return g->gstate == 1;
    g->gstate = 2;
    snprintf(g->path, sizeof(g->path), "%s/hub/graph.bin", g->base);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return false;
    uint8_t* b = g->buf;
    uint32_t n = 0;
    if(read_at(f, 0, b, HUB_GRAPH_HDR) && memcmp(b, "HGRF\0\0\0\1", 8) == 0) {
        n = be32(b + 8);
        uint64_t want = HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * n + 20ull * be32(b + 12);
        if(n == 0 || hub_gitio_size(f) != want) n = 0;
    }
    if(n == 0) {
        hub_gitio_close(f);
        return false;
    }
    g->gf = f;
    g->gcount = n;
    g->gstate = 1;
    g->fan = malloc(1024); /* one read per lookup instead of two */
    if(g->fan && !read_at(f, HUB_GRAPH_HDR, g->fan, 1024)) {
        free(g->fan);
        g->fan = NULL;
    }
    return true;
}

/* 1 = rec holds the record, 0 = not in the graph */
static int graph_find(HubGit* g, const HubOid* oid, uint8_t* rec) {
    if(!graph_open(g)) return 0;
    uint8_t* b = g->buf;
    uint32_t lo = 0, hi;
    unsigned c = oid->b[0];
    if(g->fan) {
        lo = c > 0 ? be32(g->fan + 4 * (c - 1)) : 0;
        hi = be32(g->fan + 4 * c);
    } else if(c > 0) {
        if(!read_at(g->gf, HUB_GRAPH_HDR + 4 * (uint64_t)(c - 1), b, 8)) return 0;
        lo = be32(b);
        hi = be32(b + 4);
    } else {
        if(!read_at(g->gf, HUB_GRAPH_HDR, b, 4)) return 0;
        hi = be32(b);
    }
    if(lo > hi || hi > g->gcount) return 0;
    while(hi - lo > 3) {
        uint32_t mid = lo + (hi - lo) / 2;
        if(!read_at(g->gf, HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * mid, b, 20)) return 0;
        int cmp = memcmp(oid->b, b, 20);
        if(cmp == 0) {
            lo = mid;
            hi = mid + 1;
        } else if(cmp < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if(hi == lo || !read_at(
                       g->gf,
                       HUB_GRAPH_RECS + (uint64_t)HUB_GRAPH_REC * lo,
                       b,
                       (size_t)HUB_GRAPH_REC * (hi - lo)))
        return 0;
    for(uint32_t k = 0; k < hi - lo; k++) {
        if(memcmp(oid->b, b + HUB_GRAPH_REC * k, 20) == 0) {
            memmove(rec, b + HUB_GRAPH_REC * k, HUB_GRAPH_REC);
            return 1;
        }
    }
    return 0;
}

void hub_git_graph_ignore(HubGit* g) {
    g->quiet = true;
}

static void meta_hdr(void* ctx, const char* k, size_t kl, const char* v, size_t vl) {
    HubCommitMeta* m = ctx;
    char hex[41];
    if(kl == 6 && memcmp(k, "parent", 6) == 0 && vl == 40) {
        if(m->nparents < HUB_GIT_MAX_PARENTS) {
            memcpy(hex, v, 40);
            hex[40] = '\0';
            if(hub_oid_from_hex(hex, &m->parent[m->nparents])) m->nparents++;
        } else {
            m->nparents++;
        }
    } else if(kl == 9 && memcmp(k, "committer", 9) == 0) {
        HubGitIdent id;
        parse_ident(v, vl, &id);
        m->time = (uint32_t)id.time;
    }
}

int hub_git_commit_meta(HubGit* g, const HubOid* oid, HubCommitMeta* out) {
    uint8_t rec[HUB_GRAPH_REC];
    memset(out, 0, sizeof(*out));
    bool known = graph_find(g, oid, rec);
    if(known) {
        uint32_t fl = be32(rec + 64);
        if(!(fl & 1)) {
            unsigned np = (fl >> 8) & 0xff;
            if(np <= 2) {
                out->nparents = np;
                out->time = be32(rec + 20);
                memcpy(out->parent[0].b, rec + 24, 20);
                memcpy(out->parent[1].b, rec + 44, 20);
                out->cached = true;
                return HUB_GIT_OK;
            }
        }
    }
    if(!known) g->stale = true;
    uint8_t type;
    uint8_t* data;
    size_t size;
    int r = hub_git_read(g, oid, &type, &data, &size);
    if(r) return r;
    out->cached = known; /* cached: the id is in the graph (parents may come from the object) */
    if(type != HUB_OBJ_COMMIT) {
        free(data);
        return HUB_GIT_ERR_CORRUPT;
    }
    const char* end;
    parse_headers(data, size, meta_hdr, out, &end);
    free(data);
    return HUB_GIT_OK;
}

/* ---------- tree walk ---------- */

typedef struct {
    HubTreeCb cb;
    void* ctx;
    uint32_t mode;
    uint8_t st; /* 0 mode digits, 1 name, 2 id */
    uint8_t k;
    uint8_t nl;
    bool bad, stopped;
    char name[256];
    HubOid id;
} TreeWalk;

static bool tw_sink(void* c, const uint8_t* p, size_t n) {
    TreeWalk* w = c;
    for(size_t i = 0; i < n; i++) {
        uint8_t b = p[i];
        if(w->st == 0) {
            if(b >= '0' && b <= '7' && w->k < 8) {
                w->mode = w->mode * 8 + (b - '0');
                w->k++;
            } else if(b == ' ' && w->k > 0) {
                w->st = 1;
                w->nl = 0;
            } else {
                w->bad = true;
                return false;
            }
        } else if(w->st == 1) {
            if(b == 0 && w->nl > 0) {
                w->name[w->nl] = '\0';
                w->st = 2;
                w->k = 0;
            } else if(b != 0 && w->nl < 255) {
                w->name[w->nl++] = (char)b;
            } else {
                w->bad = true;
                return false;
            }
        } else {
            w->id.b[w->k++] = b;
            if(w->k == 20) {
                if(!w->cb(w->ctx, w->mode, w->name, w->nl, &w->id)) {
                    w->stopped = true;
                    return false;
                }
                w->st = 0;
                w->k = 0;
                w->mode = 0;
            }
        }
    }
    return true;
}

int hub_git_tree_walk(HubGit* g, const HubOid* tree, HubTreeCb cb, void* ctx) {
    TreeWalk* w = calloc(1, sizeof(TreeWalk));
    if(!w) return HUB_GIT_ERR_NOMEM;
    w->cb = cb;
    w->ctx = ctx;
    uint8_t type;
    uint8_t* data;
    size_t size;
    int r = hub_git_read(g, tree, &type, &data, &size);
    if(r == HUB_GIT_OK) {
        if(type == HUB_OBJ_TREE) tw_sink(w, data, size);
        free(data);
        if(type != HUB_OBJ_TREE) r = HUB_GIT_ERR_BADARG;
    } else if(r == HUB_GIT_ERR_TOOLARGE) {
        r = hub_git_stream(g, tree, tw_sink, w, &type, &size);
        if(r == HUB_GIT_ERR_WRITE && !w->stopped) r = HUB_GIT_ERR_CORRUPT;
        if(r == HUB_GIT_OK && type != HUB_OBJ_TREE) r = HUB_GIT_ERR_BADARG;
    }
    if(r == HUB_GIT_OK) {
        if(w->stopped)
            r = HUB_GIT_ERR_WRITE;
        else if(w->bad || w->st != 0)
            r = HUB_GIT_ERR_CORRUPT;
    }
    free(w);
    return r;
}
