#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-thread-jumps,no-cse-follow-jumps,no-tree-reassoc") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "bank.h"

bool bank_validate_lim(const uint8_t* bank, uint32_t size, uint32_t limit) {
    if(!bank || size < BANK_HDR) return false;
    if(bank[0] != 1 && bank[0] != 2) return false; // 2: a map bank with streamed actors
    uint8_t n = bank[1];
    uint32_t total = (uint32_t)bank[4] | ((uint32_t)bank[5] << 8) | ((uint32_t)bank[6] << 16) |
                     ((uint32_t)bank[7] << 24);
    if(total > size || total < BANK_HDR + 16u * n) return false;
    for(uint8_t i = 0; i < n; i++) {
        const SprEntry* e = bank_spr(bank, i);
        if(e->kind > SPR_ACTOR || !e->w || !e->h || !e->nframes) return false;
        if(e->ndirs != 1 && e->ndirs != 4 && e->ndirs != 8) return false;
        uint32_t off = bank_data_off(e);
        if(off & 3) return false;
        uint32_t bpr = (e->w + 3u) / 4, mbpr = (e->w + 7u) / 8;
        uint32_t fsz = bpr * e->h + ((e->flags & SPRF_MASK) ? mbpr * e->h : 0);
        if((e->flags & SPRF_MIRROR) && e->ndirs == 1) return false;
        uint32_t need = fsz * bank_stored_dirs(e) * e->nframes;
        if(e->flags & SPRF_STREAM) { // behind the resident part, inside the file's bank
            if(e->kind != SPR_ACTOR || bank[0] < 2 || off < total || off > limit || need > limit - off) return false;
            continue;
        }
        if(off < BANK_HDR + 16u * n || off > total || need > total - off)
            return false;
    }
    return true;
}

void bank_fill_view(const SprEntry* e, const uint8_t* p, uint8_t flip, Blit* out) {
    uint8_t bpr = (uint8_t)((e->w + 3) / 4), mbpr = (uint8_t)((e->w + 7) / 8);
    out->pix = p;
    out->mask = (e->flags & SPRF_MASK) ? p + (uint32_t)bpr * e->h : (const uint8_t*)0;
    out->w = e->w;
    out->h = e->h;
    out->bpr = bpr;
    out->mbpr = mbpr;
    out->flip = flip;
}

bool bank_view(const uint8_t* bank, uint8_t idx, uint8_t dir, uint8_t frame, Blit* out) {
    const SprEntry* e = bank_spr(bank, idx);
    if(!e || (e->flags & SPRF_STREAM)) return false; // a streamed sprite has no resident pixels: map_actor_view()
    uint32_t fsz = bank_frame_bytes(e);
    uint32_t d = bank_dir_index(e->ndirs, dir);
    uint8_t flip = 0;
    if(d >= bank_stored_dirs(e)) { // folded direction: the mirror of direction ndirs - d
        d = e->ndirs - d;
        flip = 1;
    }
    uint32_t f = e->nframes > 1 ? (uint32_t)(frame % e->nframes) : 0;
    bank_fill_view(e, bank + bank_data_off(e) + (d * e->nframes + f) * fsz, flip, out);
    return true;
}
