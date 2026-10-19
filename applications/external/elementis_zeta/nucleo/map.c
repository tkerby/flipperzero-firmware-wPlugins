#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-move-loop-invariants,no-rerun-cse-after-loop,no-early-inlining,no-tree-coalesce-vars,no-tree-loop-ivcanon,no-optimize-sibling-calls") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "map.h"
#include "assets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HDR_SIZE 64u

static __attribute__((always_inline)) inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void sc_drop_owner(const Map* m);

void map_bank_free(Map* m) {
    sc_drop_owner(m);
    if(!m->bank_blk) return;
    free(m->bank_blk);
    m->bank_blk = NULL;
    m->bank = NULL;
}

MapErr map_bank_reload(Map* m) {
    if(!m->w || m->bank_blk) return MAP_OK;
    if(!m->bank_size || !heap_can_alloc(m->bank_size)) return MAP_E_MEM;
    uint8_t* bank = malloc(m->bank_size);
    if(!bank) return MAP_E_MEM;
    if(!read_at(m->file, m->bank_off, bank, m->bank_size)) {
        free(bank);
        return MAP_E_READ;
    }
    if(!bank_validate_lim(bank, m->bank_size, m->bank_len)) {
        free(bank);
        return MAP_E_BANK;
    }
    m->bank = bank;
    m->bank_blk = bank;
    return MAP_OK;
}

void map_data_free(Map* m) {
    if(!m->blk) return;
    m->blk_old = (uintptr_t)m->blk;
    free(m->blk);
    m->blk = NULL;
}

// moves a pointer that pointed into the old blk to the same offset of the new one
#define REBASE(f) \
    do { \
        if(m->f) m->f = (const void*)((uintptr_t)m->f - m->blk_old + (uintptr_t)blk); \
    } while(0)

MapErr map_data_reload(Map* m) {
    if(!m->w || m->blk) return MAP_OK;
    if(!m->bank_off || !heap_can_alloc(m->bank_off)) return MAP_E_MEM;
    uint8_t* blk = malloc(m->bank_off); // the file's bytes [0, bank_off)
    if(!blk) return MAP_E_MEM;
    if(!read_at(m->file, 0, blk, m->bank_off)) {
        free(blk);
        return MAP_E_READ;
    }
    m->ground = (const uint8_t*)((uintptr_t)m->ground - m->blk_old + (uintptr_t)blk);
    m->cflags = (const uint8_t*)((uintptr_t)m->cflags - m->blk_old + (uintptr_t)blk);
    REBASE(objs);
    REBASE(npcs);
    REBASE(warps);
    REBASE(interacts);
    REBASE(spawns);
    REBASE(scripts);
    REBASE(code);
    REBASE(enc);
    REBASE(ledges);
    REBASE(xt);
    m->blk = blk;
    m->blk_old = 0;
    return MAP_OK;
}

void map_free(Map* m) {
    sc_drop_owner(m);
    if(m->blk) free(m->blk);
    if(m->bank_blk) free(m->bank_blk);
    memset(m, 0, sizeof(*m));
}

const char* map_err_str(MapErr e) {
    switch(e) {
    case MAP_OK: return "ok";
    case MAP_E_OPEN: return "open";
    case MAP_E_MAGIC: return "magic";
    case MAP_E_VERSION: return "version";
    case MAP_E_SIZE: return "size";
    case MAP_E_MEM: return "mem";
    case MAP_E_READ: return "read";
    case MAP_E_BANK: return "bank";
    case MAP_E_DATA: return "data";
    }
    return "?";
}

// section [off, off+len) must be 4-aligned and inside [HDR_SIZE, limit]
static bool sec_ok(uint32_t off, uint32_t len, uint32_t limit) {
    return (off & 3) == 0 && off >= HDR_SIZE && off <= limit && len <= limit - off;
}

