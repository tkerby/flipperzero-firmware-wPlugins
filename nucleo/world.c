#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-tree-forwprop,no-if-conversion,no-caller-saves,no-move-loop-invariants,no-tree-coalesce-vars,no-optimize-sibling-calls,no-ssa-phiopt") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "world.h"
#include "text.h"
#include "gen/maps_gen.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "input.h"
#include "scene.h"
#include "sound.h"
#include "daycare.h"
#include "party.h" // party_state()->rv_steps (rustle_tick: the Reserva Salvaje's session)
#include "natives_shop.h" // reserva_on_step (contract 43.3)
#include "pc_host.h" // menu_open_daycare: the C5 world features run in the PC plugin (contract 42)

_Static_assert(FLAG_BIKE == FLAG_ON_BIKE, "world.h and mapc's builtin flag `on_bike` agree");
_Static_assert(FLAG_SURF == FLAG_ON_SURF && FLAG_BADGE_TOXINA == 84, "world.h and mapc's builtin flags `on_surf` / `badge_toxina` agree");

static const int8_t DI[8] = {1, 1, 1, 0, -1, -1, -1, 0};
static const int8_t DJ[8] = {1, 0, -1, -1, -1, 0, 1, 1};

// [dy+1][dx+1] -> facing (-1 = no direction)
static const int8_t DIR_TAB[3][3] = {{5, 4, 3}, {6, -1, 2}, {7, 0, 1}};

// ---------------------------------------------------------------- alloc / flags
World* g_w; // THE world (C4-A: world_on_bike / world_map_flag / world_bike_toggle are called by code that has no World pointer); set by world_alloc

World* world_alloc(void) {
    World* w = malloc(sizeof(World));
    if(!w) return NULL;
    memset(w, 0, sizeof(*w));
    w->rng = 0x9E3779B1u;
    w->dbg_on = WORLD_DEBUG_DEFAULT;
    g_w = w;
    return w;
}

void world_free(World* w) {
    if(!w) return;
    map_free(&w->map);
    free(w);
}

bool world_flag_get(const World* w, uint16_t id) {
    return id - 1u < WORLD_FLAGS - 1u && (w->flags[id >> 3] >> (id & 7)) & 1; // (flag 0 = none, and an id past the bitset is clear)
}

void world_flag_set(World* w, uint16_t id, bool v) {
    if(id - 1u >= WORLD_FLAGS - 1u) return;
    if(v && id >= 80 && id < 88 && !world_flag_get(w, id)) sound_play(SND_FX_BADGE); // the badge jingle: flags 80..87 are the badges (party.h FLAG_BADGE_FIRST; contract 28)
    if(v)
        w->flags[id >> 3] |= (uint8_t)(1 << (id & 7));
    else
        w->flags[id >> 3] &= (uint8_t)~(1 << (id & 7));
    world_refresh_npcs(w);
}

uint32_t world_rand(World* w) {
    uint32_t x = w->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return w->rng = x;
}

// ---------------------------------------------------------------- the bicycle (C4-A, contract 35.4)
static inline uint8_t bike_on(const World* w) {
    return (w->flags[FLAG_BIKE >> 3] >> (FLAG_BIKE & 7)) & 1;
}

static inline uint8_t surf_on(const World* w) { // C5 (contract 42.1)
    return (w->flags[FLAG_SURF >> 3] >> (FLAG_SURF & 7)) & 1;
}

uint16_t world_music_id(const World* w) {
#ifdef SND_MAR
    if(surf_on(w)) return SND_MAR; // the sea's track while surfing (assets/music/index.txt `music mar`; a missing track compiles out)
#endif
#ifdef SND_BICI
    if(bike_on(w)) return SND_BICI;
#endif
    return (uint16_t)(SND_PICK + SND_TAB_MAP + w->map.id);
}

bool world_bike_toggle(void) {
    World* w = g_w;
    if(!bike_on(w) && ((w->map.flags & MAPF_NOBIKE) || surf_on(w))) return false; // (an interior, a cave, a dark map; the bicycle and surfing exclude each other)
    w->flags[FLAG_BIKE >> 3] ^= (uint8_t)(1 << (FLAG_BIKE & 7));
    sound_play(world_music_id(w));
    return true;
}

// The text to show after world_bike_toggle, a function of the state it left: riding = got on; afoot on a map that refuses the bicycle = refused; else got off. Not in the plugin API table
// (the bag derives the same from the flag, `world_bike_toggle`'s result and TXT_GL_C4A_BIKE_*): the linker drops it from the device image; the host tests use it.
uint16_t world_bike_msg(void) {
    return bike_on(g_w) ? TXT_GL_C4A_BIKE_ON : ((g_w->map.flags & MAPF_NOBIKE) || surf_on(g_w)) ? TXT_GL_C4A_BIKE_NO : TXT_GL_C4A_BIKE_OFF;
}

void world_refresh_npcs(World* w) {
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) {
        const NpcDef* n = &w->map.npcs[k];
        const uint16_t sf = npc_show_flag(n), hf = npc_hide_flag(n);
        bool v = (!sf || world_flag_get(w, sf)) && (!hf || !world_flag_get(w, hf));
        if(w->npc_force[k]) v = w->npc_force[k] == 2; // script NPC_SHOW / NPC_HIDE wins until the next load
        w->npc[k].visible = v;
    }
}

bool world_can_enter(const World* w, int i, int j) {
    const uint8_t cf = cflags_at(&w->map, i, j);
    if((cf & (C_SOLID | C_LEDGE)) && !(surf_on(w) && (cf & C_WATER))) return false; // a ledge is only crossed by its hop (try_hop); water is walkable while surfing (C5)
    return npc_at(&w->map, i, j, w->npc) < 0;
}

// C5 (contract 42.1): the hero stands on land: he gets off the board (the map's music comes back). Called at every arrival and by world_place (a warp, a load, Continuar).
static void surf_check(World* w) {
    if(surf_on(w) && !(cflags_at(&w->map, w->hero.ti, w->hero.tj) & C_WATER)) {
        w->flags[FLAG_SURF >> 3] &= (uint8_t)~(1 << (FLAG_SURF & 7));
        sound_play(world_music_id(w));
    }
}

// The PC plugin runs the world features (scene_card.c world_feat: the surf / Empuje questions and their answers, the fishing cast); 1 = it pushed a scene or started a step.
static __attribute__((noinline)) bool feat(World* w, uint8_t act, uint8_t arg) {
    if(act != WACT_LAND && act != WACT_GIVE && act < WACT_ROAM) world_stop(w); // (a boulder that landed while he walks must not cut his step; a gift to the box must not cut a scripted walk: C6)
    return menu_open_daycare((uint8_t)(DC_FEAT + act), arg) != 0;
}

// Contract 55 (roaming creatures): one hero step has passed. A roamer that was fought waits roam_cnt[k] steps (the plugin sets it; 0 = not waiting, so every other NPC is inert here); when the wait ends a placement
// is requested (WACT_ROAM: the PC plugin places every hidden roamer that is not waiting, out of view; one it cannot place waits 6 more steps). The first placements are requested by the map load.
static void roam_tick(World* w) {
    for(uint8_t k = 0; k < WORLD_MAX_NPCS; k++)
        if(w->roam_cnt[k] && !--w->roam_cnt[k]) w->act = WACT_ROAM;
}

// Contract 71 (the rustling grass): one hero step has passed. A rustling tile counts its steps down; with none, a roll (rand8 < RUSTLE_ROLL) asks the casino plugin for a new one (it knows the rules: which maps, which tiles).
#ifndef NUCLEO_DEVICE
int g_rustle_on; // host tests only: the roll is OFF unless a test turns it on (the campaign walks and the route walkers depend on the RNG streams, which a roll on every step would shift)
#endif
static void rustle_tick(World* w) {
    if(w->rustle_n) {
        if(w->rustle_n > 1 || w->hero.ti != w->rustle_i || w->hero.tj != w->rustle_j) w->rustle_n--; // (a step onto the tile on its LAST step must still reach encounter_check: contract 72)
        return;
    }
    bool dev = false;
#if defined(NUCLEO_DEBUG) || !defined(NUCLEO_DEVICE)
    dev = (w->flags[FLAG_DEV_RUSTLE >> 3] >> (FLAG_DEV_RUSTLE & 7)) & 1;
#endif
#ifndef NUCLEO_DEVICE
    if(!g_rustle_on && !dev) return;
#endif
    // the cheap rules BEFORE the plugin is asked (contract 72): a roll that can never succeed must not load the casino plugin (scene_roam.c rustle_spawn keeps the same rules)
    if(!w->map.enc_rate || (w->map.flags & (MAPF_INTERIOR | MAPF_DARK)) || w->vars[VAR_REPEL] || w->scr.active || party_state()->rv_steps) return;
    if(dev || ((world_rand(w) >> 16) & 0xFF) < RUSTLE_ROLL) feat(w, WACT_RUSTLE, 0);
}

