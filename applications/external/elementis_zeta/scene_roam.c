// The roaming wild creatures' placement and touch (contract 55), a part of the CASINO plugin since contract 69 (they were the PC plugin's world_feat; the PC plugin gave them up for the room: its DEV `pc_open` row had 159 B of margin and
// the casino plugin, the smallest one, has 10 KB). world.c keeps only the hooks (the wander rule, the touch, the step counters) and asks `feat()`: the casino native with the operation CAS_ROAM (natives_shop.c nat_pc, a plugin load
// per call: a placement when a roamer is due, a touch). What is chosen here is rare: a placement when a roamer is due (species and level from the map's grass table, a cell of the roam area OUT OF VIEW) and a touch (hide it, arm its
// respawn, queue the battle). The host harnesses link this file like the plugin.
#include <stdlib.h>
#include "world.h"
#include "party.h"
#include "battle_core.h"
#include "gen/maps_gen.h"

// One weighted pick of an encounter slot and its level (the grass / the fishing tables' rule): the slot, its level in *lv.
static const EncDef* roam_pick(const EncDef* t, uint8_t n, uint8_t* lv) {
    uint32_t* rng = party_rng();
    uint32_t sum = 0;
    for(uint8_t k = 0; k < n; k++)
        sum += t[k].weight;
    uint32_t r = bc_rand_range(rng, 0, sum - 1);
    for(; r >= t->weight; t++)
        r -= t->weight;
    *lv = (uint8_t)bc_rand_range(rng, t->lv_min, t->lv_max);
    return t;
}

// A free cell of the roam area, out of view (which also keeps it off the hero and the tile he steps to): a tile (a, b) from the hero's tile has its feet at screen (64 + (a - b) * 14, 33 + (a + b) * 7); a sprite
// of <= 34 x 34 px is on the 128 x 64 screen only while |a - b| < 6 and -5 < a + b < 10; one more tile of margin is kept so a creature never pops in.
static bool roam_cell(const World* w, int i, int j) {
    const int a = i - w->hero.ti, b = j - w->hero.tj;
    return map_roam_ok(&w->map, i, j) && (abs(a - b) >= 7 || a + b <= -6 || a + b >= 11) &&
           world_can_enter(w, i, j);
}

#ifndef NUCLEO_DEVICE
int g_roam_off =
    1; // host tests only (contract 63: ON by default = no roamer is ever placed, so the old harnesses keep their RNG streams and open corridors now that 42 maps have roamers; roam_check.c turns them on). cp_saves.c walks a whole campaign whose recorded fights depend on the RNG streams; roamers would shift them (the placement draws from the party RNG, their wander from the world's)
#endif
static void roam_place(
    World* w,
    uint8_t k) { // roamer k is due: place it within 16 tiles of the hero, or wait 6 steps more
    const uint8_t r = (uint8_t)(k - (w->map.n_npcs - w->roam_n));
    uint32_t* rng = party_rng();
    int i, j, t = 0;
    w->roam_cnt[k] = 6;
#ifndef NUCLEO_DEVICE
    if(g_roam_off) return;
#endif
    if(!(*world_party_ready_hook)()) return; // (nobody can fight: no creature comes)
    do {
        i = w->hero.ti + (int)bc_rand_range(rng, 0, 32) - 16;
        j = w->hero.tj + (int)bc_rand_range(rng, 0, 32) - 16;
    } while(!roam_cell(w, i, j) && ++t < 40);
    if(t == 40) return;
    const EncDef* d;
    do
        d = roam_pick(w->map.enc, w->map.n_enc, &w->roam_lv[r]);
    while(
        map_roam_spr(&w->map, (uint8_t)(d - w->map.enc)) ==
        MAP_ROAM_NONE); // (contract 66: the Solar form of an affinity pair never roams: mapc keeps one slot at least)
    NpcState* s =
        &w->npc[k]; // (a hidden roamer is at rest: moving / frame / lift are 0; its wander timer is long past: it takes its first step at once)
    s->i = s->to_i = s->near_i = (uint8_t)i;
    s->j = s->to_j = s->near_j = (uint8_t)j;
    s->fi = i * 256;
    s->fj = j * 256;
    s->facing = (uint8_t)(i + j) & 7;
    s->spr = map_roam_spr(&w->map, (uint8_t)(d - w->map.enc));
    s->visible = 1;
    w->npc_force[k] = 2; // (shown whatever its show flag says)
    w->roam_sp[r] = d->species;
    w->roam_cnt[k] = 0;
}

static void roam_hit(
    World* w,
    uint8_t
        k) { // the hero touched roamer k: it is gone (a new one comes ROAM_WAIT_STEPS steps later); the battle unless nobody can fight / the Ward Spray scares it off
    const uint8_t r = (uint8_t)(k - (w->map.n_npcs - w->roam_n)),
                  lead = (*world_party_ready_hook)();
    if(!lead) return; // (nobody can fight: it only blocks the way)
    NpcState* s = &w->npc[k];
    s->visible = s->moving = 0;
    w->npc_force[k] = 0;
    w->npcw[k].steps = 0;
    w->roam_cnt[k] = ROAM_WAIT_STEPS;
    if(w->vars[VAR_REPEL] && w->roam_lv[r] < lead) return;
    w->enc_sp = w->roam_sp[r];
    w->enc_lv = w->roam_lv[r];
    w->enc_pending = true; // (the shaking tile is dropped by wild_done)
}