static MapErr validate_data(const Map* m) {
    const uint8_t* bank = m->bank;
    for(uint32_t k = 0; k < (uint32_t)m->w * m->h; k++) {
        uint8_t g = m->ground[k];
        if(g == 0xFF) continue;
        const SprEntry* e = bank_spr(bank, g);
        if(!e || e->kind != SPR_TILE) return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_objs; k++) {
        const ObjDef* o = &m->objs[k];
        const SprEntry* e = bank_spr(bank, o->spr);
        if(!e || e->kind != SPR_OBJECT || !e->fw || !e->fh) return MAP_E_DATA;
        if(o->i0 + e->fw > m->w || o->j0 + e->fh > m->h) return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_npcs; k++) {
        const NpcDef* n = &m->npcs[k];
        const SprEntry* e = bank_spr(bank, n->spr);
        if(!e || e->kind != SPR_ACTOR || n->i >= m->w || n->j >= m->h) return MAP_E_DATA;
        if(npc_has_script(n) && npc_text_id(n) >= m->n_scripts) return MAP_E_DATA;
        if(npc_behavior(n) == NPCB_TRAINER && (!npc_has_script(n) || !npc_radius(n))) return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_interacts; k++) {
        const InteractDef* d = &m->interacts[k];
        if(interact_kind(d) == INT_SCRIPT && interact_text_id(d) >= m->n_scripts) return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_enc; k++) {
        const EncDef* e = &m->enc[k];
        if(!e->species || e->lv_min < 1 || e->lv_max > 100 || e->lv_min > e->lv_max || !e->weight) return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_ledges; k++) {
        const LedgeDef* l = &m->ledges[k];
        if(!(l->dir & 1) || l->dir > 7 || !map_in(m, l->i, l->j) || !(cflags_at(m, l->i, l->j) & C_LEDGE))
            return MAP_E_DATA;
    }
    for(uint32_t k = 0; k < m->n_scripts; k++) {
        const ScriptDef* d = &m->scripts[k];
        if(d->trigger > SCRT_STEP || !script_len(d) ||
           (uint32_t)script_off(d) + script_len(d) > m->code_len)
            return MAP_E_DATA;
    }
    return MAP_OK;
}