// Contract 81 (overworld poison): every 4th completed hero step (a free-running counter, not saved) a poisoned creature that still stands loses 1 HP: the casino plugin does the damage, the flash with its sound, the faint lines and the
// blackout when all have fainted (scene_poison.c); here only the counter and the cheap test, so a party without poison never loads it. Takes the arrival's `stop` (a warp, a script, a battle took it: the damage waits for the next step)
// and returns the new one (true = the plugin's scene is up, the arrival is over like a battle's).
static bool poison_step(World* w, bool stop) {
    if(++w->psn_n & 3) return stop;
    if(stop) {
        w->psn_n--;
        return true;
    }
    const Party* p = &party_state()->party; // (party_state, not party_player: the harnesses that link the world alone stub the one)
    for(uint8_t k = 0; k < p->count; k++)
        if(p->mon[k].hp && (p->mon[k].status & BS_PSN)) return feat(w, WACT_POISON, 0);
    return false;
}

// ---------------------------------------------------------------- the follower (contract 69)
// It stands where the hero stood one step ago: his tile is copied into the trail right before he moves on (at every arrival; a step cut short by world_stop leaves it one tile further back until his next step). A placement (a map load, a warp, a teleport) puts it on the tile behind him, whatever that
// tile is (a wall or water hide it until his first step: world_fol_pose).
#ifndef NUCLEO_DEVICE
int g_fol_on; // host tests only (contract 69): the follower is OFF unless a harness turns it on (follow_check.c): every older harness keeps its pictures
#define FOL_ON g_fol_on
#else
#define FOL_ON 1
#endif
static inline void fol_trail(World* w) {
    w->fol_i = w->hero.ti;
    w->fol_j = w->hero.tj;
}

// ---------------------------------------------------------------- placement / loading
static void hero_sync_pose(World* w) {
    Hero* h = &w->hero;
    h->fi = h->ti * 256;
    h->fj = h->tj * 256;
}

void world_stop(World* w) {
    Hero* h = &w->hero;
    h->cand = -1;
    h->turned = false;
    w->ok_buf = false;
    if(h->stepping) {
        // snap to the destination tile so the position is always consistent
        h->ti = h->to_i;
        h->tj = h->to_j;
        h->walk_px += h->step_px;
        h->stepping = false;
        h->hop = false;
        h->lift = 0;
    }
    h->frame = 0;
    w->scr.hsteps = 0; // a scripted walk is cancelled with the step
    hero_sync_pose(w);
}

void world_place(World* w, uint8_t ti, uint8_t tj, uint8_t facing) {
    Hero* h = &w->hero;
    memset(h, 0, sizeof(*h));
    h->ti = ti;
    h->tj = tj;
    h->facing = facing & 7;
    h->cand = -1;
    w->ok_buf = false;
    w->scr.hsteps = 0;
    w->rustle_n = 0; // contract 71: a map load, a warp or a teleport drops the rustling tile
    hero_sync_pose(w);
    surf_check(w);
    w->fol_i = (uint8_t)(ti - DI[h->facing]);
    w->fol_j = (uint8_t)(tj - DJ[h->facing]);
}

void world_art_release(World* w) {
    w->area_on = 0; // the battle frees the font: a banner would come back with its glyphs gone (contract 22)
    map_bank_free(&w->map);
    map_data_free(&w->map);
}

bool world_art_restore(World* w) {
    MapErr e = map_bank_reload(&w->map); // the big block first, then the small one
    if(e == MAP_OK) e = map_data_reload(&w->map);
    if(e == MAP_OK) return true;
    map_free(&w->map);
    script_stop(w);
    w->err = e;
    return false;
}

MapErr world_load_map(World* w, uint8_t id, uint8_t ti, uint8_t tj, uint8_t facing) {
    w->err = map_load(&w->map, id);
    if(w->err == MAP_OK && w->map.n_npcs > WORLD_MAX_NPCS) {
        map_free(&w->map);
        w->err = MAP_E_DATA;
    }
    script_stop(w); // the bytecode lived in the map that was just freed / replaced
    memset(w->npc, 0, sizeof(w->npc));
    memset(w->npcw, 0, sizeof(w->npcw));
    _Static_assert(offsetof(World, roam_cnt) == offsetof(World, npc_force) + WORLD_MAX_NPCS, "roam_cnt follows npc_force");
    memset(w->npc_force, 0, 2 * WORLD_MAX_NPCS); // (npc_force and roam_cnt, contract 55: no roamer is waiting)
    w->cam_npc = -1;
    w->cam_dy = 0;
    w->balloon = 0;
    w->push_on = w->act = w->roam_n = 0; // C5: Empuje is asked again on every map visit; contract 55: the roamers are counted below
    if(!w->rng_seeded) { // once: the creature RNG (seeded from the tick on the device) makes every run's rolls different
        w->rng_seeded = true;
        if(world_seed_hook) w->rng ^= world_seed_hook();
        if(!w->rng) w->rng = 0x9E3779B1u;
    }
    w->enc_grace = ENC_GRACE_STEPS;
    w->enc_pending = false;
    w->wild_pending = false;
    if(w->err == MAP_OK) {
        text_set_map(id);
        const unsigned fk = FLY_FLAG0 - 1u + w->map.fly; // C6 (contract 46): entering a Planeador town (header byte 1 + slot) marks it visited
        if(fk - FLY_FLAG0 < FLY_SLOTS) w->flags[fk >> 3] |= (uint8_t)(1u << (fk & 7));
        if(w->map.flags & MAPF_NOBIKE) w->flags[FLAG_BIKE >> 3] &= (uint8_t)~(1 << (FLAG_BIKE & 7)); // C4-A: a building, a cave or a dark map dismounts
        sound_play(world_music_id(w)); // the map's music (music.bin table; contract 28), the bicycle's while riding
        for(uint8_t k = 0; k < w->map.n_npcs; k++) {
            const NpcDef* n = &w->map.npcs[k];
            NpcState* s = &w->npc[k];
            s->facing = n->facing & 7;
            s->i = s->near_i = s->to_i = n->i;
            s->j = s->near_j = s->to_j = n->j;
            s->fi = n->i * 256;
            s->fj = n->j * 256;
            w->npc_next[k] = w->wclock + (uint32_t)n->param * 250u;
            s->spr = n->spr;
            if(npc_behavior(n) == NPCB_ROAM) { // contract 55: the roamers are placed (out of view) at the first update
                w->act = WACT_ROAM;
                w->roam_n++;
            }
        }
        world_refresh_npcs(w);
    }
    world_place(w, ti, tj, facing);
    w->just_loaded = true;
    w->enter_pending = w->err == MAP_OK;
    return w->err;
}

MapErr world_load_map_spawn(World* w, uint8_t id, uint8_t spawn_id) {
    MapErr e = world_load_map(w, id, 0, 0, 0);
    if(e != MAP_OK) return e;
    const SpawnDef* sp = spawn_by_id(&w->map, spawn_id);
    if(sp) world_place(w, sp->i, sp->j, sp->facing);
    return MAP_OK;
}

void (*world_new_game_hook)(void);

MapErr world_new_game(World* w) {
    if(world_new_game_hook) world_new_game_hook();
    memset(w->flags, 0, sizeof(w->flags));
    memset(w->vars, 0, sizeof(w->vars));
    w->fade_hold = 0;
    w->wclock = 0;
    w->bo = BO_NONE;
    return world_load_map_spawn(w, START_MAP, START_SPAWN);
}

// ---------------------------------------------------------------- stepping
static uint16_t dur_of(uint8_t f) {
    if(f & 1) return STEP_MS_ODD;
    return (f == 0 || f == 4) ? STEP_MS_S_N : STEP_MS_E_W;
}
static uint8_t px_of(uint8_t f) {
    if(f & 1) return 17; // sqrt(14^2 + 7^2) ~ 15.7; 17 keeps the walk cycle at ~2.4 frames/step
    return (f == 0 || f == 4) ? 14 : 28;
}

static void begin_step(World* w, uint8_t facing, int ni, int nj, uint32_t t0) {
    Hero* h = &w->hero;
    h->facing = facing;
    h->stepping = true;
    h->hop = false;
    h->to_i = (uint8_t)ni;
    h->to_j = (uint8_t)nj;
    h->t0 = t0;
    h->dur = dur_of(facing) >> bike_on(w); // C4-A: on the bicycle a step takes half the time
    h->step_px = px_of(facing);
}

// Ledge hop: the hero at A faces the jump direction `f` (odd = a world-axis step) of the ledge tile L = A + f, and lands
// on B = L + f (two tiles, one arc). Only that exact direction crosses a ledge, and only when B can be entered.
static bool try_hop(World* w, uint8_t f, uint32_t t0) {
    Hero* h = &w->hero;
    if(!(f & 1)) return false;
    int li = h->ti + DI[f], lj = h->tj + DJ[f];
    if(!(cflags_at(&w->map, li, lj) & C_LEDGE) || ledge_dir_at(&w->map, li, lj) != f) return false;
    int bi = li + DI[f], bj = lj + DJ[f];
    if(!world_can_enter(w, bi, bj)) return false;
    begin_step(w, f, bi, bj, t0);
    h->hop = true;
    h->dur = HOP_MS;
    h->step_px = 34; // two tiles of the walk cycle
    return true;
}