// ---------------------------------------------------------------- the rustling grass (contract 71)
// Can tile (i, j) rustle now? Tall grass and nothing else (no solid, ledge, door, trigger or water), in view (the hero's feet are at screen (64, 33): a tile (a, b) from him has its centre at (64 + (a - b) * 14, 32 + (a + b) * 7),
// so |a - b| <= 3 and -3 <= a + b <= 3 keep the whole 28 x 15 diamond inside 128 x 64), not his tile or one of the 8 around it (the follower stands on the trail behind him, so it is never under the tile, and his next step cannot
// reach it), nobody on it.
static bool rustle_cell(const World* w, int i, int j) {
    const int a = i - w->hero.ti, b = j - w->hero.tj;
    const uint8_t cf = cflags_at(&w->map, i, j);
    if(!(cf & C_GRASS) || (cf & (C_WARP | C_TRIGGER | C_WATER))) return false;
    if(abs(a) < 2 && abs(b) < 2) return false;
    if(abs(a - b) > 3 || abs(a + b) > 3) return false;
    return world_can_enter(w, i, j); // (no solid, no ledge, nobody on it)
}

// A new rustling tile (arg 0 of WACT_RUSTLE: world.c rustle_tick rolled 1 in 64 on a step). Not on an interior or a dark map, not inside the Reserva's session (its balls and steps have their own rules), not with Ward Spray running,
// not with nobody able to fight, not during a script, not while one is already on. Two passes over the tiles near him (count, then the k-th): no candidate list on the stack. Nothing found = nothing happens (the next roll is 1 in 64 again).
static void rustle_spawn(World* w) {
    const Map* m = &w->map;
    if(!m->enc_rate || !m->n_enc || (m->flags & (MAPF_INTERIOR | MAPF_DARK)) ||
       party_state()->rv_steps || w->vars[VAR_REPEL] || w->scr.active || w->rustle_n ||
       !(*world_party_ready_hook)())
        return;
    const int hi = w->hero.ti, hj = w->hero.tj;
    int n = 0;
    for(int dj = -3; dj <= 3; dj++)
        for(int di = -3; di <= 3; di++)
            n += rustle_cell(w, hi + di, hj + dj);
    if(!n) return;
    int k = (int)bc_rand_range(party_rng(), 0, (uint32_t)n - 1);
    for(int dj = -3; dj <= 3; dj++)
        for(int di = -3; di <= 3; di++)
            if(rustle_cell(w, hi + di, hj + dj) && k-- == 0) {
                w->rustle_i = (uint8_t)(hi + di);
                w->rustle_j = (uint8_t)(hj + dj);
                w->rustle_n = (uint8_t)(RUSTLE_STEPS +
                                        bc_rand_range(party_rng(), 0, RUSTLE_STEPS_SPREAD - 1));
                w->rustle_ph = 1;
                return;
            }
}

// The hero stepped onto the rustling tile (arg 1; world.c already dropped it): the creature of the map's grass table with the LOWEST weight (every slot that ties for it is as likely as another), at the table's highest level + 3 .. 5
// (at most 100). The base species of the slot: the Solar / Lunar remap (files/aff.bin) is applied by scene_battle_wild when the fight opens, as for any wild creature, and there is no extra variocolor chance (battle_open does the usual roll).
// The Ward Spray rule is the grass's own: nothing below the first able creature's level (a Ward Spray drunk after the tile started rustling can scare it off).
static void rustle_hit(World* w) {
    const uint8_t lead = (*world_party_ready_hook)(), n = w->map.n_enc;
    const EncDef* t = w->map.enc;
    if(!lead || !n) return;
    uint8_t wmin = 255, lmax = 0, c = 0;
    for(uint8_t k = 0; k < n; k++) {
        if(t[k].weight && t[k].weight < wmin) wmin = t[k].weight;
        if(t[k].lv_max > lmax) lmax = t[k].lv_max;
    }
    for(uint8_t k = 0; k < n; k++)
        c += t[k].weight == wmin;
    if(!c) return;
    uint32_t r = bc_rand_range(party_rng(), 0, c - 1u);
    for(;; t++)
        if(t->weight == wmin && !r--) break;
    uint32_t lv = lmax + RUSTLE_LV_MIN + bc_rand_range(party_rng(), 0, RUSTLE_LV_SPREAD - 1);
    if(lv > 100) lv = 100;
    if(w->vars[VAR_REPEL] && lv < lead) return;
    w->enc_sp = t->species;
    w->enc_lv = (uint8_t)lv;
    w->enc_pending = true;
}

// `act` WACT_ROAM: a placement is due; WACT_ROAM_HIT: the hero touched roamer `arg`; WACT_RUSTLE: a new rustling tile (arg 0) or the step onto it (arg 1). Never pushes a scene.
#ifndef NUCLEO_DEVICE
unsigned
    g_rustle_asks; // host tests (contract 72): how many times the world asked this plugin for a new rustling tile
#endif
bool roam_feat(uint8_t act, uint8_t arg) {
    World* w = g_w;
#ifndef NUCLEO_DEVICE
    g_rustle_asks += act == WACT_RUSTLE && !arg;
#endif
    if(act ==
       WACT_ROAM) { // every roamer that is hidden and not waiting gets a creature (out of view, or 6 steps' more wait)
        for(uint8_t k = (uint8_t)(w->map.n_npcs - w->roam_n); k < w->map.n_npcs; k++)
            if(!w->npc[k].visible && !w->roam_cnt[k]) roam_place(w, k);
    } else if(act == WACT_RUSTLE) {
        if(arg)
            rustle_hit(w);
        else
            rustle_spawn(w);
    } else {
        roam_hit(w, arg);
    }
    return false;
}