MapErr map_load_file(Map* m, const char* path) {
    map_free(m);
    uint8_t h[HDR_SIZE];
    if(!read_at(path, 0, h, HDR_SIZE)) return MAP_E_OPEN;
    if(memcmp(h, "NMAP", 4) != 0) return MAP_E_MAGIC;
    if(h[4] != 1 && h[4] != 2) return MAP_E_VERSION; // 2: u16 at 18 = resident bank bytes / 4, the rest of the bank is streamed (contract 3.1)

    uint8_t w = h[6], ht = h[7];
    if(!w || !ht || w > MAP_MAX_DIM || ht > MAP_MAX_DIM || h[8] > 7 || h[14] > MAP_MAX_NPCS) return MAP_E_SIZE; // (h[14] = n_npcs: the runtime arrays of the world hold MAP_MAX_NPCS, contract 72)
    uint16_t n_objs = rd16(h + 12);
    uint8_t n_npcs = h[14], n_warps = h[15], n_int = h[16], n_spw = h[17];
    uint32_t off_scr = rd32(h + 56); // M2: script section (all zero in older files)
    uint16_t n_scr = rd16(h + 60);
    uint32_t off_ground = rd32(h + 20), off_cf = rd32(h + 24), off_objs = rd32(h + 28),
             off_npcs = rd32(h + 32), off_warps = rd32(h + 36), off_int = rd32(h + 40),
             off_spw = rd32(h + 44), off_bank = rd32(h + 48), fsize = rd32(h + 52);

    int32_t real = file_size(path);
    if(real < 0) return MAP_E_OPEN;
    uint32_t cells = (uint32_t)w * ht;
    if(fsize != (uint32_t)real || (off_bank & 3) || off_bank < HDR_SIZE || off_bank + BANK_HDR > fsize)
        return MAP_E_SIZE;
    if(!sec_ok(off_ground, cells, off_bank) || !sec_ok(off_cf, cells, off_bank) ||
       !sec_ok(off_objs, 4u * n_objs, off_bank) || !sec_ok(off_npcs, 12u * n_npcs, off_bank) ||
       !sec_ok(off_warps, 8u * n_warps, off_bank) || !sec_ok(off_int, 8u * n_int, off_bank) ||
       !sec_ok(off_spw, 4u * n_spw, off_bank))
        return MAP_E_SIZE;
    // extras section (encounter table + ledges): u16 at 62 = file offset / 4, 0 = none; it lies before the scripts
    uint32_t off_x = (uint32_t)rd16(h + 62) * 4u, x_end = n_scr ? off_scr : off_bank;
    if(off_x && (!sec_ok(off_x, 5, x_end) || off_x < off_spw + 4u * n_spw)) return MAP_E_SIZE;
    uint32_t code_len = 0;
    if(n_scr) {
        // table (8 B each) followed by the code blob up to the bank
        if(n_scr > 255 || !sec_ok(off_scr, 8u * n_scr, off_bank)) return MAP_E_SIZE;
        code_len = off_bank - off_scr - 8u * n_scr;
        if(!code_len || code_len > 0xFFFFu) return MAP_E_SIZE;
    }

    uint32_t bank_len = fsize - off_bank, bank_size = bank_len;
    if(h[4] >= 2) {
        bank_size = (uint32_t)rd16(h + 18) * 4u;
        if(bank_size < BANK_HDR || bank_size > bank_len) return MAP_E_SIZE;
    }
    uint8_t* blk = heap_can_alloc(off_bank) ? malloc(off_bank) : NULL;
    uint8_t* bank = (blk && heap_can_alloc(bank_size)) ? malloc(bank_size) : NULL;
    if(!blk || !bank) {
        free(blk);
        return MAP_E_MEM;
    }
    if(!read_at(path, 0, blk, off_bank) || !read_at(path, off_bank, bank, bank_size)) {
        free(blk);
        free(bank);
        return MAP_E_READ;
    }
    if(!bank_validate_lim(bank, bank_size, bank_len) || (h[4] >= 2 && bank_resident_size(bank) != bank_size)) {
        free(blk);
        free(bank);
        return MAP_E_BANK;
    }

    m->id = h[5];
    m->w = w;
    m->h = ht;
    m->bg_level = h[8]; // (bits 0..1 the colour outside the map, bit 2 = MAP_BG_NOFOL, contract 69)
    m->flags = h[9];
    m->area_id = h[10];
    m->fly = h[11];
    m->n_objs = n_objs;
    m->n_npcs = n_npcs;
    m->n_warps = n_warps;
    m->n_interacts = n_int;
    m->n_spawns = n_spw;
    m->n_spr = bank_count(bank);
    m->ground = blk + off_ground;
    m->cflags = blk + off_cf;
    m->objs = (const ObjDef*)(blk + off_objs);
    m->npcs = (const NpcDef*)(blk + off_npcs);
    m->warps = (const WarpDef*)(blk + off_warps);
    m->interacts = (const InteractDef*)(blk + off_int);
    m->spawns = (const SpawnDef*)(blk + off_spw);
    m->n_scripts = (uint8_t)n_scr;
    m->code_len = (uint16_t)code_len;
    if(n_scr) {
        m->scripts = (const ScriptDef*)(blk + off_scr);
        m->code = blk + off_scr + 8u * n_scr;
    }
    if(off_x) {
        const uint8_t* x = blk + off_x;
        uint32_t ne = x[1], nl = x[2];
        if(off_x + 5u + 4u * ne + 3u * nl > x_end) {
            free(blk);
            free(bank);
            memset(m, 0, sizeof(*m));
            return MAP_E_SIZE;
        }
        m->enc_rate = x[0];
        m->n_enc = (uint8_t)ne;
        m->n_ledges = (uint8_t)nl;
        m->open_rate = x[4]; // contract 63: the encounter rate of open ground (byte 4 of the 5-byte header)
        m->enc = (const EncDef*)(x + 5);
        m->ledges = (const LedgeDef*)(x + 5 + 4 * ne);
        if(x[3] & 1) m->xt = x + 5 + 4 * ne + 3 * nl + (x[3] >> 1) * 8; // (contract 55: the roam block, map.h map_roam_block, lies between the ledges and the tail: x[3] bits 1..7 = its length in 8-byte units; trusted like the slots) // C5 (contract 42.1): the extras tail follows the ledges (rate[4], n[4], the slots); mapc wrote it inside the section (like the base extras, the slots are not re-validated)
    }
    m->bank = bank;
    m->bank_size = bank_size;
    m->bank_len = bank_len;
    m->bank_off = off_bank;
    snprintf(m->file, sizeof(m->file), "%s", path);
    m->blk = blk;
    m->bank_blk = bank;

    MapErr e = validate_data(m);
    if(e != MAP_OK) {
        map_free(m);
        return e;
    }
    return MAP_OK;
}

MapErr map_load(Map* m, uint8_t map_id) {
    char name[16];
    snprintf(name, sizeof(name), "map_%02u.bin", (unsigned)map_id);
    return map_load_file(m, name);
}

const WarpDef* warp_at(const Map* m, int i, int j) {
    for(uint8_t k = 0; k < m->n_warps; k++)
        if(m->warps[k].i == i && m->warps[k].j == j) return &m->warps[k];
    return NULL;
}

const InteractDef* interact_at(const Map* m, int i, int j, int mode, int facing) {
    for(uint8_t k = 0; k < m->n_interacts; k++) {
        const InteractDef* d = &m->interacts[k];
        if(d->i != i || d->j != j || d->mode != mode) continue;
        if(facing >= 0 && !((d->dirmask >> (facing & 7)) & 1)) continue;
        return d;
    }
    return NULL;
}

int ledge_dir_at(const Map* m, int i, int j) {
    for(uint8_t k = 0; k < m->n_ledges; k++)
        if(m->ledges[k].i == i && m->ledges[k].j == j) return m->ledges[k].dir;
    return -1;
}