// ---------------------------------------------------------------- scripted movement
// Facing whose world step is the sign of the offset (fi,fj) -> (ti,tj); -1 when equal.
static int8_t dir_toward(int fi, int fj, int ti, int tj) {
    int di = (ti > fi) - (ti < fi), dj = (tj > fj) - (tj < fj);
    for(int8_t f = 0; f < 8; f++)
        if(DI[f] == di && DJ[f] == dj) return f;
    return -1;
}

bool world_hero_walk(World* w, uint8_t dir, uint8_t steps) {
    Hero* h = &w->hero;
    dir &= 7;
    h->cand = -1;
    h->turned = false;
    if(!steps || h->stepping) return false;
    int ni = h->ti + DI[dir], nj = h->tj + DJ[dir];
    h->facing = dir;
    if(!map_in(&w->map, ni, nj)) return false;
    w->scr.hdir = dir;
    w->scr.hsteps = steps;
    begin_step(w, dir, ni, nj, w->wclock);
    return true;
}

bool world_hero_busy(const World* w) {
    return w->hero.stepping || w->scr.hsteps;
}

void world_hero_face_npc(World* w, uint8_t k) {
    if(k >= w->map.n_npcs) return;
    int8_t f = dir_toward(w->hero.ti, w->hero.tj, w->npc[k].i, w->npc[k].j);
    if(f >= 0) w->hero.facing = (uint8_t)f;
    w->hero.frame = 0;
}

// Can NPC k walk onto (i,j)? Not through solid tiles, the hero (arrived or destination tile) or
// another solid NPC.
static bool npc_tile_free(const World* w, uint8_t k, int i, int j) {
    if(!map_in(&w->map, i, j) || (cflags_at(&w->map, i, j) & (C_SOLID | C_LEDGE))) return false;
    const Hero* h = &w->hero;
    if(h->ti == i && h->tj == j) return false;
    if(h->stepping && h->to_i == i && h->to_j == j) return false;
    int o = npc_at(&w->map, i, j, w->npc);
    return o < 0 || o == k;
}

// Starts the next step of NPC k's walk at wclock t0, or ends the walk when blocked.
static bool npc_try_step(World* w, uint8_t k, uint32_t t0) {
    NpcState* s = &w->npc[k];
    NpcWalk* nw = &w->npcw[k];
    uint8_t dir = nw->dir;
    int di = DI[dir], dj = DJ[dir];
    s->facing = dir;
    bool ok = npc_tile_free(w, k, s->i + di, s->j + dj);
    // same corner rule as the hero: a cardinal (2-tile diagonal) step needs both flanks free
    if(ok && !(dir & 1)) ok = npc_tile_free(w, k, s->i + di, s->j) && npc_tile_free(w, k, s->i, s->j + dj);
    if(!ok) {
        nw->steps = 0;
        s->moving = 0;
        s->frame = 0;
        return false;
    }
    s->moving = 1;
    s->to_i = (uint8_t)(s->i + di);
    s->to_j = (uint8_t)(s->j + dj);
    nw->t0 = t0;
    nw->dur = dur_of(dir);
    nw->step_px = px_of(dir);
    return true;
}

bool world_npc_walk(World* w, uint8_t k, uint8_t dir, uint8_t steps) {
    if(k >= w->map.n_npcs || k >= WORLD_MAX_NPCS || !w->npc[k].visible || w->npc[k].moving || !steps)
        return false;
    w->npcw[k].dir = dir & 7;
    w->npcw[k].steps = steps;
    npc_try_step(w, k, w->wclock);
    return true;
}

bool world_npc_busy(const World* w, uint8_t k) {
    return k < WORLD_MAX_NPCS && (w->npc[k].moving || w->npcw[k].steps);
}

bool world_npcs_busy(const World* w) {
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++)
        if(w->npc[k].moving || w->npcw[k].steps) return true;
    return false;
}

void world_npc_face(World* w, uint8_t k, uint8_t dir) {
    if(k < w->map.n_npcs && k < WORLD_MAX_NPCS) w->npc[k].facing = dir & 7;
}

void world_npc_place(World* w, uint8_t k, uint8_t i, uint8_t j, uint8_t facing) {
    if(k >= w->map.n_npcs || k >= WORLD_MAX_NPCS || !map_in(&w->map, i, j)) return;
    NpcState* s = &w->npc[k];
    s->i = s->to_i = s->near_i = i;
    s->j = s->to_j = s->near_j = j;
    s->fi = i * 256;
    s->fj = j * 256;
    s->moving = 0;
    s->frame = 0;
    s->facing = facing & 7;
    w->npcw[k].steps = 0;
}

void world_npc_show(World* w, uint8_t k, bool show) {
    if(k >= w->map.n_npcs || k >= WORLD_MAX_NPCS) return;
    w->npc_force[k] = show ? 2 : 1;
    world_refresh_npcs(w);
}

// Arrivals and chained steps of walking NPCs. Returns true when something moved.
static __attribute__((noinline)) bool npcs_logic(World* w, uint32_t now) {
    bool any = false;
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) {
        NpcState* s = &w->npc[k];
        NpcWalk* nw = &w->npcw[k];
        for(int guard = 0; guard < 4 && s->moving; guard++) {
            if(now >= nw->t0 && now - nw->t0 < nw->dur) break;
            uint32_t end = nw->t0 + nw->dur;
            s->i = s->to_i;
            s->j = s->to_j;
            s->moving = 0;
            nw->walk_px += nw->step_px;
            any = true;
            if(w->push_on && (w->map.flags & MAPF_PITS)) w->act = WACT_LAND; // C5 (contract 42.2): something rolled onto a tile of a map with pits: is there a pit under a boulder? (the PC plugin looks at the next update)
            if(nw->steps) nw->steps--;
            if(nw->steps) npc_try_step(w, k, end);
        }
        if(s->moving) any = true;
    }
    return any;
}

// Position / frame of every NPC at `now` (render state).
static __attribute__((noinline)) void npcs_pose(World* w, uint32_t now) {
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) {
        NpcState* s = &w->npc[k];
        const NpcWalk* nw = &w->npcw[k];
        if(!s->moving) {
            s->fi = s->i * 256;
            s->fj = s->j * 256;
            s->near_i = s->i;
            s->near_j = s->j;
            s->frame = 0;
            s->lift = 0;
            continue;
        }
        uint32_t el = now >= nw->t0 ? now - nw->t0 : 0;
        if(el >= nw->dur) el = nw->dur - 1;
        int e = (int)(el * 256u / nw->dur);
        s->fi = s->i * 256 + ((int)s->to_i - (int)s->i) * e;
        s->fj = s->j * 256 + ((int)s->to_j - (int)s->j) * e;
        s->near_i = e >= 128 ? s->to_i : s->i;
        s->near_j = e >= 128 ? s->to_j : s->j;
        const SprEntry* se = bank_spr(w->map.bank, s->spr);
        uint8_t nf = se ? se->nframes : 1;
        s->lift = 0;
        if(se && (se->flags & SPRF_HOP)) { // (a hop sprite of ONE frame bobs too: the roamers' actors, contract 55) // birds hop: one small arc per ~240 ms of the step (an E / W step is two), frame 1 in the air
            int f = (e * (nw->dur >= 400 ? 2 : 1)) & 255;
            s->lift = (int8_t)((NPC_HOP_ARC * 4 * f * (255 - f) + 32512) / 65025);
            s->frame = s->lift > 0 ? 1 : 0;
        } else if(nf > 1) { // frame 0 idle; 2 frames alternate idle / step, more frames cycle 1..nf-1
            uint32_t ph = (nw->walk_px + (uint32_t)nw->step_px * (uint32_t)e / 256u) / WALK_PX_PER_FRAME;
            s->frame = nf == 2 ? (uint8_t)(ph & 1) : (uint8_t)(1 + ph % (nf - 1u));
        } else {
            s->frame = 0;
        }
    }
}

// Contract 69: the follower's render state (world.h): its species = the first creature of the party that can fight (frame; nobody able = hidden), it walks from its tile to the hero's in step with him (Hero.prog), hopping once
// per step, facing the way it walks. Hidden too: the hero rides the bicycle or surfs, the map says `@nofollow` (MAP_BG_NOFOL), its tile is solid (a placement behind him in front of a wall: it shows from his first step) or water
// (he has just landed), or the map's bank / data are released (a battle: the cflags would dangle). The casino plugin asks the same for the OK reaction (scene_follow.c).
void world_fol_pose(const World* w, NpcState* s) {
    const Hero* h = &w->hero;
    const uint16_t r = world_party_ready_hook ? world_party_ready_hook() : 0; // the first creature able to fight: level | species << 8
    const int i = w->fol_i, j = w->fol_j, e = h->stepping ? h->prog : 0;
    s->frame = (uint8_t)(r >> 8);
    s->spr = FOL_SPR;
    s->near_i = (uint8_t)i;
    s->near_j = (uint8_t)j;
    s->visible = r && FOL_ON && w->map.bank && !(((w->flags[FLAG_BIKE >> 3] >> (FLAG_BIKE & 7)) | (w->flags[FLAG_SURF >> 3] >> (FLAG_SURF & 7)) | (w->map.bg_level >> 2)) & 1) && !(cflags_at(&w->map, i, j) & (C_SOLID | C_WATER));
    s->fi = (int16_t)(i * 256 + (h->ti - i) * e);
    s->fj = (int16_t)(j * 256 + (h->tj - j) * e);
    s->lift = (int8_t)((e * (255 - e)) >> 12); // one small hop per step: 0 .. 3 px
    s->facing = (uint8_t)dir_toward(i, j, h->ti, h->tj);
}

