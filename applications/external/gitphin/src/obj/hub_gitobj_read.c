/* Object reader: loose and plain objects, deltas, hub_git_info / hub_git_read.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

/* ---------- loose objects ---------- */

static int type_from_name(const char* s, size_t n) {
    if(n == 4 && memcmp(s, "blob", 4) == 0) return HUB_OBJ_BLOB;
    if(n == 4 && memcmp(s, "tree", 4) == 0) return HUB_OBJ_TREE;
    if(n == 6 && memcmp(s, "commit", 6) == 0) return HUB_OBJ_COMMIT;
    if(n == 3 && memcmp(s, "tag", 3) == 0) return HUB_OBJ_TAG;
    return 0;
}

/* Open the loose object (path already in g->path) and parse "<type> <size>\0".
 * On success the file is open and the caller closes it. */
static int loose_head(
    HubGit* g,
    const HubOid* oid,
    HubGitFile** fout,
    uint8_t* type,
    size_t* size,
    size_t* hlen) {
    uint8_t h[HEAD_PEEK];
    loose_path(g, oid);
    HubGitFile* f = hub_gitio_open(g->io, g->path);
    if(!f) return HUB_GIT_ERR_NOTFOUND;
    size_t got = 0;
    int r = hub_inflate_buf(rd_file, f, h, sizeof(h), HUB_INF_PARTIAL, &got, NULL);
    if(r) {
        hub_gitio_close(f);
        return inf_err(r);
    }
    size_t i = 0;
    while(i < got && h[i] != ' ')
        i++;
    int t = i < got ? type_from_name((const char*)h, i) : 0;
    if(!t) goto bad;
    i++;
    uint64_t sz = 0;
    size_t digits = 0;
    while(i < got && h[i] >= '0' && h[i] <= '9') {
        sz = sz * 10 + (h[i] - '0');
        i++;
        digits++;
        if(sz > 0xFFFFFFFFull) goto bad;
    }
    if(digits == 0 || i >= got || h[i] != '\0') goto bad;
    *type = (uint8_t)t;
    *size = (size_t)sz;
    *hlen = i + 1;
    *fout = f;
    return HUB_GIT_OK;
bad:
    hub_gitio_close(f);
    return HUB_GIT_ERR_CORRUPT;
}

typedef struct {
    HubGitSink sink;
    void* ctx;
    size_t skip;
    size_t total;
} SkipCtx;

static bool skip_write(void* c, const uint8_t* p, size_t n) {
    SkipCtx* s = c;
    if(s->skip) {
        size_t k = n < s->skip ? n : s->skip;
        s->skip -= k;
        p += k;
        n -= k;
    }
    if(n == 0) return true;
    s->total += n;
    return s->sink(s->ctx, p, n);
}

/* ---------- plain objects (loose or pack, no delta) ---------- */

static int plain_info(HubGit* g, Loc loc, uint8_t* type, size_t* size) {
    if(loc.pack < 0) {
        HubGitFile* f;
        size_t hl;
        int r = loose_head(g, &loc.oid, &f, type, size, &hl);
        if(r == HUB_GIT_OK) hub_gitio_close(f);
        return r;
    }
    PackHdr h;
    int r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
    *type = h.type;
    *size = h.size;
    return HUB_GIT_OK;
}

static int plain_read(HubGit* g, Loc loc, uint8_t* type, uint8_t** data, size_t* size) {
    HubGitFile* f;
    size_t hl = 0, sz = 0, got = 0, in_off = 0;
    uint8_t t = 0;
    int r;
    if(loc.pack < 0) {
        r = loose_head(g, &loc.oid, &f, &t, &sz, &hl);
        if(r) return r;
        if(!hub_gitio_seek(f, 0)) {
            hub_gitio_close(f);
            return HUB_GIT_ERR_IO;
        }
    } else {
        PackHdr h;
        r = pack_hdr(g, loc.pack, loc.off, &h);
        if(r) return r;
        if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
        t = h.type;
        sz = h.size;
        f = g->pack_f;
        in_off = h.data_off;
    }
    if(sz > g->max_obj) {
        r = HUB_GIT_ERR_TOOLARGE;
        goto out;
    }
    uint8_t* buf = malloc(hl + sz + 1);
    if(!buf) {
        r = HUB_GIT_ERR_NOMEM;
        goto out;
    }
    if(loc.pack >= 0 && !hub_gitio_seek(f, in_off)) {
        free(buf);
        r = HUB_GIT_ERR_IO;
        goto out;
    }
    r = inf_err(hub_inflate_buf(rd_file, f, buf, hl + sz, 0, &got, NULL));
    if(r == HUB_GIT_OK && got != hl + sz) r = HUB_GIT_ERR_CORRUPT;
    if(r) {
        free(buf);
        goto out;
    }
    if(hl) memmove(buf, buf + hl, sz);
    buf[sz] = 0;
    *type = t;
    *data = buf;
    *size = sz;
out:
    if(loc.pack < 0) hub_gitio_close(f);
    return r;
}

/* ---------- delta ---------- */

static bool delta_varint(const uint8_t** p, const uint8_t* end, size_t* v) {
    size_t r = 0;
    unsigned shift = 0;
    for(;;) {
        if(*p >= end || shift > 28) return false;
        uint8_t c = *(*p)++;
        r |= (size_t)(c & 0x7f) << shift;
        shift += 7;
        if(!(c & 0x80)) break;
    }
    *v = r;
    return true;
}