const SpawnDef* spawn_by_id(const Map* m, uint8_t id) {
    for(uint8_t k = 0; k < m->n_spawns; k++)
        if(m->spawns[k].id == id) return &m->spawns[k];
    return NULL;
}

bool map_peek_spawn(uint8_t map_id, uint8_t spawn_id, uint8_t* i, uint8_t* j, uint8_t* facing) {
    char name[16];
    snprintf(name, sizeof(name), "map_%02u.bin", (unsigned)map_id);
    uint8_t h[HDR_SIZE], b[4];
    if(!read_at(name, 0, h, HDR_SIZE) || memcmp(h, "NMAP", 4) != 0 || spawn_id >= h[17]) return false;
    if(!read_at(name, rd32(h + 44) + 4u * spawn_id, b, 4) || b[0] != spawn_id || b[1] >= h[6] || b[2] >= h[7]) return false;
    *i = b[1];
    *j = b[2];
    *facing = b[3] & 7;
    return true;
}

int npc_at(const Map* m, int i, int j, const NpcState* st) {
    for(uint8_t k = 0; k < m->n_npcs; k++) {
        const NpcDef* n = &m->npcs[k];
        if(!(n->flags & NPCF_SOLID)) continue;
        if(!st) {
            if(n->i == i && n->j == j) return k;
            continue;
        }
        const NpcState* s = &st[k];
        if(!s->visible) continue;
        if((s->i == i && s->j == j) || (s->moving && s->to_i == i && s->to_j == j)) return k;
    }
    return -1;
}

// ---------------------------------------------------------------- streamed actor frames (map.h "Streamed actors")
#define SC_ENTRIES 16     // distinct (sprite, facing) blocks held at once; a screenful of NPCs needs far fewer
#define SC_CAP 2560u      // bytes kept beyond what the current frame draws: all 8 facings of the biggest single-frame NPC (8 x 306 B)

typedef struct {
    uint8_t* d;           // NULL = free slot
    uint16_t len;
    uint16_t last;        // g_epoch when it was last drawn
    uint8_t spr, dir;     // bank index, STORED direction (a folded facing uses its twin's block)
    uint8_t nfr;          // frames held: 1 = only frame 0, else all the sprite's frames
    uint8_t pad;
} ScEnt;

typedef struct {
    const Map* owner;
    uint32_t bytes;       // sum of the blocks' lengths
    ScEnt e[SC_ENTRIES];
} ScCache;

static ScCache* g_sc;
static uint16_t g_epoch;
#ifndef NUCLEO_DEVICE // contract 70: the stream read counters feed the host tests (contract 73: and no longer the DEV overlay's H field: -0.2 KB of the DEV build)
#define SS(x) x
static MapStreamStats g_ss;

const MapStreamStats* map_stream_stats(void) {
    return &g_ss;
}
#else
#define SS(x)
#endif

void map_stream_begin(void) {
    g_epoch++;
}

void map_stream_release(void) {
    if(!g_sc) return;
    for(int k = 0; k < SC_ENTRIES; k++) free(g_sc->e[k].d);
    free(g_sc);
    g_sc = NULL;
}

static __attribute__((noinline)) void sc_drop_owner(const Map* m) {
    if(g_sc && g_sc->owner == m) map_stream_release();
}

static void sc_free_ent(ScEnt* x) {
    free(x->d);
    g_sc->bytes -= x->len;
    memset(x, 0, sizeof(*x));
}

// Least recently drawn block that was not drawn in the current frame, or NULL.
static ScEnt* sc_victim(void) {
    ScEnt* v = NULL;
    uint16_t age = 0;
    for(int k = 0; k < SC_ENTRIES; k++) {
        ScEnt* x = &g_sc->e[k];
        uint16_t a = (uint16_t)(g_epoch - x->last);
        if(x->d && a > age) {
            age = a;
            v = x;
        }
    }
    return v;
}

// Contract 69: the follower's actor (one entry for every species: the pixels of species `s` are in files/follow.bin, FOL_HDR + (s - 1) x 5 facings; map.h). Anchor = bottom centre of the canvas.
const SprEntry g_fol_spr = {SPR_ACTOR, FOL_W, FOL_H, 8, 1, SPRF_MASK | SPRF_MIRROR | SPRF_HOP | SPRF_STREAM, 1, 1, FOL_W / 2, FOL_H - 1, 0, 0xFF, {0, 0, 0, 0}};