// A slide step of facing `sf` (pressed even facing `pressed`) may not end on a door/warp tile unless
// the warp accepts both facings: only deliberate door entries (never an accidental brush past a mat).
static __attribute__((always_inline)) inline bool slide_ok(const World* w, uint8_t sf, uint8_t pressed, int ti, int tj) {
    int ni = ti + DI[sf], nj = tj + DJ[sf];
    if(!(cflags_at(&w->map, ni, nj) & C_WARP)) return true;
    const WarpDef* wp = warp_at(&w->map, ni, nj);
    return !wp || (((wp->req_dirs >> pressed) & 1) && ((wp->req_dirs >> sf) & 1)); // flag without a WarpDef: harmless
}

// Tries to start a step of `dir` at wclock t0 (slide assist, corner rule). On failure the hero
// turns to face `dir` (bump). Returns true when a step started. `push` (an arrow floor's push, C4-A): no ledge hop, no sliding along a wall: it walks on or it stops.
static bool try_step(World* w, uint8_t dir, uint32_t t0, bool push) {
    Hero* h = &w->hero;
    int ti = h->ti, tj = h->tj;
    int di = DI[dir], dj = DJ[dir];

    if(dir & 1) {
        if(world_can_enter(w, ti + di, tj + dj)) {
            begin_step(w, dir, ti + di, tj + dj, t0);
            return true;
        }
        if(!push && try_hop(w, dir, t0)) return true;
    } else {
        bool dest = world_can_enter(w, ti + di, tj + dj);
        bool f1 = world_can_enter(w, ti + di, tj);
        bool f2 = world_can_enter(w, ti, tj + dj);
#if STRICT_CORNERS
        bool corner = !f1 || !f2; // no cutting a corner with a solid flank (it would draw through it)
#else
        bool corner = !f1 && !f2;
#endif
        if(dest && !corner) {
            begin_step(w, dir, ti + di, tj + dj, t0);
            return true;
        }
        // components along the world axes: +-i -> facing 1/5, +-j -> facing 7/3
        const uint8_t fa = di > 0 ? 1 : 5, fb = dj > 0 ? 7 : 3;
        // a ledge on one of the two axes (the diagonal itself is blocked): a cardinal press hops it like the world-axis key
        if(!push && (try_hop(w, fa, t0) || try_hop(w, fb, t0))) return true;
#if SLIDE_ASSIST
        // Slide along a wall / furniture side: only when the diagonal target tile exists (never along
        // the open edge of the map; a flank is inside the map whenever the target is).
        if(!push && map_in(&w->map, ti + di, tj + dj)) {
            if(f1 != f2) {
                uint8_t sf = f1 ? fa : fb;
                if(slide_ok(w, sf, dir, ti, tj)) {
                    begin_step(w, sf, ti + DI[sf], tj + DJ[sf], t0);
                    return true;
                }
            } else if(f1 && f2) { // both enterable (target solid): only a door tile wins
                bool w1 = cflags_at(&w->map, ti + di, tj) & C_WARP;
                bool w2 = cflags_at(&w->map, ti, tj + dj) & C_WARP;
                if(w1 != w2) {
                    uint8_t sf = w1 ? fa : fb;
                    if(slide_ok(w, sf, dir, ti, tj)) {
                        begin_step(w, sf, ti + DI[sf], tj + DJ[sf], t0);
                        return true;
                    }
                }
            }
        }
#endif
    }
    const int k = npc_at(&w->map, ti + di, tj + dj, w->npc); // what he bumped into
    if(k >= 0) {
        const uint8_t kind = npc_behavior(&w->map.npcs[k]);
        if(kind == NPCB_ROAM) feat(w, WACT_ROAM_HIT, (uint8_t)k); // contract 55: a roaming wild creature: the battle (the PC plugin hides it and queues it)
        // C5 (contract 42.2): Empuje is on and he walks into a boulder: it rolls one tile (npc_try_step's rules) unless that tile is a door / a step trigger
        else if(w->push_on && kind == NPCB_BOULDER && !(cflags_at(&w->map, ti + 2 * di, tj + 2 * dj) & (C_WARP | C_TRIGGER))) world_npc_walk(w, (uint8_t)k, dir, 1);
    }
    h->facing = dir; // bump
    h->frame = 0;
    return false;
}

// ---------------------------------------------------------------- arrow floors (C4-A, contract 35.3)
// The hero has arrived on (ti, tj): when it is an arrow floor he is pushed one tile in its facing, without input, at the speed of two steps; the next arrival decides again, so a run
// follows the arrows until a tile that does not push, a wall / a solid NPC (a screen-cardinal facing also needs both flank tiles free, like a walk), or a loop: arriving on the tile the
// run began on again ends it (tools/mapc.py refuses the maps where a run could fall into a loop that does not contain its first tile); SLIDE_MAX_RUN pushes end any run anyway.
#define SLIDE_MAX_RUN 64
static bool floor_push(World* w, uint32_t t0) {
    Hero* h = &w->hero;
    const Map* m = &w->map;
    const int at = h->tj * m->w + h->ti; // (the hero's own tile: always inside the map, and a C_SLIDE tile is a bank tile)
    if(m->cflags[at] & C_SLIDE) {
        if(!h->sl_n) { // the first arrow of a run
            h->sl_i = h->ti;
            h->sl_j = h->tj;
        } else if((h->sl_i == h->ti && h->sl_j == h->tj) || h->sl_n >= SLIDE_MAX_RUN) {
            goto end;
        }
        const uint8_t fl = m->bank[BANK_HDR + 16 * m->ground[at] + 5]; // (+5: the directory entry's `flags` byte)
        if((fl & SPRF_SLIDE_BIKE) && !bike_on(w)) goto end; // C5 (contract 42.6): a bike slope pushes a rider, nobody else (afoot it is plain floor)
        if(try_step(w, (fl >> SPRF_SLIDE_SHIFT) & 7, t0, true)) {
            h->sl_n++;
            h->dur >>= !bike_on(w); // (a push takes half a step's time; on the bicycle begin_step has halved it already)
            return true;
        }
    }
end:
    h->sl_n = 0; // the run is over
    return false;
}

bool world_start_step(World* w, uint8_t facing, bool forced) {
    Hero* h = &w->hero;
    facing &= 7;
    if(h->stepping) return false;
    if(forced) {
        int ni = h->ti + DI[facing], nj = h->tj + DJ[facing];
        if(!map_in(&w->map, ni, nj)) return false;
        begin_step(w, facing, ni, nj, w->wclock);
        return true;
    }
    return try_step(w, facing, w->wclock, false);
}

bool world_is_stepping(const World* w) {
    return w->hero.stepping;
}

static int8_t dir_from_keys(uint8_t held) {
    int dx = ((held & KEY_RIGHT) ? 1 : 0) - ((held & KEY_LEFT) ? 1 : 0);
    int dy = ((held & KEY_DOWN) ? 1 : 0) - ((held & KEY_UP) ? 1 : 0);
    return DIR_TAB[dy + 1][dx + 1];
}

// How long a new key combination must stay stable before it counts. A single D-pad key (cardinal
// facing) waits longer: it may be the first key of a two-key diagonal whose second key is still on
// its way. Two keys held (odd facing) are already complete.
static __attribute__((noinline)) uint32_t settle_ms(int8_t want) {
    return (want & 1) ? SETTLE_MS : SETTLE_SINGLE_MS;
}

static void hero_pose(World* w, uint32_t now) {
    Hero* h = &w->hero;
    if(!h->stepping) {
        hero_sync_pose(w);
        h->frame = 0;
        h->lift = 0;
        return;
    }
    uint32_t el = now >= h->t0 ? now - h->t0 : 0;
    if(el >= h->dur) el = h->dur - 1;
    int e = (int)(el * 256u / h->dur);
    h->prog = (uint8_t)e;
    h->fi = h->ti * 256 + ((int)h->to_i - (int)h->ti) * e;
    h->fj = h->tj * 256 + ((int)h->to_j - (int)h->tj) * e;
    if(h->hop) { // standing pose in the air, a parabola 0 .. HOP_ARC .. 0 (the shadow stays on the ground)
        h->frame = 0;
        h->lift = (int8_t)((HOP_ARC * 4 * e * (255 - e) + 32512) / 65025);
        return;
    }
    uint32_t wpx = h->walk_px + (uint32_t)h->step_px * (uint32_t)e / 256u;
    h->frame = (uint8_t)(1 + ((wpx / WALK_PX_PER_FRAME) & 3));
}

