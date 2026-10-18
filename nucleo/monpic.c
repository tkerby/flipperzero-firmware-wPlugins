#include "monpic.h"
#include "bank.h"
#include "assets.h"
#include <stdlib.h>
#include <string.h>

#define MON_FILE "mon.bin"

// Reads directory entry `idx` of mon.bin and its picture into one heap block. Nothing is kept open.
static bool load_entry(MonPic* p, unsigned idx) {
    uint8_t hdr[BANK_HDR];
    if(!read_at(MON_FILE, 0, hdr, BANK_HDR) || hdr[0] != 1) return false;
    if(idx >= 6u * ((unsigned)hdr[2] | ((unsigned)hdr[3] << 8)) + hdr[1]) return false; // header: u16 species count N, byte 1 = trainer pictures (monc.py)
    SprEntry e;
    if(!read_at(MON_FILE, BANK_HDR + 16u * idx, &e, sizeof(e))) return false;
    if(!e.w || !e.h || !(e.flags & SPRF_MASK)) return false; // no art for this species (placeholder entry)
    uint32_t bpr = (e.w + 3u) / 4u, mbpr = (e.w + 7u) / 8u;
    uint32_t psz = bpr * e.h, msz = mbpr * e.h;
    if(!heap_can_alloc(psz + msz)) return false;
    uint8_t* blk = malloc(psz + msz);
    if(!blk) return false;
    if(!read_at(MON_FILE, bank_data_off(&e), blk, psz + msz)) {
        free(blk);
        return false;
    }
    p->blk = blk;
    p->b.pix = blk;
    p->b.mask = blk + psz;
    p->b.w = e.w;
    p->b.h = e.h;
    p->b.bpr = (uint8_t)bpr;
    p->b.mbpr = (uint8_t)mbpr;
    p->b.flip = 0;
    return true;
}

bool monpic_load(MonPic* p, uint8_t species, bool back) {
    memset(p, 0, sizeof(*p));
    if(species == 0) return false;
    return load_entry(p, 2u * (species - 1u) + (back ? 1u : 0u));
}

bool monpic_load_kind(MonPic* p, uint8_t species, uint8_t kind) {
    memset(p, 0, sizeof(*p));
    uint8_t hdr[BANK_HDR];
    if(!read_at(MON_FILE, 0, hdr, BANK_HDR) || hdr[0] != 1) return false;
    unsigned n_sp = (unsigned)hdr[2] | ((unsigned)hdr[3] << 8); // header u16 = species count N (monc.py)
    if(!n_sp) return false;
    if(kind != BP_TRAINER && species == 0) return false;
    unsigned idx = kind == BP_TRAINER ? 6u * n_sp + species : 2u * n_sp + 4u * (species - 1u) + kind;
    return load_entry(p, idx);
}

void monpic_free(MonPic* p) {
    free(p->blk);
    memset(p, 0, sizeof(*p));
}
