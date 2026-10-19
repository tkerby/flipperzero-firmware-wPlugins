#pragma once
// Map loader + accessors (contract 3.1). No furi.h; files only through read_at()/file_size().
//
// A loaded map owns two heap blocks: `blk` = file bytes [0, off_bank) (header + sections) and
// `bank` = the RESIDENT part of the sprite bank (file bytes [off_bank, off_bank + bank_size)). Section pointers point INTO blk (little-endian,
// 4-byte aligned). Since file v2 the pixels of the big actors (SPRF_STREAM) are not in the bank block: they stay on the card behind it and
// map_actor_view() reads the facing that is drawn (see "Streamed actors" below).
#include <stdint.h>
#include <stdbool.h>
#include "bank.h"

#define MAP_MAX_DIM 64
#define MAP_MAX_NPCS \
    16 // the World's runtime arrays hold WORLD_MAX_NPCS of them (world.h defines it as this); a map file with more is refused (contract 72)

typedef enum {
    MAP_OK = 0,
    MAP_E_OPEN, // file missing / unreadable header
    MAP_E_MAGIC, // not an NMAP file
    MAP_E_VERSION, // unsupported version
    MAP_E_SIZE, // sizes / offsets inconsistent
    MAP_E_MEM, // heap exhausted
    MAP_E_READ, // short read
    MAP_E_BANK, // bank invalid
    MAP_E_DATA, // ground / object / npc index out of range or wrong sprite kind
} MapErr;

// Packed, little-endian, as stored in the file. Use these only through the Map pointers below.
typedef struct __attribute__((packed)) {
    uint8_t spr, i0, j0, flags;
} ObjDef;

typedef struct __attribute__((packed)) {
    uint8_t spr, i, j, facing;
    uint8_t behavior; // 0 static, 1 periodic turn
    uint8_t
        flags; // bit0 solid, bit1 script; bits 2..7 = the HIGH two bits (8, 9) of show / hide / talk flag (contract 40.2): read the flags through npc_show_flag() & co
    uint8_t text_id[2]; // u16 LE, 0xFFFF none (npc_text_id)
    uint8_t show_lo, hide_lo, param, talk_lo; // the low 8 bits of the flag ids
} NpcDef;

typedef struct __attribute__((packed)) {
    uint8_t i, j, target_map, target_i, target_j, target_facing;
    uint8_t flags; // bit0 no_fade
    uint8_t req_dirs; // bitmask of arrival facings, 0xFF any
} WarpDef;

typedef struct __attribute__((packed)) {
    uint8_t i, j;
    uint8_t dirmask; // allowed player facings (bit = facing 0..7), 0xFF any
    uint8_t mode; // 0 OK while facing, 1 on step-onto
    uint8_t
        kind; // bits 0..5: 0 TEXT, 2 SET_FLAG+TEXT, 3 TEXT then push back, 4 RUN_SCRIPT (script id in text_id); bits 6..7 = the high two bits of `flag` (contract 40.2): use interact_kind() / interact_flag()
    uint8_t text_id[2]; // u16 LE (interact_text_id / script id)
    uint8_t flag_lo;
} InteractDef;

typedef struct __attribute__((packed)) {
    uint8_t id, i, j, facing;
} SpawnDef;

// Script table entry (8 B). `off`/`len` are u16 LE, relative to the start of the code blob.
typedef struct __attribute__((packed)) {
    uint8_t off[2], len[2];
    uint8_t trigger; // SCRT_*
    uint8_t
        cond_lo; // low 8 bits of the condition flag id (script_cond_flag()); 0 = always; else the script only starts when flag == cond_value
    uint8_t cond_value; // 0 / 1
    uint8_t
        cond_hi; // the high two bits (8, 9) of the condition flag id (contract 40.2); 0 in every file written before them
} ScriptDef;

// Encounter slot (4 B) and ledge (3 B) of the map's "extras" section (contract 13).
typedef struct __attribute__((packed)) {
    uint8_t species, lv_min, lv_max, weight;
} EncDef;

typedef struct __attribute__((packed)) {
    uint8_t i, j;
    uint8_t dir; // jump facing: 1 SE (+i), 3 NE (-j), 5 NW (-i), 7 SW (+j)
} LedgeDef;

static inline uint16_t script_off(const ScriptDef* d) {
    return (uint16_t)(d->off[0] | (d->off[1] << 8));
}
static inline uint16_t script_len(const ScriptDef* d) {
    return (uint16_t)(d->len[0] | (d->len[1] << 8));
}