// ---------------------------------------------------------------- wild battles, blackout
bool (*world_wild_hook)(World* w, uint8_t species, uint8_t level, uint8_t* result_out);
uint16_t (*world_party_ready_hook)(void);
void (*world_heal_hook)(void);
uint32_t (*world_seed_hook)(void);

bool world_start_wild(World* w, uint8_t species, uint8_t level, int var) {
    if(!w->sc || !world_wild_hook) return false;
    uint8_t* out = (var >= 0 && var < SCRIPT_VARS) ? &w->vars[var] : &w->wild_res;
    *out = WILD_RAN; // what a battle that never reports (cannot open) leaves behind: no blackout
    world_stop(w);
    w->wild_out = out;
    if(!world_wild_hook(w, species, level, out)) return false;
    w->wild_pending = true;
    return true;
}

// First update after a wild battle (the scenes above the world have popped): grace steps, a stale shake is dropped,
// losing = blackout (also from a script's `wild`: its script is cut, the story does not go on), and the post-battle chain (below) is armed:
// a capture -> nickname, box, evolutions; anything else (won, ran) -> evolutions.
static void wild_done(World* w) {
    uint8_t res = w->wild_out ? *w->wild_out : (uint8_t)WILD_RAN;
    w->wild_pending = false;
    w->wild_out = NULL;
    w->enc_grace = ENC_GRACE_STEPS;
    w->rustle_n = 0;
    if(res == WILD_LOST) {
        world_blackout(w);
        if(world_pb_hook) world_pb_hook(w, PBH_CLEAR, 0); // no evolution after a blackout
    } else {
        w->pb = res == WILD_CAUGHT ? PB_NICK : PB_EVO;
        w->pb_k = 0;
    }
}

// ---------------------------------------------------------------- the post-battle chain (M5-B, contract 24)
// Runs at the first world update after ANY battle (wild: wild_done armed it; trainer: the leveled mask is set), BEFORE the script that was
// waiting for the result resumes (script_run is later in world_update; evolves right after the battle screen too). Order: (a) the
// nickname of a captured creature (PBH_NICK knows about the pending slot), (b) the trip of a pending creature to the box, (c) one
// evolution scene per party slot whose creature leveled in the battle and evolves at its level. Each step may push a scene: the world pauses
// under it and the next update goes on with the next step. What the chain needs from the party / battle / UI code comes through
// world_pb_hook (natives.c; NULL in the harnesses that link the world alone: no chain there).
uint8_t (*world_pb_hook)(World* w, uint8_t op, uint8_t k);

// true = a scene was pushed
static bool pb_step(World* w) {
    while(w->pb) {
        const uint8_t ph = w->pb;
        if(ph != PB_EVO) { // PB_NICK, PB_BOX: one hook call each (PBH_NICK / PBH_BOX = phase + 1)
            w->pb = (uint8_t)(ph + 1);
            if(world_pb_hook && world_pb_hook(w, (uint8_t)(ph + 1), 0)) return true;
        } else {
            while(w->pb_k < PB_SLOTS)
                if(world_pb_hook && world_pb_hook(w, PBH_EVO, w->pb_k++)) return true;
            if(world_pb_hook) world_pb_hook(w, PBH_CLEAR, 0);
            w->pb = PB_NONE;
        }
    }
    return false;
}

// A trainer battle came back (a battle sets the leveled mask; wild ones armed the chain in wild_done). PBH_MASK = the slots that leveled, 0 when the
// party is all fainted (that is the blackout the script is about to start: a lost battle never evolves anything; the hook clears the mask then).
static void pb_arm_trainer(World* w) {
    if(world_pb_hook && world_pb_hook(w, PBH_MASK, 0)) {
        w->pb = PB_EVO;
        w->pb_k = 0;
    }
}

// The last Center (script vars VAR_RESPAWN_MAP / VAR_RESPAWN_SPAWN, 0 = unset), else the new-game spawn: the fade warp of the blackout and of the Exit Wisp (C2-S1).
void world_go_respawn(World* w) {
    uint8_t mp = START_MAP, ti = START_I, tj = START_J, tf = START_FACING, rm = w->vars[VAR_RESPAWN_MAP];
    if(rm && map_peek_spawn(rm, w->vars[VAR_RESPAWN_SPAWN], &ti, &tj, &tf)) {
        mp = rm;
    } else {
        ti = START_I, tj = START_J, tf = START_FACING;
    }
    if(!w->sc || !scene_fade_warp(w->sc, mp, ti, tj, tf)) {
        world_load_map(w, mp, ti, tj, tf); // no scene stack room: no fade
    }
}

void world_blackout(World* w) {
    if(w->scr.active) script_stop(w); // the story does not go on after a lost battle
    w->bo = BO_SAY1;
}

static bool say(World* w, uint16_t text_id, int8_t push_back); // (hooks section)
static bool trainer_sight(World* w);                            // (trainers, before on_step_arrival)

// One phase per update: "everything goes white" + "you ran home" dialogs, then the fade home with the party healed.
static void blackout_step(World* w) {
    while(w->bo) {
        uint8_t ph = w->bo;
        if(ph == BO_SAY1) {
            w->bo = BO_SAY2;
            if(say(w, TXT_GL_BLACKOUT, -1)) return;
        } else if(ph == BO_SAY2) {
            w->bo = BO_WARP;
            if(say(w, TXT_GL_BLACKOUT_HOME, -1)) return;
        } else {
            w->bo = BO_NONE;
            world_league_reset(w); // C9 (contract 50): a blackout anywhere ends the League four run (the doors open again, every member must be beaten again)
            if(world_heal_hook) world_heal_hook();
            for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) { // trainers go back to their posts (a respawn in this very map keeps the NPCs)
                const NpcDef* n = &w->map.npcs[k];
                if(npc_behavior(n) == NPCB_TRAINER) world_npc_place(w, k, n->i, n->j, n->facing);
            }
            world_go_respawn(w);
        }
    }
}

// ---------------------------------------------------------------- the rustling grass (contract 71)
// The tile is chosen by the casino plugin (scene_roam.c rustle_spawn) and counted down by rustle_tick; here only its animation phase (the clock of the animated tiles, render.c draws it) and a redraw at every phase change.
static void rustle_update(World* w, uint8_t* dirty, uint32_t* wake) {
    if(!w->rustle_n) return;
    const uint8_t ph = (uint8_t)(1 + (w->aclk / RUSTLE_FRAME_MS) % RUSTLE_PHASES);
    if(ph != w->rustle_ph) {
        w->rustle_ph = ph;
        *dirty |= D_WORLD;
    }
    *wake = RUSTLE_FRAME_MS - w->aclk % RUSTLE_FRAME_MS;
}

// Wanderer (NpcDef behavior 2): picks a random facing and walks one tile, staying within `radius` of its home tile
// (the NpcDef tile) and never onto the hero, solids, doors / triggers / ledges / water, or another NPC.
// A ROAMER (behavior 6, contract 55) wanders over the whole roam area of the map (map_roam_ok: no radius, no tile test of its own) and a step INTO the hero is a touch: the wild battle.
static bool npc_wander_step(World* w, uint8_t k) {
    const NpcDef* n = &w->map.npcs[k];
    const NpcState* s = &w->npc[k];
    const bool roam = npc_behavior(n) == NPCB_ROAM;
    int r = npc_radius(n);
    uint8_t start = (uint8_t)(world_rand(w) >> 12) & 7;
    for(int t = 0; t < 8; t++) {
        uint8_t dir = (uint8_t)((start + t) & 7);
        int ni = s->i + DI[dir], nj = s->j + DJ[dir];
        if(roam) {
            if(ni == w->hero.ti && nj == w->hero.tj) {
                feat(w, WACT_ROAM_HIT, k);
                return false;
            }
            if(!map_roam_bit(&w->map, ni, nj)) continue;
        } else {
            if(abs(ni - n->i) > r || abs(nj - n->j) > r) continue;
            if(cflags_at(&w->map, ni, nj) & (C_SOLID | C_WARP | C_TRIGGER | C_LEDGE | C_WATER)) continue;
        }
        world_npc_walk(w, k, dir, 1);
        if(w->npc[k].moving) return true; // the step was accepted (a blocked one ends the walk at once)
    }
    return false;
}

