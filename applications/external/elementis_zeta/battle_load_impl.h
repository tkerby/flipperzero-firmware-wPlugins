// (This is a header only so that the host Makefiles, which depend on every nucleo/*.h, rebuild when it changes; battle_load.c is its one-line translation unit on the device.)
// Data loading of the battle core (contract sections 7 and 27, memory recovery 38.1): the header checks of "moves.bin" / "reg.bin" / "trainers.bin" / "items.bin", the
// streamed validation of the type and item records (and, on the host, of every record) and the heap tables. It runs ONCE, at boot (party_init -> ui_bc_load), so on the
// device this file is part of the UI plugin (lib/ui/ui_unity.c) and the resident app has none of its ~1.1 KB; on the host battle_core.c includes it (the harnesses keep calling bc_load).
#pragma once
#include "battle_core_priv.h"

// ---- header checks + a streamed validation of every record (nothing but the small tables stays in RAM)
#define SCAN_BYTES 240u // transient buffer of one scan
typedef BcErr (*ScanFn)(BcData* d, uint8_t id, const uint8_t* r);

// Reads records first .. n-1 (rec bytes each, record 0 at byte `off`) in chunks and checks them with fn.
static BcErr scan(
    BcData* d,
    const char* file,
    uint32_t off,
    uint8_t first,
    uint8_t n,
    uint8_t rec,
    ScanFn fn) {
    uint8_t per = (uint8_t)(SCAN_BYTES / rec);
    uint8_t* buf = malloc((size_t)per * rec);
    if(!buf) return BC_E_MEM;
    BcErr e = BC_OK;
    for(uint8_t id = first; id < n && !e;) {
        uint8_t k = (uint8_t)(n - id < per ? n - id : per);
        if(!d->io.read(d->io.ctx, file, off + (uint32_t)id * rec, buf, (uint32_t)k * rec)) {
            e = BC_E_OPEN;
            break;
        }
        for(uint8_t j = 0; j < k && !e; j++)
            e = fn(d, (uint8_t)(id + j), buf + (size_t)j * rec);
        id = (uint8_t)(id + k);
    }
    free(buf);
    return e;
}

// The first `hdr` bytes of a file after the size / magic / version checks; *len = the file size.
static BcErr head(
    const BcData* d,
    const char* name,
    uint8_t* h,
    uint32_t hdr,
    const char* magic,
    uint32_t* len) {
    int32_t sz = d->io.size(d->io.ctx, name);
    if(sz < 0) return BC_E_OPEN;
    if((uint32_t)sz < hdr || sz > MAX_FILE) return BC_E_SIZE;
    if(!d->io.read(d->io.ctx, name, 0, h, hdr)) return BC_E_OPEN;
    if(memcmp(h, magic, 4) != 0) return BC_E_MAGIC;
    if(h[4] != 1) return BC_E_VERSION;
    *len = (uint32_t)sz;
    return BC_OK;
}

static BcErr sc_type(BcData* d, uint8_t id, const uint8_t* r) {
    if(r[0] == 0) d->phys |= 1u << id;
    return BC_OK;
}
#if BC_SCAN
static BcErr sc_move(BcData* d, uint8_t id, const uint8_t* r) {
    (void)id;
    MoveDef m;
    return rec_move(d, r, &m);
}
static BcErr sc_species(BcData* d, uint8_t id, const uint8_t* r) {
    (void)id;
    SpeciesDef s;
    return rec_species(d, r, &s);
}
static BcErr sc_trainer(BcData* d, uint8_t id, const uint8_t* r) {
    (void)id;
    TrainerDef t;
    return rec_trainer(d, r, &t);
}
#endif
static BcErr sc_item(BcData* d, uint8_t id, const uint8_t* r) {
    ItemDef* it = &d->items[id];
    it->kind = r[0];
    it->arg = r[1];
    const uint16_t price = rd16(r + 4);
    if(r[0] < IK_CATCH || r[0] > IK_ETHER || (r[0] == IK_CATCH && r[1] > BALL_MASTER) ||
       (r[0] == IK_HEAL && !r[1]) || (r[0] == IK_CURE && r[1] > SK_PARALYSIS) ||
       (r[0] == IK_BATTLE && r[1] > 6) || (r[0] == IK_KEY && (r[1] || price)))
        return BC_E_DATA;
    return BC_OK;
}

