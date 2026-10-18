// The elevator's floor list (C4-A, contract 35.5), the Planeador's list (C6) and the world features of chapters 5 / 6 (world_feat: the surf / Empuje prompts, the fishing cast, the gift to the box), screens and code
// of the PC plugin. The TRAINER CARD that used to be the first half of this file is scene_tcard.c (the casino plugin's since contract 74).
#include <stdlib.h>
#include <string.h>
#include "scene_card.h"
#include "party.h"
#include "bag.h"
#include "save.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "sound.h"
#include "world.h"
#include "pc_host.h" // world_feat (C5)
#include "battle_core.h" // bc_rand8 / bc_rand_range (the fishing roll)
#include "gen/maps_gen.h"
#ifndef NUCLEO_DEVICE
bool roam_feat(uint8_t act, uint8_t arg); // scene_roam.c
#endif

#define CAP 16

// ---------------------------------------------------------------- C4-A (contract 35.5): the elevator's floor list (a screen of the PC plugin, opened by native 23 op 13 through menu_open_list)
// A small list picker for the scripts (mapc `elevator VAR KEY1..KEY5`, native 23 op 13): 2..5 rows whose text ids the script stored in World.tr (tr[2k], tr[2k + 1], tr[10] = rows). A framed box at the
// top right over the frozen world (no text box: a `say` before it asks the question); Up / Down move (wraps), OK writes the row to script variable `var` and pops, Back writes 255. It lives in this
// file because the PC plugin has the most heap headroom (the UI plugin's ui_open was the tightest row) and already links it (no new source in any Makefile / application.fam); the world scene below pauses and resumes the script.
#define LIST_ROWS 5 // rows on the screen at once (the elevator's maximum; more rows scroll)
#define LIST_MAX 16 // rows in all: C6 (contract 46) uses the list for the Planeador's towns (up to 16 visited)
#define LIST_ELEV 5 // the elevator's rows (World.tr holds 5 labels)
#define LIST_PAD_X 3 // inside the frame, left of the cursor
#define LIST_GAP 2
#define LIST_CAP 16 // codes of a label with its END ("Metropolis Neon" is 15)
typedef struct {
    tchar lab[LIST_ROWS][LIST_CAP]; // the labels of rows top .. top + vis - 1 (reloaded when the window scrolls)
    uint16_t id[LIST_MAX];          // the text id of every row
    uint8_t n, sel, top, vis, var;
    int16_t w, h;
} ListScene;

static ListScene g_ls;
static bool g_ls_live; // the list is on the stack (keeps the plugin loaded: scene_pc_active)

bool scene_list_active(void) {
    return g_ls_live;
}

static void l_enter(SceneCtx* c, void* d) {
    (void)d;
    g_ls_live = true;
    scene_dirty(c, D_WORLD);
}

static void l_leave(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    g_ls_live = false;
}

// loads the labels of the visible window (rows top .. top + vis - 1); a card-backed font (hanzi) gets its glyphs now
static void ls_fill(ListScene* l) {
    const tchar* all[LIST_ROWS];
    for(int r = 0; r < l->vis; r++) {
        if(text_load(l->id[l->top + r], l->lab[r], LIST_CAP) < 0) l->lab[r][0] = 0;
        all[r] = l->lab[r];
    }
    text_prefetch_many(all, l->vis);
}

// the rows' text ids are in g_ls.id[0 .. n - 1]
static bool ls_open(SceneCtx* c, uint8_t var, uint8_t n) {
    ListScene* l = &g_ls;
    if(!n || n > LIST_MAX || var >= SCRIPT_VARS || c->n >= MAX_SCENES) return false;
    l->n = n;
    l->sel = l->top = 0;
    l->var = var;
    int wmax = 0;
    for(int k = 0; k < n; k++) { // the frame fits the widest label
        if(text_load(l->id[k], l->lab[0], LIST_CAP) < 0) l->lab[0][0] = 0;
        const int w = font_text_width(l->lab[0]);
        if(w > wmax) wmax = w;
    }
    const int pitch = font_pitch();
    l->vis = n < LIST_ROWS ? n : LIST_ROWS;
    while(l->vis > 2 && l->vis * pitch + 2 * TB_PAD_Y > SCREEN_H) l->vis--;
    l->w = (int16_t)(LIST_PAD_X + font_cell_w() + LIST_GAP + wmax + LIST_PAD_X + 1);
    l->h = (int16_t)(l->vis * pitch + 2 * TB_PAD_Y);
    ls_fill(l);
    return scene_push(c, &scene_list_ops, l);
}