static void npcs_update(World* w, uint32_t now, uint8_t* dirty, uint32_t* wake) {
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) {
        const NpcDef* n = &w->map.npcs[k];
        uint8_t kind = npc_behavior(n);
        if(kind == NPCB_ROAM) kind = NPCB_WANDER; // (npc_wander_step knows the roamer)
        if(kind != NPCB_TURN && kind != NPCB_WANDER) continue;
        if((kind == NPCB_TURN && n->param == 0) || !w->npc[k].visible || w->npc[k].moving) continue;
        uint32_t period = (uint32_t)(n->param ? n->param : 8) * 250u; // wander: 0 = 2 s
        if(kind == NPCB_WANDER && w->scr.active) { // no wandering during a cutscene
            w->npc_next[k] = now + period;
            continue;
        }
        if((int32_t)(now - w->npc_next[k]) >= 0) {
            if(kind == NPCB_TURN) {
                // turn to a different cardinal facing (0/2/4/6)
                uint8_t cur = w->npc[k].facing >> 1;
                uint8_t nf = (uint8_t)(((cur + 1 + world_rand(w) % 3) & 3) << 1);
                w->npc[k].facing = nf;
                w->npc_next[k] = now + period;
                *dirty |= D_WORLD;
            } else {
                if(npc_wander_step(w, k)) {
                    *dirty |= D_WORLD;
                    *wake = 1;
                }
                w->npc_next[k] = now + period / 2 + (world_rand(w) >> 8) % (period + 1);
            }
        }
        if(w->npc[k].moving) continue;
        uint32_t rem = w->npc_next[k] - now;
        if(*wake == 0 || rem < *wake) *wake = rem ? rem : 1;
    }
}

// ---------------------------------------------------------------- zone banner (contract 22)
_Static_assert(TXT_GL_AREA_LAST - TXT_GL_AREA_VILLA_ALISO == AREA_COUNT - 1, "gl.area.* texts must be consecutive (assets/text, tools/mapc.py AREA_KEYS)");

int world_banner_off(const World* w) { // rows on the screen: min(steps since the start, steps to the end, BANNER_STEPS) * BANNER_STEP_PX
    if(!w->area_on) return 0;
    uint32_t t = w->aclk - w->area_t0, k = t >> BANNER_STEP_SHIFT, r = t < BANNER_TOTAL_MS ? (BANNER_TOTAL_MS - t) >> BANNER_STEP_SHIFT : 0;
    if(r < k) k = r;
    if(k > BANNER_STEPS) k = BANNER_STEPS;
    return (int)(k * BANNER_STEP_PX);
}

// Starts the banner of a new zone (no script running: the next update tries again), ends it, asks for the redraw when the slide moved.
static __attribute__((always_inline)) inline void banner_update(World* w, uint8_t* dirty, uint32_t* wake) {
    uint8_t a = w->map.area_id;
    if(a && a != w->area_last && !w->scr.active) {
        w->area_last = a;
        w->area_t0 = w->aclk;
        w->area_on = text_load((uint16_t)(TXT_GL_AREA_VILLA_ALISO + a - 1), w->area_txt, AREA_TXT_CAP) > 0; // (an id past the table: -1, nothing)
        text_prefetch(w->area_txt); // a card-backed font (hanzi) must have the glyphs before the first draw
    }
    uint32_t t = w->aclk - w->area_t0;
    if(w->area_on && t >= BANNER_TOTAL_MS) w->area_on = 0;
    uint8_t off = (uint8_t)world_banner_off(w);
    if(off != w->area_off) {
        w->area_off = off;
        *dirty |= D_COMPOSE;
    }
    uint32_t next = (1u << BANNER_STEP_SHIFT) - (t & ((1u << BANNER_STEP_SHIFT) - 1)); // ms to the next slide step
    if(w->area_on && (*wake == 0 || next < *wake)) *wake = next;
}

uint8_t world_update(World* w, uint32_t dt_ms, uint8_t held, uint32_t* wake_ms) {
    Hero* h = &w->hero;
    uint8_t dirty = 0;
    uint32_t wake = 0;
    if(w->just_loaded) { // the load itself took SD time: do not count it
        w->just_loaded = false;
        dt_ms = 0;
        dirty |= D_WORLD;
    }
    w->aclk += dt_ms;
    w->wclock += dt_ms < WCLOCK_MAX_DT ? dt_ms : WCLOCK_MAX_DT;
    const uint32_t now = w->wclock;
    if(w->map.w == 0) { // no map (load failed): nothing to move
        *wake_ms = 0;
        return dirty;
    }
    if(w->wild_pending) wild_done(w); // a wild battle just ended: blackout when it was lost, else the post-battle chain
    else if(!w->pb) pb_arm_trainer(w); // a trainer battle (a script waits for it): the leveled mask is set
    if(w->pb && pb_step(w)) { // a scene was pushed (nickname / box / evolution): the world waits under it
        *wake_ms = 1;
        return dirty | D_WORLD;
    }
    if(w->bo) { // blackout: dialogs, then the fade home (the hero is stopped, nothing else moves)
        blackout_step(w);
        *wake_ms = 1;
        return dirty | D_WORLD;
    }
    if(w->enter_pending) { // a map was just loaded and faded in: on_enter script
        w->enter_pending = false;
        if(w->map.n_scripts && script_start_enter(w)) dirty |= D_WORLD;
        else if(trainer_sight(w)) dirty |= D_WORLD; // arriving in a trainer's line of sight (a warp lands there)
    }
    if(w->act) { // C5 (contract 42): the answer of a prompt / the fishing cast / a boulder that landed, continued in the PC plugin (a scene it pushed pauses the world; a step it started goes on below)
        const uint8_t a = w->act;
        w->act = 0;
        feat(w, a, 0);
    }
    const int8_t want = script_input_locked(&w->scr) ? -1 : dir_from_keys(held);
    uint8_t old_frame = h->frame, old_facing = h->facing;

    if(h->stepping) {
        for(int guard = 0; guard < 4 && h->stepping; guard++) {
            if(now - h->t0 < h->dur && now >= h->t0) break; // still in the step
            // ---- arrival
            uint32_t end = h->t0 + h->dur;
            fol_trail(w);
            h->ti = h->to_i;
            h->tj = h->to_j;
            h->walk_px += h->step_px;
            h->stepping = false;
            h->hop = false;
            h->lift = 0;
            if(w->scr.hsteps) { // scripted walk: no triggers, no input, chain the next forced step
                dirty |= D_WORLD;
                if(--w->scr.hsteps > 0) {
                    int ni = h->ti + DI[w->scr.hdir], nj = h->tj + DJ[w->scr.hdir];
                    if(map_in(&w->map, ni, nj)) begin_step(w, w->scr.hdir, ni, nj, end);
                    else w->scr.hsteps = 0;
                }
                if(!h->stepping) break;
                continue;
            }
            surf_check(w); // C5: a step onto land gets him off the board
            daycare_on_step(); // C3 (contract 33): one completed hero step feeds the creature in care
            roam_tick(w); // contract 55: the roamers' respawn counters
            rustle_tick(w); // contract 71
            bool stop = poison_step(w, on_step_arrival(w, h->ti, h->tj));
            if(stop) h->sl_n = 0; // (a warp / a script / a trainer took over: no run goes on)
            else if(floor_push(w, end)) { // C4-A: an arrow floor pushes him on (input is ignored, a buffered OK is dropped)
                w->ok_buf = false;
                dirty |= D_WORLD;
                continue;
            }
            if(w->ok_buf) {
                w->ok_buf = false;
                if(!stop) stop = on_interact_pressed(w);
            }
            dirty |= D_WORLD;
            if(stop || want < 0) {
                h->cand = -1;
                h->turned = false;
                break;
            }
            // chained step: starts at the previous step's end time, no delays
            h->cand = want;
            h->turned = true;
            h->turn_t = end;
            if(!try_step(w, (uint8_t)want, end, false)) break; // bump
        }
    } else {
        if(want < 0) {
            h->cand = -1;
            h->turned = false;
        } else if(want != h->cand) {
            h->cand = want;
            h->cand_t = now;
            h->turned = false;
            wake = settle_ms(want);
        } else if(!h->turned) {
            uint32_t el = now - h->cand_t;
            const uint32_t settle = settle_ms(want);
            if(el >= settle) {
                if(h->facing != (uint8_t)want) {
                    h->facing = (uint8_t)want; // turn first; walk only if still held after TURN_MS
                    h->turned = true;
                    h->turn_t = now;
                    wake = TURN_MS;
                } else {
                    h->turned = true;
                    h->turn_t = now;
                    try_step(w, (uint8_t)want, now, false);
                }
            } else {
                wake = settle - el;
            }
        } else if(now - h->turn_t >= TURN_MS) {
            try_step(w, (uint8_t)want, now, false); // a held blocked key just bumps again (no redraw)
        } else {
            wake = TURN_MS - (now - h->turn_t);
        }
    }

    if(npcs_logic(w, now)) dirty |= D_WORLD;
    if(w->scr.active) { // the VM runs after the walks were advanced, so it sees them finished
        uint32_t swake = 0;
        if(script_run(w, &swake)) dirty |= D_WORLD;
        if(swake && (wake == 0 || swake < wake)) wake = swake;
        if(!w->map.blk) { // the script opened a battle: the map's data is dropped until it ends (world_art_release), read nothing of it
            *wake_ms = 0;
            return dirty;
        }
        if(w->bo) { // the script lost a trainer battle (native 19): the blackout runs at the next update
            *wake_ms = 1;
            return dirty | D_WORLD;
        }
    }
    hero_pose(w, now);
    npcs_pose(w, now);
    if(h->stepping || world_npcs_busy(w)) {
        dirty |= D_WORLD;
        wake = 1;
    }
    if(h->frame != old_frame || h->facing != old_facing) dirty |= D_WORLD;

    uint32_t nwake = 0;
    npcs_update(w, now, &dirty, &nwake);
    if(nwake && (wake == 0 || nwake < wake)) wake = nwake;
    nwake = 0;
    rustle_update(w, &dirty, &nwake);
    if(nwake && (wake == 0 || nwake < wake)) wake = nwake;
    if(w->enc_pending) { // an encounter decided at the arrival: the battle opens now that the hero is in his final pose
        w->enc_pending = false;
        if(world_start_wild(w, w->enc_sp, w->enc_lv, -1)) dirty |= D_WORLD;
    }
    banner_update(w, &dirty, &wake);
    if(w->map.flags & MAPF_ANIM) { // animated tiles / objects: redraw when the frame changes, wake for the next change (maps without them never wake)
        uint32_t st = w->aclk / ANIM_MS, r = ANIM_MS * (st + 1) - w->aclk;
        if(st != w->anim_step) {
            w->anim_step = st;
            dirty |= D_WORLD;
        }
        if(wake == 0 || r < wake) wake = r;
    }
    if(w->map.flags & MAPF_FOG) { // drifting mist (contract 40.5): a redraw at every FOG_MS boundary of the animation clock (the pass reads the clock itself in world_render_hero); the other maps never wake
        dirty |= D_WORLD;
        uint32_t r = FOG_MS - (w->aclk & (FOG_MS - 1));
        if(wake == 0 || r < wake) wake = r;
    }
    *wake_ms = wake;
    return dirty;
}