bool map_actor_view(const Map* m, uint8_t spr, uint8_t dir, uint8_t frame, Blit* out) {
    const bool fol = spr == FOL_SPR; // the follower: `frame` is its species (it has one frame), the cache keeps ONE block of it
    const SprEntry* e = fol ? &g_fol_spr : bank_spr(m->bank, spr);
    if(!e) return false;
    if(!(e->flags & SPRF_STREAM)) return bank_view(m->bank, spr, dir, frame, out);
    uint32_t d = bank_dir_index(e->ndirs, dir), nf = e->nframes, fsz = bank_frame_bytes(e);
    uint8_t flip = 0;
    if(d >= bank_stored_dirs(e)) {
        d = e->ndirs - d;
        flip = 1;
    }
    uint32_t f = nf > 1 ? frame % nf : 0;
    const uint8_t sp = fol ? frame : 0; // (ScEnt.pad holds the follower's species)
    if(!g_sc || g_sc->owner != m) {
        map_stream_release(); // another map's blocks (or none): start clean
        g_sc = heap_can_alloc(sizeof(ScCache)) ? calloc(1, sizeof(ScCache)) : NULL;
        if(!g_sc) return false;
        g_sc->owner = m;
    }
    ScEnt *hit = NULL, *slot;
    for(int k = 0; k < SC_ENTRIES; k++) {
        ScEnt* x = &g_sc->e[k];
        if(!x->d || x->spr != spr) continue;
        if(x->dir != d || x->pad != sp) { // another facing / species of the follower: its block goes (one block at most)
            if(fol) sc_free_ent(x);
            continue;
        }
        if(x->nfr == nf || f == 0) hit = x; // a full block has every frame; a frame-0 block serves frame 0
        else sc_free_ent(x);                // frame 0 only and a walking frame is asked for: replaced by the whole facing
    }
    if(!hit) {
        uint32_t len = (f == 0 ? 1 : nf) * fsz; // idle: frame 0 only; walking: the whole facing (frame 0 included) in one read
        for(;;) { // a free slot, heap for the block, and (softly) room under the cap: drop what was not drawn this frame, oldest first
            slot = g_sc->e;
            while(slot < g_sc->e + SC_ENTRIES && slot->d) slot++;
            bool room = slot < g_sc->e + SC_ENTRIES && heap_can_alloc(len);
            ScEnt* v = (room && g_sc->bytes + len <= SC_CAP) ? NULL : sc_victim();
            if(!v) {
                if(room) break;
                SS(g_ss.fails++;)
                return false; // retried at the next draw
            }
            sc_free_ent(v);
        }
        uint8_t* buf = malloc(len);
        SS(uint32_t t0 = asset_tick();)
        if(!buf || !(fol ? read_at(FOL_FILE, FOL_HDR + ((sp - 1u) * 5u + d) * fsz, buf, len) : read_at(m->file, m->bank_off + bank_data_off(e) + d * nf * fsz, buf, len))) {
            free(buf);
            SS(g_ss.fails++;)
            return false;
        }
        SS(g_ss.reads++;
           t0 = asset_tick() - t0;
           if(t0 > g_ss.max_ms) g_ss.max_ms = t0;)
#ifndef NUCLEO_DEVICE
        g_ss.bytes += len;
        g_ss.last_ms = t0;
#endif
        *slot = (ScEnt){.d = buf, .len = (uint16_t)len, .spr = spr, .dir = (uint8_t)d, .nfr = (uint8_t)(f == 0 ? 1 : nf), .pad = sp};
        g_sc->bytes += len;
#ifndef NUCLEO_DEVICE
        uint32_t cb = (uint32_t)sizeof(ScCache) + g_sc->bytes;
        if(cb > g_ss.peak_bytes) g_ss.peak_bytes = cb;
#endif
        hit = slot;
    }
    hit->last = g_epoch;
    bank_fill_view(e, hit->d + (hit->nfr == nf ? f * fsz : 0), flip, out);
    return true;
}

#ifndef NUCLEO_DEVICE
uint32_t map_stream_cache_bytes(void) {
    return g_sc ? (uint32_t)sizeof(ScCache) + g_sc->bytes : 0;
}

uint32_t map_stream_follower_blocks(void) { // contract 69: how many follower blocks the cache holds (at most 1)
    uint32_t n = 0;
    for(int k = 0; g_sc && k < SC_ENTRIES; k++) n += g_sc->e[k].d && g_sc->e[k].spr == FOL_SPR;
    return n;
}

uint32_t map_stream_block_bytes(void) {
    return g_sc ? g_sc->bytes : 0;
}

void map_stream_reset_peak(void) {
    g_ss.peak_bytes = map_stream_cache_bytes();
}
#endif