static inline uint16_t npc_text_id(const NpcDef* n) {
    return (uint16_t)(n->text_id[0] | (n->text_id[1] << 8));
}
static inline uint16_t interact_text_id(const InteractDef* d) {
    return (uint16_t)(d->text_id[0] | (d->text_id[1] << 8));
}
// Flag ids are 10 bits (WORLD_FLAGS = 1024, contract 40.2). The map format kept its sizes: bits 0..7 sit in the old u8, bits 8..9 in spare bits that every older file has clear.
static inline uint16_t npc_show_flag(const NpcDef* n) {
    return (uint16_t)(n->show_lo | ((n->flags & 0x0C) << 6));
}
static inline uint16_t npc_hide_flag(const NpcDef* n) {
    return (uint16_t)(n->hide_lo | ((n->flags & 0x30) << 4));
}
static inline uint16_t npc_talk_flag(const NpcDef* n) {
    return (uint16_t)(n->talk_lo | ((n->flags & 0xC0) << 2));
}
static inline uint16_t interact_flag(const InteractDef* d) {
    return (uint16_t)(d->flag_lo | ((d->kind & 0xC0) << 2));
}
static inline uint8_t interact_kind(const InteractDef* d) {
    return (uint8_t)(d->kind & 0x3F);
}
static inline uint16_t script_cond_flag(const ScriptDef* d) {
    return (uint16_t)(d->cond_lo | (d->cond_hi << 8));
}

_Static_assert(sizeof(ObjDef) == 4, "ObjDef");
_Static_assert(sizeof(NpcDef) == 12, "NpcDef");
_Static_assert(sizeof(WarpDef) == 8, "WarpDef");
_Static_assert(sizeof(InteractDef) == 8, "InteractDef");
_Static_assert(sizeof(SpawnDef) == 4, "SpawnDef");
_Static_assert(sizeof(ScriptDef) == 8, "ScriptDef");
_Static_assert(sizeof(EncDef) == 4, "EncDef");
_Static_assert(sizeof(LedgeDef) == 3, "LedgeDef");

// NpcDef.behavior: low nibble = kind, high nibble = wander radius in tiles (0..15)
// NPCB_TRAINER (M4-C, contract 18): a script NPC that sees the hero along its facing; high nibble = sight range in tiles (1..15),
// talk_flag = its "beaten" flag, text_id = the generated sight / battle script. It never turns or wanders.
// C5 world systems (contract 42): NPCB_BOULDER = a rock the Empuje module pushes (a solid NPC without text or script; world.c), NPCB_HOLE = a pit a boulder fills (a NON-solid NPC whose hide_flag is the hole's flag: it vanishes when the boulder lands in it).
// Contract 55 (roaming wild creatures, pilot): NPCB_ROAM = one of the map's `@roam` slots (the LAST n NPC slots of the map, solid, no text / script): hidden until the PC plugin's world_feat places a wild creature
// of the map's grass table on it (World.roam_*); it wanders inside the map's roam area (below), and touching it starts that wild battle.
enum {
    NPCB_STATIC = 0,
    NPCB_TURN = 1,
    NPCB_WANDER = 2,
    NPCB_TRAINER = 3,
    NPCB_BOULDER = 4,
    NPCB_HOLE = 5,
    NPCB_ROAM = 6
};
static inline uint8_t npc_behavior(const NpcDef* n) {
    return n->behavior & 15;
}
static inline uint8_t npc_radius(const NpcDef* n) {
    return n->behavior >> 4;
}
enum {
    NPCF_SOLID = 1,
    NPCF_SCRIPT = 2
}; // SCRIPT: text_id is a script id (NPC talk runs the script)
static inline bool npc_has_script(const NpcDef* n) {
    return (n->flags & NPCF_SCRIPT) != 0;
}
enum {
    WARPF_NO_FADE = 1
};
enum {
    MAPF_INTERIOR = 1,
    MAPF_BALLOON = 2,
    MAPF_ANIM = 4,
    MAPF_CAVE = 8,
    MAPF_DARK = 16,
    MAPF_GHOSTS = 32,
    MAPF_FOG = 64,
    MAPF_PITS = 128,
    MAPF_NOBIKE = MAPF_INTERIOR | MAPF_CAVE | MAPF_DARK
}; // BALLOON: the LAST bank sprite is the "!" balloon (an object, drawn above a trainer); ANIM: a tile / object of the bank has nframes > 1 (contract 22); CAVE (C2-S1, `@cave yes`): the Exit Wisp works here (contract 29); DARK (C4-A, `@dark yes`): without the Linterna module only a small diamond around the hero is drawn (contract 35.1); GHOSTS (`@ghosts yes`): the battle side reads it with world_map_flag(MAPF_GHOSTS) (contract 35.2); FOG (`@fog yes`, contract 40.5): drifting mist over the scene; PITS (C5, contract 42.2): the map has a pit NPC (legend `hole=`): a boulder that arrives on a tile is then checked against them (in the PC plugin)
enum {
    INT_MODE_FACE = 0,
    INT_MODE_STEP = 1
};
enum {
    INT_TEXT = 0,
    INT_SET_FLAG = 2,
    INT_PUSH = 3,
    INT_SCRIPT = 4
};
enum {
    SCRT_CALLED = 0,
    SCRT_ENTER = 1,
    SCRT_INTERACT = 2,
    SCRT_NPC = 3,
    SCRT_STEP = 4
};