// ---------------------------------------------------------------- render helpers
void world_render_hero(const World* w, RenderHero* out) {
    const Hero* h = &w->hero;
    out->draw = true;
    out->dir = h->facing;
    out->frame = h->frame;
    out->fi = h->fi;
    out->fj = h->fj;
    // Depth box = the tile the hero is nearer to (from-tile for the first half of a step, then the
    // destination). The union of both tiles drew him over furniture he was still behind.
    uint8_t ti = h->ti, tj = h->tj;
    if(h->stepping) { // the nearest tile to the real position (a hop is two tiles long: start, ledge, landing)
        ti = (uint8_t)((h->fi + 128) >> 8);
        tj = (uint8_t)((h->fj + 128) >> 8);
    }
    out->i0 = out->i1 = ti;
    out->j0 = out->j1 = tj;
    out->lift = h->lift;
    out->rustle_ph = w->rustle_n ? w->rustle_ph : 0;
    out->rustle_i = w->rustle_i;
    out->rustle_j = w->rustle_j;
    out->balloon = (w->balloon && (int32_t)(w->wclock - w->balloon_until) < 0) ? w->balloon : 0;
    out->bike = bike_on(w) ? 1 : (uint8_t)(surf_on(w) << 1); // C5: the hero's sprite set: 0 walking, 1 bicycle, 2 surfing (render.h)
    out->fog = (w->map.flags & MAPF_FOG) ? (uint8_t)(1 + ((w->aclk >> FOG_SHIFT) & 31)) : 0; // C4b: the mist's drift step (FOG_COV has a period of 32 rows)
    out->dark = ((w->map.flags & MAPF_DARK) && !world_module_has(w, MOD_LINTERNA)) ? DARK_R : 0; // C4-A: the Linterna lights the cave, without it only a small diamond around the hero
}

void world_camera(const World* w, int* camx, int* camy) {
    if(w->cam_npc >= 0 && w->cam_npc < w->map.n_npcs && w->cam_npc < WORLD_MAX_NPCS) {
        const NpcState* s = &w->npc[w->cam_npc];
        render_camera(s->fi, s->fj, camx, camy); // script camera: follows the NPC (cut, no pan)
        *camy -= w->cam_dy;
        return;
    }
    render_camera(w->hero.fi, w->hero.fj, camx, camy);
}

// ---------------------------------------------------------------- hooks (B2)
static bool say(World* w, uint16_t text_id, int8_t push_back) {
    if(!w->sc) return false;
    world_stop(w);
    return scene_dialog_open(w->sc, text_id, push_back);
}

// Starts script `id` (its condition decides); the hero stops where he is. Returns true when it started.
static bool run_script(World* w, uint16_t id) {
    if(id >= w->map.n_scripts || !script_start(w, (uint8_t)id)) return false;
    world_stop(w);
    return true;
}

// Step-on interactions (mode 1) of the tile just arrived at.
static bool step_on(World* w, int ti, int tj) {
    const InteractDef* d = interact_at(&w->map, ti, tj, INT_MODE_STEP, w->hero.facing);
    if(!d) return false;
    uint16_t tid = interact_text_id(d);
    const uint8_t kd = interact_kind(d);
    if(kd == INT_SCRIPT) return run_script(w, tid);
    if(kd == INT_SET_FLAG) world_flag_set(w, interact_flag(d), true);
    int8_t pb = kd == INT_PUSH ? (int8_t)((w->hero.facing + 4) & 7) : (int8_t)-1;
    return say(w, tid, pb);
}

// ---------------------------------------------------------------- wild encounters
#ifndef NUCLEO_DEVICE
int g_open_on; // host tests only (contract 63): the open-ground roll is OFF unless a test turns it on (the campaign walks and the route walkers depend on the RNG streams, which a roll on every step of a route would shift)
static uint32_t s_rolls, s_hits;
void world_enc_stats(uint32_t* rolls, uint32_t* hits) {
    *rolls = s_rolls;
    *hits = s_hits;
}
#endif

// Decides an encounter for the tile just arrived at; the battle itself opens at the end of the update (enc_pending).
static bool encounter_check(World* w, int ti, int tj) {
    const Map* m = &w->map;
    if(w->enc_grace) { // the first step after a load / a battle never rolls
        w->enc_grace--;
        return false;
    }
    if(w->rustle_n && w->rustle_i == ti && w->rustle_j == tj) { // contract 71: he stepped onto the rustling tile: the casino plugin picks the special creature (no rate roll) and queues the battle
        w->rustle_n = 0;
        feat(w, WACT_RUSTLE, 1);
        return w->enc_pending;
    }
    const EncDef* tb = m->enc; // the tall grass's table; surfing (C5, contract 42.1): the water's, on a water tile
    uint8_t n = m->n_enc, rate = m->enc_rate;
    const uint8_t cf = cflags_at(m, ti, tj);
    if(cf & C_WATER) tb = map_table(m, TBL_WATER, &n, &rate);
    else if(!(cf & C_GRASS)) { // contract 63: open ground (no flag at all) rolls the grass table at the map's open_rate; any other flagged tile (counter, warp, trigger, ledge, arrow floor) never
        if(cf) return false;
        rate = m->open_rate;
#ifndef NUCLEO_DEVICE
        if(!g_open_on) rate = 0;
#endif
    }
    if(!rate || !n) return false;
    uint8_t lead = world_party_ready_hook ? world_party_ready_hook() : 0; // level of the first creature able to fight, 0 = none
    if(!lead || !world_wild_hook) return false;
#ifndef NUCLEO_DEVICE
    s_rolls++;
#endif
    if(((world_rand(w) >> 16) & 0xFF) >= rate) return false;
#ifndef NUCLEO_DEVICE
    s_hits++;
#endif
    uint32_t sum = 0;
    for(uint8_t k = 0; k < n; k++) sum += tb[k].weight;
    uint32_t r = (world_rand(w) >> 8) % sum;
    const EncDef* e = &tb[0];
    for(uint8_t k = 0; k < n; k++) {
        e = &tb[k];
        if(r < e->weight) break;
        r -= e->weight;
    }
    uint32_t lv = e->lv_min + (world_rand(w) >> 8) % (uint32_t)(e->lv_max - e->lv_min + 1);
    if(lv > 100) lv = 100;
    if(w->vars[VAR_REPEL] && lv < lead) return false; // Ward Spray (C2-S1): nothing below the first able creature's level appears
    w->enc_sp = e->species;
    w->enc_lv = (uint8_t)lv;
    w->enc_pending = true;
    return true;
}

// ---------------------------------------------------------------- trainers (contract 18)
// A trainer (NpcDef behavior NPCB_TRAINER, range = the high nibble) sees the hero when he stands on the straight line of the trainer's
// facing within range tiles: the ray steps (DI, DJ) of that facing, i.e. a world axis for the odd facings and a screen-cardinal 2-tile
// diagonal for the even ones (the 4-direction sprites face S / E / N / W). Every tile before the hero must be enterable (not solid, not
// a ledge, no solid NPC) and a diagonal step also needs both flank tiles free (the walking rule of npc_try_step), so the trainer can
// always walk up to him. Water and grass do not block the view.
static bool sight_free(const World* w, uint8_t self, int i, int j) {
    if(cflags_at(&w->map, i, j) & (C_SOLID | C_LEDGE)) return false;
    int o = npc_at(&w->map, i, j, w->npc);
    return o < 0 || o == self;
}

