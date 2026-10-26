/* Object reader: streaming sinks, the far window, the delta-to-file parser.
 * Part of hub_gitobj.c: included there, in this order, not built alone. */
#include "hub_gitobj_int.h"

/* ---------- streaming and the spill-to-SD path ---------- */

/* Output file that also serves the far window of the inflater: bytes already
 * written are read back from it. For a loose object the inflater counts the
 * "<type> <size>\0" header too (hl bytes, kept in hdr), the file does not. */
typedef struct {
    HubGit* g;
    HubGitFile* f;
    bool failed;
    uint64_t w; /* bytes written */
    size_t hl;
    char hdr[24];
} FileSink;

static bool file_sink(void* c, const uint8_t* p, size_t n) {
    FileSink* s = c;
    if(hub_gitio_write(s->f, p, n) != n) {
        s->failed = true;
        return false;
    }
    s->w += n;
    return true;
}

#ifdef HUB_TEST_COUNTERS
unsigned long hub_far_reads, hub_far_bytes; /* host tests only */
#endif

static bool far_read(void* c, uint64_t pos, uint8_t* b, size_t n) {
    FileSink* s = c;
#ifdef HUB_TEST_COUNTERS
    hub_far_reads++;
    hub_far_bytes += n;
#endif
    for(; n && pos < s->hl; n--)
        *b++ = (uint8_t)s->hdr[pos++];
    if(!n) return true;
    if(!obj_beat() || !hub_gitio_seek(s->f, pos - s->hl) || hub_gitio_read(s->f, b, n) != n)
        return false;
    return hub_gitio_seek(s->f, s->w); /* back to the write position */
}

static int plain_stream(
    HubGit* g,
    Loc loc,
    HubGitSink sink,
    void* ctx,
    uint8_t* type,
    size_t* size,
    FileSink* fs) { /* fs: the sink's own file, for the far window (NULL: none) */
    int r;
    size_t out = 0;
    if(loc.pack < 0) {
        HubGitFile* f;
        size_t hl;
        r = loose_head(g, &loc.oid, &f, type, size, &hl);
        if(r) return r;
        if(!hub_gitio_seek(f, 0)) {
            hub_gitio_close(f);
            return HUB_GIT_ERR_IO;
        }
        SkipCtx sc = {sink, ctx, hl, 0};
        if(fs) {
            static const char* const tn[5] = {"", "commit", "tree", "blob", "tag"};
            fs->hl = hl;
            snprintf(fs->hdr, sizeof(fs->hdr), "%s %u", tn[*type], (unsigned)*size);
        }
        r = inf_err(hub_inflate_stream(
            rd_file, f, skip_write, &sc, 0, &out, NULL, fs ? far_read : NULL, fs));
        hub_gitio_close(f);
        if(r == HUB_GIT_OK && (out != hl + *size || sc.total != *size)) r = HUB_GIT_ERR_CORRUPT;
        return r;
    }
    PackHdr h;
    r = pack_hdr(g, loc.pack, loc.off, &h);
    if(r) return r;
    if(h.type < 1 || h.type > 4) return HUB_GIT_ERR_CORRUPT;
    *type = h.type;
    *size = h.size;
    if(!hub_gitio_seek(g->pack_f, h.data_off)) return HUB_GIT_ERR_IO;
    r = inf_err(hub_inflate_stream(
        rd_file, g->pack_f, sink, ctx, 0, &out, NULL, fs ? far_read : NULL, fs));
    if(r == HUB_GIT_OK && out != h.size) r = HUB_GIT_ERR_CORRUPT;
    return r;
}

#define SPILL_BUF  1024
#define SPILL_PATH (HUB_GIT_PATH_MAX + 56)

/* States of the delta instruction parser (it sees the inflated bytes in
 * arbitrary chunks). */
enum {
    DS_SRC,
    DS_DST,
    DS_CMD,
    DS_ARGS,
    DS_INS
};

typedef struct {
    HubGit* g;
    HubGitFile* src;
    HubGitFile* dst;
    uint8_t* cb;
    int st, err;
    uint32_t acc;
    unsigned shift, bit;
    uint8_t cmd;
    size_t src_size, dst_size, out, off, len, ins_left;
} DeltaSpill;

static bool ds_write(DeltaSpill* d, const uint8_t* p, size_t n) {
    if(n > d->dst_size - d->out) {
        d->err = HUB_GIT_ERR_CORRUPT;
        return false;
    }
    if(hub_gitio_write(d->dst, p, n) != n) {
        d->err = HUB_GIT_ERR_IO;
        return false;
    }
    d->out += n;
    return true;
}

static bool ds_copy(DeltaSpill* d) {
    size_t off = d->off, len = d->len ? d->len : 0x10000;
    if(off > d->src_size || len > d->src_size - off) {
        d->err = HUB_GIT_ERR_CORRUPT;
        return false;
    }
    if(!hub_gitio_seek(d->src, off)) {
        d->err = HUB_GIT_ERR_IO;
        return false;
    }
    while(len) {
        size_t k = len < SPILL_BUF ? len : SPILL_BUF;
        if(!obj_beat() || hub_gitio_read(d->src, d->cb, k) != k) {
            d->err = HUB_GIT_ERR_IO;
            return false;
        }
        if(!ds_write(d, d->cb, k)) return false;
        len -= k;
    }
    return true;
}

/* Skip the argument bits that are not set; run the copy when all are read. */
static bool ds_args_step(DeltaSpill* d) {
    while(d->bit < 7 && !(d->cmd & (1u << d->bit)))
        d->bit++;
    if(d->bit == 7) {
        d->st = DS_CMD;
        return ds_copy(d);
    }
    return true;
}

static bool ds_sink(void* c, const uint8_t* p, size_t n) {
    DeltaSpill* d = c;
    while(n) {
        switch(d->st) {
        case DS_SRC:
        case DS_DST: {
            uint8_t b = *p++;
            n--;
            if(d->shift > 28) {
                d->err = HUB_GIT_ERR_CORRUPT;
                return false;
            }
            d->acc |= (uint32_t)(b & 0x7f) << d->shift;
            d->shift += 7;
            if(!(b & 0x80)) {
                if(d->st == DS_SRC) {
                    if(d->acc != d->src_size) {
                        d->err = HUB_GIT_ERR_CORRUPT;
                        return false;
                    }
                    d->st = DS_DST;
                } else {
                    d->dst_size = d->acc;
                    d->st = DS_CMD;
                }
                d->acc = 0;
                d->shift = 0;
            }
            break;
        }
        case DS_CMD:
            d->cmd = *p++;
            n--;
            if(d->cmd & 0x80) {
                d->off = d->len = 0;
                d->bit = 0;
                d->st = DS_ARGS;
                if(!ds_args_step(d)) return false;
            } else if(d->cmd) {
                d->ins_left = d->cmd;
                d->st = DS_INS;
            } else {
                d->err = HUB_GIT_ERR_CORRUPT;
                return false;
            }
            break;
        case DS_ARGS: {
            uint8_t b = *p++;
            n--;
            if(d->bit < 4)
                d->off |= (size_t)b << (8 * d->bit);
            else
                d->len |= (size_t)b << (8 * (d->bit - 4));
            d->bit++;
            if(!ds_args_step(d)) return false;
            break;
        }
        default: { /* DS_INS */
            size_t k = n < d->ins_left ? n : d->ins_left;
            if(!ds_write(d, p, k)) return false;
            p += k;
            n -= k;
            d->ins_left -= k;
            if(d->ins_left == 0) d->st = DS_CMD;
        }
        }
    }
    return true;
}