// Runtime state of one NPC (world.c owns it; render.c draws from it). Positions are Q8 tile units
// (tile centre = i * 256), like the hero.
typedef struct {
    uint8_t facing; // 0..7
    uint8_t visible; // 0 = hidden (not drawn, not solid)
    uint8_t frame; // sprite frame: 0 idle, 1.. walk (only when the bank entry has nframes > 1)
    uint8_t moving; // a step is in progress
    uint8_t i, j; // last arrived tile
    uint8_t to_i, to_j; // destination while moving (the tile is reserved)
    uint8_t near_i, near_j; // tile the NPC is nearer to (depth box while moving)
    int8_t
        lift; // hop bob (SPRF_HOP sprites): pixels the sprite is raised above its ground position while walking
    uint8_t
        spr; // bank sprite drawn (contract 55: the NpcDef's, set at the map load; a roamer's is set to its species' actor by the PC plugin)
    int16_t fi,
        fj; // Q8 position (16 bits: a tile is at most 63 and the Q8 position at most 16,383; contract 69 took 4 B x 17 slots out of the World block)
} NpcState;

typedef struct {
    uint8_t id, w, h, bg_level, flags, area_id;
    uint8_t
        fly; // C6 (contract 46): 1 + the Planeador slot of this town (header byte 11, mapc `@fly`), 0 = not a Planeador town
    uint16_t n_objs;
    uint8_t n_npcs, n_warps, n_interacts, n_spawns;
    uint8_t n_spr; // sprites in the bank
    uint8_t n_scripts; // script table entries (0 on files written before M2)
    uint16_t code_len; // bytes of the bytecode blob
    const uint8_t* ground; // w*h bank indices, 0xFF = nothing
    const uint8_t* cflags; // w*h baked collision flags
    const ObjDef* objs; // sorted by (i0+fw-1)+(j0+fh-1)
    const NpcDef* npcs;
    const WarpDef* warps;
    const InteractDef* interacts;
    const SpawnDef* spawns;
    const ScriptDef* scripts; // n_scripts entries
    const uint8_t* code; // bytecode blob (script offsets are relative to it)
    uint8_t
        enc_rate; // wild encounters: encounter when rand8 < enc_rate on a step into C_GRASS (0 = none)
    uint8_t n_enc, n_ledges;
    uint8_t
        open_rate; // contract 63: encounter when rand8 < open_rate on a step onto OPEN ground (cflags == 0: not grass, water, counter, warp, trigger, ledge or arrow floor), drawn from the grass table; 0 = none
    const EncDef* enc; // n_enc slots (weighted)
    const LedgeDef* ledges; // n_ledges one-way ledges
    const uint8_t*
        xt; // C5 (contract 42.1): the extras tail (u8 rate[4], u8 n[4], then the EncDef slots of the water / fish_old / fish_good / fish_super tables), NULL = none; read it with map_table()
    const uint8_t* bank;
    uint32_t bank_size; // bytes of the resident bank block (the bank header's `size`)
    uint32_t bank_len; // bytes of the bank in the file: resident + streamed
    uint8_t* blk; // owned heap blocks (do not touch)
    uint8_t* bank_blk;
    uintptr_t
        blk_old; // where blk was while map_data_free() has dropped it (the pointers above still hold that address)
    uint32_t bank_off; // file offset of the bank (map_bank_reload)
    char file[32]; // the file the map was loaded from (map_bank_reload)
} Map;