// Distance at which trainer k sees the hero (1..range), 0 = it does not.
static int trainer_sees(const World* w, uint8_t k) {
    const NpcState* s = &w->npc[k];
    const uint8_t f = s->facing & 7;
    const int di = DI[f], dj = DJ[f], range = npc_radius(&w->map.npcs[k]);
    int ci = s->i, cj = s->j;
    for(int d = 1; d <= range; d++) {
        int ni = ci + di, nj = cj + dj;
        if(ni == w->hero.ti && nj == w->hero.tj) return d;
        if(!sight_free(w, k, ni, nj)) return 0;
        if(!(f & 1) && (!sight_free(w, k, ni, cj) || !sight_free(w, k, ci, nj))) return 0;
        ci = ni;
        cj = nj;
    }
    return 0;
}

// The hero has arrived / appeared on a tile: starts the sight script of the first unbeaten trainer that sees him.
static bool trainer_sight(World* w) {
    if(w->scr.active || w->bo || !w->map.n_scripts) return false;
    for(uint8_t k = 0; k < w->map.n_npcs && k < WORLD_MAX_NPCS; k++) {
        const NpcDef* n = &w->map.npcs[k];
        const NpcState* s = &w->npc[k];
        if(npc_behavior(n) != NPCB_TRAINER || !s->visible || s->moving || world_flag_get(w, npc_talk_flag(n))) continue;
        if(trainer_sees(w, k)) return run_script(w, npc_text_id(n));
    }
    return false;
}

// Distance from NPC k to the hero along a straight line (8 directions), 0 when they are not in line.
static uint8_t line_dist(const World* w, uint8_t k) {
    int di = w->hero.ti - w->npc[k].i, dj = w->hero.tj - w->npc[k].j;
    int ai = abs(di), aj = abs(dj);
    return (di == 0 || dj == 0 || ai == aj) ? (uint8_t)(ai > aj ? ai : aj) : 0;
}

uint8_t world_trainer_spot(World* w, uint8_t k) {
    if(k >= w->map.n_npcs || k >= WORLD_MAX_NPCS) return 1;
    uint8_t d = line_dist(w, k);
    if(d <= 1) return 1; // adjacent (a talk), or not in line (cannot happen when the trainer saw him)
    w->balloon = (uint8_t)(k + 1);
    w->balloon_until = w->wclock + BALLOON_MS;
    w->cam_dy = BALLOON_CAM_DY; // (the script cuts the camera to the trainer next; script_stop / cam_hero clear this)
    return d;
}

void world_trainer_come(World* w, uint8_t k) {
    if(k >= w->map.n_npcs || k >= WORLD_MAX_NPCS) return;
    uint8_t d = line_dist(w, k);
    int8_t f = dir_toward(w->npc[k].i, w->npc[k].j, w->hero.ti, w->hero.tj);
    if(d > 1 && f >= 0) world_npc_walk(w, k, (uint8_t)f, (uint8_t)(d - 1));
}

bool on_step_arrival(World* w, int ti, int tj) {
    if(!w->sc || w->scr.active) return false; // no warps / triggers while a script runs
    const WarpDef* wp = warp_at(&w->map, ti, tj);
    if(wp && ((wp->req_dirs >> (w->hero.facing & 7)) & 1)) {
        if(wp->flags & WARPF_NO_FADE) {
            world_stop(w);
            if(w->map.w && w->map.id == wp->target_map)
                world_place(w, wp->target_i, wp->target_j, wp->target_facing);
            else
                world_load_map(w, wp->target_map, wp->target_i, wp->target_j, wp->target_facing);
            return true;
        }
        if(scene_fade_warp(w->sc, wp->target_map, wp->target_i, wp->target_j, wp->target_facing))
            return true;
    }
    if(reserva_on_step(w)) return true; // C5-B (contract 43): the Reserva Salvaje's step counter
    if(step_on(w, ti, tj)) return true;
    if(trainer_sight(w)) return true; // a trainer who saw you wins over a wild roll
    if(w->vars[VAR_REPEL] && !--w->vars[VAR_REPEL] && say(w, TXT_GL_REPEL_END, -1)) return true; // Ward Spray (C2-S1): one step counted; it ran out
    return encounter_check(w, ti, tj);
}

// Index of the visible solid NPC on (i,j), or -1.
static int probe_npc(const World* w, int i, int j) {
    const int k = npc_at(&w->map, i, j, w->npc);
    if(k >= 0 || w->fol_i != i || w->fol_j != j) return k;
    NpcState f;
    world_fol_pose(w, &f);
    return f.visible ? WORLD_MAX_NPCS : -1; // contract 69: the follower's tile answers OK too, but only while it is SHOWN (contract 72: a hidden one - bicycle, sea, a wall behind him - swallowed an interaction on its tile)
}

// Turn NPC k to face the hero (quantized to the directions of its sprite bank entry).
static void npc_face_hero(World* w, int k) {
    const NpcDef* n = &w->map.npcs[k];
    int8_t f = dir_toward(w->npc[k].i, w->npc[k].j, w->hero.ti, w->hero.tj);
    if(f < 0) f = (int8_t)((w->hero.facing + 4) & 7);
    const SprEntry* se = bank_spr(w->map.bank, n->spr);
    uint8_t nd = se ? se->ndirs : 8;
    if(nd == 4 && (f & 1)) f = (int8_t)((f + 1) & 7); // same rounding the renderer applies
    if(nd == 1) f = (int8_t)(n->facing & 7);
    w->npc[k].facing = (uint8_t)f;
}

void world_npc_face_hero(World* w, uint8_t k) {
    if(k < w->map.n_npcs && k < WORLD_MAX_NPCS) npc_face_hero(w, k);
}

// Talks to NPC k (talk rule: talk_flag set -> text_id+1, then set the flag).
static bool talk_npc(World* w, int k) {
    if(k == WORLD_MAX_NPCS) return feat(w, WACT_FOLLOW, 0); // contract 69: OK facing the follower: its emote and one line (menu_open_daycare -> the casino plugin, scene_follow.c)
    const NpcDef* n = &w->map.npcs[k];
    npc_face_hero(w, k);
    const uint8_t kind = npc_behavior(n);
    if(kind == NPCB_BOULDER || kind == NPCB_ROAM) { // C5 (contract 42.2): OK on a boulder asks for Empuje (the PC plugin); contract 55: OK on a roaming wild creature is a touch too
        feat(w, kind == NPCB_ROAM ? WACT_ROAM_HIT : WACT_BOULDER, (uint8_t)k);
        return true;
    }
    uint16_t tid = npc_text_id(n);
    if(npc_has_script(n)) {
        run_script(w, tid); // the script's condition may refuse: the NPC then only turns
        return true;
    }
    if(tid == 0xFFFF) return true; // silent NPC: it only turns
    const uint16_t tf = npc_talk_flag(n);
    if(tf) {
        if(world_flag_get(w, tf)) tid++;
        else world_flag_set(w, tf, true);
    }
    say(w, tid, -1);
    return true;
}

// First NPC / face-mode interaction on (i,j); (di,dj) is the probe direction used to reach over a
// counter. Returns true when it was handled.
static bool probe_tile(World* w, int i, int j, int di, int dj) {
    for(int pass = 0; pass < 2; pass++) { // pass 1: one tile further over a counter
        if(pass == 1) {
            if(!(cflags_at(&w->map, i, j) & C_COUNTER)) return false;
            i += di;
            j += dj;
        }
        int k = probe_npc(w, i, j);
        if(k >= 0) return talk_npc(w, k);
        const InteractDef* d = interact_at(&w->map, i, j, INT_MODE_FACE, w->hero.facing);
        if(d) {
            const uint8_t kd = interact_kind(d);
            if(kd == INT_SCRIPT) {
                run_script(w, interact_text_id(d));
                return true;
            }
            if(kd == INT_SET_FLAG) world_flag_set(w, interact_flag(d), true);
            say(w, interact_text_id(d), -1);
            return true;
        }
    }
    return false;
}

bool on_interact_pressed(World* w) {
    if(!w->sc || w->map.w == 0) return false;
    const Hero* h = &w->hero;
    int di = DI[h->facing & 7], dj = DJ[h->facing & 7];
    int ti = h->ti, tj = h->tj;
    if(probe_tile(w, ti + di, tj + dj, di, dj)) return true;
    if(!(h->facing & 1) && (probe_tile(w, ti + di, tj, di, 0) || probe_tile(w, ti, tj + dj, 0, dj))) return true; // (odd facing: only the tile in front)
    if(world_module_has(w, MOD_HIDRO) && (cflags_at(&w->map, ti + di, tj + dj) & C_WATER)) return feat(w, WACT_WATER, 0); // C5 (contract 42.1): OK on the water with the module: Hidrodeslizador?
    return false;
}