bool scene_list_open(SceneCtx* c, uint8_t var) {
    const uint8_t* tr = c->world ? c->world->tr : NULL;
    if(!tr || tr[10] < 2 || tr[10] > LIST_ELEV) return false;
    for(int k = 0; k < tr[10]; k++) g_ls.id[k] = (uint16_t)(tr[2 * k] | (tr[2 * k + 1] << 8));
    return ls_open(c, var, tr[10]);
}

static void l_finish(SceneCtx* c, ListScene* l, uint8_t v, uint16_t snd) {
    c->world->vars[l->var] = v;
    sound_play(snd);
    scene_pop(c);
    scene_wake(c, 1); // the world resumes the script
}

static void l_input(SceneCtx* c, void* d, const InputEv* ev) {
    ListScene* l = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        else if(ev->type == EV_SHORT) l_finish(c, l, 255, SND_FX_CANCEL);
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        l->sel = (uint8_t)((l->sel + (ev->key == KEY_DOWN ? 1 : l->n - 1)) % l->n);
        const uint8_t top0 = l->top;
        if(l->sel < l->top) l->top = l->sel;
        if(l->sel >= l->top + l->vis) l->top = (uint8_t)(l->sel - l->vis + 1);
        if(l->top != top0) ls_fill(l);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        l_finish(c, l, l->sel, SND_FX_CONFIRM);
    }
}

static void l_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    ListScene* l = d;
    const int pitch = font_pitch(), x = SCREEN_W - l->w;
    ov_fill(x, 0, l->w, l->h, 0);
    ov_frame(x, 0, l->w, l->h, 1);
    for(int r = 0; r < l->vis; r++) {
        int k = l->top + r, ly = TB_PAD_Y + r * pitch;
        text_draw_line(l->lab[r], 0, x + LIST_PAD_X + font_cell_w() + LIST_GAP, ly, 99);
        if(l->sel == k) font_draw_glyph(GLYPH_CURSOR, x + LIST_PAD_X, ly, 1);
    }
}

const SceneOps scene_list_ops = {
    .enter = l_enter,
    .leave = l_leave,
    .input = l_input,
    .draw_world = NULL,
    .draw_overlay = l_draw_overlay,
    .pauses_below = true,
};

#ifndef NUCLEO_DEVICE
// Host tests (c4a_sys_check): the list on top of the stack. false when it is not; rows, cursor row, first visible row, visible rows, the text of row k.
bool scene_list_probe(const SceneCtx* c, int* n, int* sel, int* top, int* vis, const tchar** lab) {
    if(scene_top_ops(c) != &scene_list_ops) return false;
    *n = g_ls.n;
    *sel = g_ls.sel;
    *top = g_ls.top;
    *vis = g_ls.vis;
    *lab = &g_ls.lab[0][0];
    return true;
}
void scene_list_set_vis(uint8_t vis) { // host tests: pretend the font is taller (fewer rows fit), to exercise the scrolling
    g_ls.vis = vis;
    g_ls.h = (int16_t)(vis * font_pitch() + 2 * TB_PAD_Y);
    if(g_ls.sel < g_ls.top) g_ls.top = g_ls.sel;
    if(g_ls.sel >= g_ls.top + g_ls.vis) g_ls.top = (uint8_t)(g_ls.sel - g_ls.vis + 1);
    ls_fill(&g_ls);
}
int scene_list_stride(void) { return LIST_CAP; } // codes between two labels of the window
#endif

// ---------------------------------------------------------------- C5 world features (contract 42): Hidrodeslizador, Empuje, the fishing cast
// They live here, in the PC plugin, because they are asked rarely (an OK on the water, on a boulder, a rod from the bag) and the resident app has no room for them: world.c calls
// menu_open_daycare(DC_FEAT + act, arg) (a plugin load per call, tens of ms) and daycare_pc_op forwards it. `act` is a World.act value (world.h WACT_*): the OK press itself
// (WACT_WATER, WACT_BOULDER: ask, then World.act = the answer's act) or the continuation of an earlier one at the next world update (the YES / NO answer is in World.vars[0]).
// Returns 1 when it pushed a scene or started a step.
static bool flag_on(const World* w, uint16_t id) {
    return (w->flags[id >> 3] >> (id & 7)) & 1;
}