// C5 (contract 42.1): the four extra wild tables of a map (mapc `@encounters water|fish_old|fish_good|fish_super`). k = TBL_*; *n = slots (0 = none), *rate = the table's byte (water: the
// encounter rate per step like `enc_rate`; fishing: the chance of a bite, 255 = always). Inline so the plugins read it without a symbol.
enum {
    TBL_WATER = 0,
    TBL_FISH_OLD,
    TBL_FISH_GOOD,
    TBL_FISH_SUPER
};
static inline const EncDef* map_table(const Map* m, uint8_t k, uint8_t* n, uint8_t* rate) {
    const uint8_t* t = m->xt;
    if(!t) {
        *n = *rate = 0;
        return NULL;
    }
    uint8_t off = 0;
    for(uint8_t j = 0; j < k; j++)
        off = (uint8_t)(off + t[4 + j]);
    *rate = t[k];
    *n = t[4 + k];
    return (const EncDef*)(t + 8 + 4 * off);
}

// Frees any previous contents of *m, then loads map_NN.bin (NN = 2-digit decimal id) from the app
// assets folder. On error *m is left empty (all zero, map_w() == 0) and the code is returned.
MapErr map_load(Map* m, uint8_t map_id);
// Same, from an explicit path (absolute, or relative to the app assets folder).
MapErr map_load_file(Map* m, const char* path);
// Frees the heap blocks and zeroes *m. Safe on an empty / zeroed Map.
void map_free(Map* m);
const char* map_err_str(MapErr e);
// The sprite bank is most of a map's RAM (lab: 13 of 14 KB) and only drawing needs it. map_bank_free() drops it
// (m->bank = NULL: bank_spr() answers NULL, nothing may draw) and map_bank_reload() reads it back from the file
// (MAP_OK when it is already there). Everything else of the map (ground, flags, objects, NPCs, scripts) stays.
void map_bank_free(Map* m); // also drops the streamed-actor cache
MapErr map_bank_reload(Map* m);
// The other block (header + ground + flags + objects + NPC / warp / interaction defs + scripts and bytecode, 0.3 - 2.3 KB) is
// dropped too while a scene that needs no map data is on (the battle, contract 10.7): map_data_free() frees `blk` and keeps
// the section pointers DANGLING (nothing may read the map until map_data_reload(); w, h, id, counts and bank_* stay valid),
// map_data_reload() reads it back from the file and re-bases every pointer. MAP_OK when it is already there.
void map_data_free(Map* m);
MapErr map_data_reload(Map* m);

// ---- Contract 69 (the follower): the first able creature of the party walks one tile behind the hero. Its overworld sprite is NOT in the map's bank: it is one SPRF_STREAM actor of fixed size, g_fol_spr, whose pixels are in
// files/follow.bin (tools/follow_ow.py writes it: FOL_HDR bytes of header, then per species 5 stored facings x one frame of FOL_W x FOL_H, pixels then mask; the three west facings are mirrors). It goes through map_actor_view()
// like a streamed NPC with the sprite index FOL_SPR; the `frame` argument is the species id (1 .. SP_COUNT - 1). The cache keeps ONE follower block (the facing in use); a turn is one card read.
#define FOL_SPR  0xFE // the "bank index" of the follower (a real bank has fewer sprites than that)
#define FOL_W    20
#define FOL_H    20
#define FOL_HDR  176 // 16 B header + the per-species table of the top opaque row (u8 each), padded
#define FOL_FILE "follow.bin"
#define MAP_BG_NOFOL \
    4 // bit 2 of Map.bg_level (the header's background byte, which keeps it: the colour is bg_level & 3): `@nofollow yes` = the follower stays hidden on this map (the ferry and the sea maps)
extern const SprEntry g_fol_spr;

// ---- Streamed actors (file v2, contract 3.1): the frames of a SPRF_STREAM actor (bank directory entry flag) are read from the map file when they
// are drawn: a facing's frame 0 (idle) or, when a walking frame is asked for, all the frames of that facing, in ONE read_at (open, seek,
// read, close; no handle is kept). The blocks live in a small LRU cache shared by every NPC of the map (<= 2.5 KB held; entries that were
// drawn in the current frame are never evicted, so a screenful of NPCs cannot thrash). Lookup is by (sprite, stored direction): two NPCs of the
// same sprite and facing share a block, a mirror-folded direction shares its twin's. map_bank_free(), map_free() and map_stream_release() drop
// every block (the battle load frees them with the bank).
// map_actor_view() is what the renderer draws NPCs with (bank_view() for a resident sprite). false = nothing to draw this frame (no heap, card
// problem; retried at the next call). The pointers in *out stay valid until the next map_actor_view() / map_stream_release().
bool map_actor_view(const Map* m, uint8_t spr, uint8_t dir, uint8_t frame, Blit* out);
void map_stream_begin(
    void); // the renderer calls it once per world frame: blocks used since are protected from eviction