static BcErr load_all(BcData* d) {
    uint8_t h[24];
    uint32_t len;
    // moves.bin: the type table and the chart stay, the move records are only validated
    BcErr e = head(d, "moves.bin", h, 24, "NMOV", &len);
    if(e) return e;
    uint8_t nm = h[5], nt = h[6], nc = h[7];
    uint32_t off_m = rd32(h + 8), off_t = rd32(h + 12), off_c = rd32(h + 16);
    if(rd32(h + 20) != len || !nm || !nt || off_m + (uint32_t)nm * MOVE_REC > len ||
       off_t + (uint32_t)nt * TYPE_REC > len || off_c + CHART_BYTES(nc) > len || (off_m & 3) ||
       (off_t & 3) || (off_c & 3))
        return BC_E_SIZE;
    if(nt > 32) return BC_E_SIZE; // (bc_phys: one bit per type)
    d->n_moves = nm;
    d->n_types = nt;
    d->n_chart = nc;
    d->off_moves = off_m;
    d->off_chart = off_c;
    if((e = scan(d, "moves.bin", off_t, 0, nt, TYPE_REC, sc_type))) return e;
#if BC_SCAN
    if((e = scan(d, "moves.bin", off_m, 1, nm, MOVE_REC, sc_move))) return e;
#endif
    // reg.bin
    if((e = head(d, "reg.bin", h, 12, "NREG", &len))) return e;
    uint8_t n = h[5];
    if(h[6] != SPECIES_REC || rd32(h + 8) != len || !n || 12u + (uint32_t)n * SPECIES_REC != len)
        return BC_E_SIZE;
    d->n_species = n;
#if BC_SCAN
    if((e = scan(d, "reg.bin", 12, 1, n, SPECIES_REC, sc_species))) return e;
#endif
    // trainers.bin
    if((e = head(d, "trainers.bin", h, 12, "NTRN", &len))) return e;
    n = h[5];
    if(h[6] != TRAINER_REC || rd32(h + 8) != len || !n || 12u + (uint32_t)n * TRAINER_REC != len)
        return BC_E_SIZE;
    d->n_trainers = n;
#if BC_SCAN
    if((e = scan(d, "trainers.bin", 12, 1, n, TRAINER_REC, sc_trainer))) return e;
#endif
    // items.bin
    if((e = head(d, "items.bin", h, 12, "NITM", &len))) return e;
    n = h[5];
    if(h[6] != ITEM_REC || rd32(h + 8) != len || !n || 12u + (uint32_t)n * ITEM_REC != len)
        return BC_E_SIZE;
    d->items = calloc(n, sizeof(ItemDef));
    if(!d->items) return BC_E_MEM;
    d->n_items = n;
    if((e = scan(d, "items.bin", 12, 1, n, ITEM_REC, sc_item))) return e;
    d->cache = calloc(1, sizeof(BcCache));
    return d->cache ? BC_OK : BC_E_MEM;
}

BcErr bc_load(BcData* d, const BcIo* io) {
    memset(d, 0, sizeof(*d));
    d->io = *io;
    BcErr e = load_all(d);
    if(e) {
#ifdef NUCLEO_DEVICE // (the UI plugin has no bc_free: what load_all allocated is the item table and the record cache; bc_free's other blocks are not there yet)
        free(d->items);
        free(d->cache);
        memset(d, 0, sizeof(*d));
#else
        bc_free(d);
#endif
    }
    return e;
}