static bool feat_ask(World* w, uint16_t text, uint8_t next) {
    w->vars[0] = 0;
    if(!scene_ask_open(w->sc, text, &w->vars[0])) return false;
    w->act = next; // (the answer is read at the next world update)
    return true;
}

// ---- C6 (contract 46): the Planeador. The towns are the table mapc generates from the `@fly` directives (maps_gen.h FLY_TABLE_INIT: one row per slot, text id 0 = the slot is unused); a slot is visited
// when its story flag FLY_FLAG0 + slot is set (world_load_map sets it when the town's map is entered). The list shows the visited slots in slot order; OK flies there (the standard warp fade).
typedef struct {
    uint8_t map, i, j, face;
    uint16_t txt;
} FlyTown;
static const FlyTown FLY[FLY_N] = {FLY_TABLE_INIT};

static bool fly_visited(const World* w, uint8_t k) {
    return k < FLY_N && FLY[k].txt && flag_on(w, (uint16_t)(FLY_FLAG0 + k));
}

// The roaming wild creatures (contract 55) are placed and touched in the CASINO plugin since contract 69 (scene_roam.c: the PC plugin needed the room). One weighted pick of an encounter slot and its level (the grass / the fishing tables' rule): the slot, its level in *lv.
static const EncDef* enc_pick(const EncDef* t, uint8_t n, uint8_t* lv) {
    uint32_t* rng = party_rng();
    uint32_t sum = 0;
    for(uint8_t k = 0; k < n; k++) sum += t[k].weight;
    uint32_t r = bc_rand_range(rng, 0, sum - 1);
    for(; r >= t->weight; t++) r -= t->weight;
    *lv = (uint8_t)bc_rand_range(rng, t->lv_min, t->lv_max);
    return t;
}