void map_stream_release(void); // frees every cached block and the table
typedef struct {
    uint32_t reads; // card reads (one per miss)
    uint32_t fails; // reads / allocations that failed
    uint32_t
        max_ms; // the slowest read (debug overlay "H": the larger of this and the hero.bin loads)
#ifndef NUCLEO_DEVICE
    uint32_t bytes, last_ms,
        peak_bytes; // host tests: bytes read, last read, most heap the cache has held since map_stream_reset_peak()
#endif
} MapStreamStats;
const MapStreamStats* map_stream_stats(void);
#ifndef NUCLEO_DEVICE
void map_stream_reset_peak(void);
uint32_t map_stream_cache_bytes(void); // heap bytes the cache holds now (blocks + table)
uint32_t map_stream_block_bytes(void); // ... the blocks alone
uint32_t map_stream_follower_blocks(void); // contract 69: the follower's blocks in it (0 or 1)
#endif

static inline uint8_t map_w(const Map* m) {
    return m->w;
}
static inline uint8_t map_h(const Map* m) {
    return m->h;
}
static inline bool map_in(const Map* m, int i, int j) {
    return i >= 0 && j >= 0 && i < m->w && j < m->h;
}
// Bank index of the ground tile, 0xFF outside the map or when nothing is drawn.
static inline uint8_t ground_at(const Map* m, int i, int j) {
    return map_in(m, i, j) ? m->ground[j * m->w + i] : 0xFF;
}
// Collision flags; outside the map = C_SOLID.
static inline uint8_t cflags_at(const Map* m, int i, int j) {
    return map_in(m, i, j) ? m->cflags[j * m->w + i] : (uint8_t)C_SOLID;
}

// Contract 55 (roaming creatures): the ROAM BLOCK of the extras section lies right after the ledges (extras byte 3: bit 0 = the C5 tail follows, bits 1..7 = the block's length in 8-byte units; mapc writes it, trusted like the rest
// of the extras): ceil(w * h / 8) bytes of the roam area (bit j * w + i, LSB first: the walkable cells the roamers may stand on), then n_enc bytes, the bank index of each grass slot's `roam_<species>` actor, then zero padding
// to a multiple of 8. Only a map with NPCB_ROAM NPCs has one.
static inline const uint8_t* map_roam_block(const Map* m) {
    return (const uint8_t*)(m->ledges + m->n_ledges);
}
// The bit of cell (i, j), NOT range-checked (a neighbour of an area cell is inside the map: mapc keeps the area off the map's border rows / columns); map_roam_ok is the checked one.
static inline bool map_roam_bit(const Map* m, int i, int j) {
    const unsigned x = (unsigned)(j * m->w + i);
    return (map_roam_block(m)[x >> 3] >> (x & 7)) & 1;
}
static inline bool map_roam_ok(const Map* m, int i, int j) {
    return map_in(m, i, j) && map_roam_bit(m, i, j);
}
#define MAP_ROAM_NONE \
    0xFF // contract 66: the actor index of a grass slot that never roams (the Solar form of an affinity pair: a Lunar player would fight its counterpart)
static inline uint8_t map_roam_spr(const Map* m, uint8_t slot) {
    return map_roam_block(m)[(((unsigned)m->w * m->h + 7) >> 3) + slot];
}

// Warp on tile (i,j), or NULL.
const WarpDef* warp_at(const Map* m, int i, int j);
// First interaction on (i,j) with the given mode; if facing >= 0 its dirmask must allow that
// facing (0..7). NULL when none.
const InteractDef* interact_at(const Map* m, int i, int j, int mode, int facing);
// Jump facing (1 SE, 3 NE, 5 NW, 7 SW) of the ledge on (i,j), or -1 when there is none.
int ledge_dir_at(const Map* m, int i, int j);
// Spawn with the given id, or NULL.
const SpawnDef* spawn_by_id(const Map* m, uint8_t id);
// Reads spawn `spawn_id` of map `map_id` straight from its file (the map need not be loaded): the blackout's respawn point.
bool map_peek_spawn(uint8_t map_id, uint8_t spawn_id, uint8_t* i, uint8_t* j, uint8_t* facing);
// Index of the solid NPC standing on (i,j), or -1. `st` (nullable, n_npcs entries) gives the runtime
// state: positions (an NPC mid-step occupies its arrived AND its destination tile), hidden NPCs are
// ignored. NULL = static NPCs at their NpcDef tiles.
int npc_at(const Map* m, int i, int j, const NpcState* st);