/* Apply the delta of chain entry ce to *cur (size *cur_size). */
static int apply_delta(HubGit* g, const ChainEnt* ce, uint8_t** cur, size_t* cur_size) {
    if(ce->dsize > g->max_obj) return HUB_GIT_ERR_TOOLARGE;
    HubGitFile* f = pack_open(g, ce->pack);
    if(!f || !hub_gitio_seek(f, ce->data_off)) return HUB_GIT_ERR_IO;
    uint8_t* d = malloc((size_t)ce->dsize + 1);
    if(!d) return HUB_GIT_ERR_NOMEM;
    size_t got = 0;
    int r = inf_err(hub_inflate_buf(rd_file, f, d, ce->dsize, 0, &got, NULL));
    if(r == HUB_GIT_OK && got != ce->dsize) r = HUB_GIT_ERR_CORRUPT;
    if(r) {
        free(d);
        return r;
    }
    const uint8_t* p = d;
    const uint8_t* end = d + got;
    size_t src_size, dst_size;
    if(!delta_varint(&p, end, &src_size) || !delta_varint(&p, end, &dst_size) ||
       src_size != *cur_size) {
        free(d);
        return HUB_GIT_ERR_CORRUPT;
    }
    if(dst_size > g->max_obj) {
        free(d);
        return HUB_GIT_ERR_TOOLARGE;
    }
    uint8_t* out = malloc(dst_size + 1);
    if(!out) {
        free(d);
        return HUB_GIT_ERR_NOMEM;
    }
    size_t o = 0;
    const uint8_t* src = *cur;
    r = HUB_GIT_OK;
    while(p < end) {
        uint8_t cmd = *p++;
        if(cmd & 0x80) {
            size_t off = 0, len = 0;
            for(int k = 0; k < 4; k++) {
                if(cmd & (1 << k)) {
                    if(p >= end) goto bad;
                    off |= (size_t)(*p++) << (8 * k);
                }
            }
            for(int k = 0; k < 3; k++) {
                if(cmd & (0x10 << k)) {
                    if(p >= end) goto bad;
                    len |= (size_t)(*p++) << (8 * k);
                }
            }
            if(len == 0) len = 0x10000;
            if(off > src_size || len > src_size - off || len > dst_size - o) goto bad;
            memcpy(out + o, src + off, len);
            o += len;
        } else if(cmd) {
            if(cmd > (size_t)(end - p) || cmd > dst_size - o) goto bad;
            memcpy(out + o, p, cmd);
            p += cmd;
            o += cmd;
        } else {
            goto bad;
        }
    }
    if(o != dst_size) goto bad;
    out[o] = 0;
    free(d);
    free(*cur);
    *cur = out;
    *cur_size = dst_size;
    return HUB_GIT_OK;
bad:
    free(d);
    free(out);
    return HUB_GIT_ERR_CORRUPT;
}

/* Result size of a delta: first two varints of the top delta. */
static int delta_result_size(HubGit* g, const ChainEnt* ce, size_t* size) {
    HubGitFile* f = pack_open(g, ce->pack);
    if(!f || !hub_gitio_seek(f, ce->data_off)) return HUB_GIT_ERR_IO;
    uint8_t d[HEAD_PEEK];
    size_t got = 0;
    int r = hub_inflate_buf(rd_file, f, d, sizeof(d), HUB_INF_PARTIAL, &got, NULL);
    if(r) return inf_err(r);
    const uint8_t *p = d, *end = d + got;
    size_t a, b;
    if(!delta_varint(&p, end, &a) || !delta_varint(&p, end, &b)) return HUB_GIT_ERR_CORRUPT;
    *size = b;
    return HUB_GIT_OK;
}

/* ---------- public object API ---------- */

int hub_git_info(HubGit* g, const HubOid* oid, uint8_t* type, size_t* size) {
    Loc loc;
    int r = locate(g, oid, &loc);
    if(r) return r;
    if(loc.pack < 0) return plain_info(g, loc, type, size);

    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type >= 1 && h.type <= 4) {
        *type = h.type;
        *size = h.size;
        return HUB_GIT_OK;
    }
    Chain ch = {0};
    Loc base;
    r = walk_chain(g, loc, h, &ch, &base);
    if(r == HUB_GIT_OK) r = delta_result_size(g, &ch.v[0], size);
    if(r == HUB_GIT_OK) {
        size_t bsz;
        r = plain_info(g, base, type, &bsz);
    }
    free(ch.v);
    return r;
}

int hub_git_read(HubGit* g, const HubOid* oid, uint8_t* type, uint8_t** data, size_t* size) {
    Loc loc;
    int r = locate(g, oid, &loc);
    if(r) return r;
    if(loc.pack < 0) return plain_read(g, loc, type, data, size);

    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type >= 1 && h.type <= 4) return plain_read(g, loc, type, data, size);

    Chain ch = {0};
    Loc base;
    r = walk_chain(g, loc, h, &ch, &base);
    if(r) goto out;
    uint8_t* cur = NULL;
    size_t cur_size = 0;
    r = plain_read(g, base, type, &cur, &cur_size);
    if(r) goto out;
    for(unsigned i = ch.n; i > 0; i--) {
        r = apply_delta(g, &ch.v[i - 1], &cur, &cur_size);
        if(r) {
            free(cur);
            goto out;
        }
    }
    *data = cur;
    *size = cur_size;
out:
    free(ch.v);
    return r;
}