uint32_t world_feat(uint8_t act, uint8_t arg) {
    World* w = g_w;
    SceneCtx* c = w->sc;
    const uint8_t f = w->hero.facing & 7;
    switch(act) {
    case WACT_WATER: // OK facing the water, with the Hidrodeslizador module
        if(!flag_on(w, FLAG_BADGE_TOXINA) || flag_on(w, FLAG_SURF) || (w->map.flags & MAPF_INTERIOR)) return 0;
        if(flag_on(w, FLAG_BIKE)) return scene_dialog_open(c, TXT_GL_C5A_SURF_BIKE, -1);
        return feat_ask(w, TXT_GL_C5A_SURF_ASK, WACT_SURF);
    case WACT_SURF: { // the answer: YES moves him onto the water, surfing (the step is a normal one: a swimmer or the corner rule can refuse it, then nothing changes)
        const uint8_t bit_s = (uint8_t)(1 << (FLAG_SURF & 7)), bit_b = (uint8_t)(1 << (FLAG_BIKE & 7));
        int fi, fj;
        world_front(w, &fi, &fj);
        if(w->vars[0] != 1 || !(cflags_at(&w->map, fi, fj) & C_WATER)) return 0;
        w->flags[FLAG_SURF >> 3] |= bit_s;
        w->flags[FLAG_BIKE >> 3] &= (uint8_t)~bit_b;
        if(world_start_step(w, f, false)) {
#ifdef SND_MAR
            sound_play(SND_MAR); // the sea's track (assets/music/index.txt `music mar`; without it the map's music goes on)
#endif
            return 1;
        }
        w->flags[FLAG_SURF >> 3] &= (uint8_t)~bit_s;
        return 0;
    }
    case WACT_BOULDER: // OK on a boulder (arg = its NPC index): Empuje is asked once per map visit
        if(!flag_on(w, MOD_EMPUJE) || !flag_on(w, 83)) return scene_dialog_open(c, TXT_GL_C5A_PUSH_NO, -1); // (83 = Medalla Raiz, builtin `badge_raiz`)
        if(w->push_on) return scene_dialog_open(c, TXT_GL_C5A_PUSH_ON, -1);
        return feat_ask(w, TXT_GL_C5A_PUSH_ASK, WACT_PUSH);
    case WACT_PUSH: // the answer
        if(w->vars[0] != 1) return 0;
        w->push_on = 1;
        return scene_dialog_open(c, TXT_GL_C5A_PUSH_USED, -1);
    case WACT_ROD0:
    case WACT_ROD1:
    case WACT_ROD2: { // the cast: a text, then the bite (a wild battle at the next update) or nothing
        uint8_t n, rate;
        const EncDef* t = map_table(&w->map, (uint8_t)(TBL_FISH_OLD + act - WACT_ROD0), &n, &rate);
        uint32_t* rng = party_rng();
        if(!n || (rate != 255 && bc_rand8(rng) >= rate)) return scene_dialog_open(c, TXT_GL_C5A_FISH_NONE, -1);
        const EncDef* e = enc_pick(t, n, &w->enc_lv);
        w->enc_sp = e->species;
        w->act = WACT_BITE;
        return scene_dialog_open(c, TXT_GL_C5A_FISH_BITE, -1);
    }
    case WACT_BITE: return world_start_wild(w, w->enc_sp, w->enc_lv, -1);
    case WACT_FLY: { // the bag's Usar on the Planeador (world_fly): the list of the visited towns, OK flies (WACT_FLY_GO at the next update)
        uint8_t n = 0;
        for(uint8_t k = 0; k < FLY_N; k++)
            if(fly_visited(w, k)) g_ls.id[n++] = FLY[k].txt;
        if(!n) return scene_dialog_open(c, TXT_GL_BAG_NOT_NOW, -1);
        w->act = WACT_FLY_GO;
        if(ls_open(c, 0, n)) return 1;
        w->act = 0;
        return 0;
    }
    case WACT_GIVE: { // C6 (contract 46.4): `give_box` with a full party (natives.c nat_give): the creature is made here and waits in the pending slot; the world's chain takes it to the box (the store scene) at its next update
        const BcData* d = party_data();
        Mon m;
        if(d && w->enc_sp && w->enc_sp < d->n_species) {
            bc_mon_create(d, &m, w->enc_sp, w->enc_lv, party_rng());
            bc_friend_set(&m, BC_FR_GIFT); // contract 65: a gift
            if(party_pending_set(&m)) {
                reg_set_caught(w->enc_sp);
                w->pb = PB_BOX;
            }
        }
        return 0;
    }
    case WACT_FLY_GO: { // the answer is World.vars[0] = the row (255 = Back)
        uint8_t r = w->vars[0];
        for(uint8_t k = 0; k < FLY_N && r != 255; k++)
            if(fly_visited(w, k) && !r--) return scene_fade_warp(c, FLY[k].map, FLY[k].i, FLY[k].j, FLY[k].face);
        return 0;
    }
    case WACT_LAND: // a boulder rolled onto a tile of a map with pits: a boulder standing on a visible pit (NPCB_HOLE) and the pit vanish; the pit's flag is set for good (the hole is floor from now on), the boulder
        for(uint8_t k = 0; k < w->map.n_npcs; k++) { // is hidden until the map is loaded again (World.npc_force)
            if(npc_behavior(&w->map.npcs[k]) != NPCB_BOULDER || !w->npc[k].visible || w->npc[k].moving) continue;
            for(uint8_t o = 0; o < w->map.n_npcs; o++) {
                const NpcDef* n = &w->map.npcs[o];
                if(npc_behavior(n) != NPCB_HOLE || !w->npc[o].visible || n->i != w->npc[k].i || n->j != w->npc[k].j) continue;
                const uint16_t id = npc_hide_flag(n);
                w->flags[id >> 3] |= (uint8_t)(1 << (id & 7));
                w->npc[o].visible = 0;
                w->npc[k].visible = 0;
                w->npc_force[k] = 1;
            }
        }
        return 0;
#ifndef NUCLEO_DEVICE
    case WACT_ROAM: // contract 55: the roamers (contract 69: on the device the casino plugin does them, world.c feat(); the host links scene_roam.c into every harness)
    case WACT_ROAM_HIT:
    case WACT_RUSTLE: return roam_feat(act, arg);
#endif
    default: (void)arg; return 0;
    }
}
