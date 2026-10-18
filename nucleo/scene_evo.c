// EVO scene (M5-B, contract section 24): evolution by level. Part of the UI plugin (ui_host.h); opened by world.c's post-battle chain through
// ui_open_evo() for a party slot whose creature leveled in the battle that just ended and whose species evolves at that level or below.
// Stack: WORLD / EVO / DIALOG (/ CHOICE). The scene draws a white screen with the creature's medium front picture (mon.bin BP_FRONT_MED) and runs
// a small state machine: every phase that shows text pushes a dialog and is resumed by update() when that dialog has closed.
//   what -> animation (~4 s: the old and the new picture as BLACK silhouettes, alternating, the swaps getting faster; Back cancels) ->
//   done ("Felicidades" + the new species) -> the moves the new species learns at exactly this level (a free slot just learns it; with 4 known
//   moves the forget dialogue of the battle, texts gl.learn.*) -> pop.
// Cancel: "stopped evolving", nothing changes (the creature tries again the next time it levels up in a battle: the chain only looks at the
// leveled mask, the eligibility is level >= evo_level).
// Evolving: species = evo_to, stats recomputed (bc_mon_recalc), keeps the HP lost (new hp = new max - (old max - old hp)), the Registro
// gets the new species (seen + caught). The nickname is not touched: without one the displayed name is simply the new species' name.
// C4-B (contract 36): the same scene also runs for an EVOLUTION STONE used from the bag (arg 0x80 | slot: scene_party.c stone_on -> SceneCtx.stone_evo / stone_to -> scene_menu.c
// update -> ui_open_evo): the target is the one scene_party.c found in evostone.bin (SceneCtx.stone_to) instead of the level fields, and Back does NOT cancel the animation.
// Memory: two pictures (<= 840 B each) + 0.2 KB of state; the dialogs are heap blocks of 0.45 KB while open.
#include <stddef.h>
#include "scene.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "monpic.h"
#include "party.h"
#include "bag.h"
#include "ui_host.h"
#include "sound.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"

#define EVO_ANIM_MS 4000    // the silhouette animation
#define SWAP_START_MS 400 // time one picture stays at the start ...
#define SWAP_SLOPE 85   // ... shortened by 85 ms per second of animation ...
#define SWAP_MIN_MS 60  // ... down to this
#define MV_CAP 14       // codes of a move name in the forget list
#define LIST_X 2
#define LIST_PAD_X 3
#define CURSOR_GAP 2

enum {
    S_INTRO,    // "what?" dialog
    S_ANIM_GO,  // its dialog closed: start the animation
    S_ANIM,     // running
    S_CANCEL,   // cancelled: "stopped evolving"
    S_APPLY,    // animation over: evolve (the new species' cry), "congratulations"
    S_LEARN,    // next move of the new species' learnset at this level
    S_FULL,     // "but X already knows 4 moves"
    S_ASK,      // "forget a move to learn Y?"
    S_ANS,      // its answer
    S_STOP,     // "stop learning Y?" (after a NO)
    S_STOP_ANS, // its answer
    S_PICK,     // the list of the 4 moves (input)
    S_FORGOT,   // "1, 2 and... poof" done: "X forgot Z"
    S_AND,      // "and... X learned Y"
    S_EXIT,
    S_FANFARE   // (after S_EXIT: the numbers above are mirrored by tools/host_world/evo_check.c) its dialog closed: the evolution-done jingle (contract 75: the cry took its place at the reveal)
};

typedef struct {
    MonPic pa, pb; // the old and the new species' medium front picture
    uint32_t t, next; // animation clock (ms) and the time of the next swap
    uint8_t slot, ph;
    uint8_t to;    // the species it becomes
    bool stone;    // C4-B (contract 36): a stone used from the bag (the target was found by scene_party.c: SceneCtx.stone_to); Back cannot cancel
    uint8_t show; // 0 old picture, 1 old silhouette, 2 new silhouette, 3 new picture
    uint8_t li;   // next entry of the new species' learnset to look at
    uint8_t mv;   // the move being learned
    uint8_t sel;  // forget list cursor
    uint8_t ans;  // YES / NO answer
    bool list;    // the forget list is on screen
    bool cancel;  // Back was pressed during the animation
    tchar mvn[4][MV_CAP];
    int16_t lw, lh; // forget list box size
} Evo;

static Evo g_ev;
#ifndef NUCLEO_DEVICE
static int g_opened;
#define EVO_COUNT() (g_opened++)
#else
#define EVO_COUNT() ((void)0)
#endif

static Mon* mon_of(const Evo* n) {
    return &party_player()->mon[n->slot];
}

// By level: what the creature evolves into now (0 = nothing: level not reached, species unknown).
static uint8_t level_target(const Mon* m) {
    const BcData* d = party_data();
    if(!d || m->species == 0 || m->species >= d->n_species) return 0;
    const SpeciesDef* s = bc_species(d, m->species);
    return s->evo_to < d->n_species ? bc_level_evo(s, m) : 0; // (contract 65: evolve_friend species need friendship >= 220 too)
}

// arg: the party slot, or (C4-B) 0x80 | slot for a stone used from the bag (menu_host.h / scene_party.c stone_on: the target is c->stone_to).
bool scene_evo_open(SceneCtx* c, uint8_t arg) {
    Party* p = party_player();
    const uint8_t slot = arg & 7;
    const bool stone = (arg & 0x80) != 0;
    const BcData* d = party_data();
    const uint8_t to = stone ? c->stone_to : (slot < p->count ? level_target(&p->mon[slot]) : 0);
    if(c->n + 3 >= MAX_SCENES || slot >= p->count || slot >= BC_MAX_PARTY || !to || !d || to >= d->n_species) return false;
    Evo* n = &g_ev;
    n->stone = stone;
    n->to = to;
    if(n->pa.blk) monpic_free(&n->pa);
    if(n->pb.blk) monpic_free(&n->pb);
    n->slot = slot;
    EVO_COUNT();
    n->ph = S_INTRO;
    n->show = 0;
    n->li = 0;
    n->sel = 0;
    n->list = false;
    n->cancel = false;
    n->t = n->next = 0;
    uint8_t from = p->mon[slot].species;
    monpic_load_kind(&n->pa, from, BP_FRONT_MED);
    monpic_load_kind(&n->pb, to, BP_FRONT_MED);
    if(!scene_push(c, &scene_evo_ops, n)) {
        monpic_free(&n->pa);
        monpic_free(&n->pb);
        return false;
    }
    return true;
}

static void set_name_var(const Evo* n) { // {v0} = the display name of the slot (the nickname, else the species name)
    tchar nm[TEXT_VAR_LEN];
    party_mon_name(n->slot, nm, TEXT_VAR_LEN);
    text_set_var(0, nm);
}

static void set_move_var(uint8_t var, uint8_t mv) {
    if(!text_set_var_id(var, reg_mv_txt(mv))) text_set_var(var, NULL);
}

static void e_enter(SceneCtx* c, void* d) {
    (void)d;
    ui_live_enter();
    scene_dirty(c, D_WORLD);
    scene_wake(c, 1);
}

static void e_leave(SceneCtx* c, void* d) {
    (void)c;
    Evo* n = d;
    ui_live_leave();
    monpic_free(&n->pa);
    monpic_free(&n->pb);
}

// the creature keeps the HP it lost. exp is never below the base of its level in the new growth rate.
static void apply_evolution(Evo* n) {
    const BcData* d = party_data();
    Mon* m = mon_of(n);
    uint8_t to = n->to;
    uint16_t old_max = m->max_hp, old_hp = m->hp;
    m->species = to;
    bc_mon_recalc(d, m);
    uint16_t lost = old_max > old_hp ? (uint16_t)(old_max - old_hp) : 0;
    m->hp = old_hp == 0 ? 0 : (m->max_hp > lost ? (uint16_t)(m->max_hp - lost) : 1);
    uint32_t base = bc_exp_for_level(bc_species(d, to)->growth, m->level);
    if(m->exp < base) m->exp = base;
    reg_set_caught(to); // seen + caught in the Registro
}

// ---- the picture
static void pic_xy(const MonPic* p, int* x, int* y) {
    *x = (SCREEN_W - p->b.w) / 2;
    const int top = tb_geom()->box_y - p->b.h;
    *y = top > 0 ? top / 2 : 0; // above the text box when it fits, else against the top edge (a 42-row picture loses its lowest rows to the box)
}

static void draw_sil(LvlBuf lvl, const MonPic* p) {
    if(!p->blk) return;
    int x0, y0;
    pic_xy(p, &x0, &y0);
    for(int y = 0; y < p->b.h; y++) {
        const uint8_t* m = p->b.mask + y * p->b.mbpr;
        for(int x = 0; x < p->b.w; x++)
            if((m[x >> 3] >> (x & 7)) & 1) lvl_putc(lvl, x0 + x, y0 + y, 3);
    }
}

static void e_draw_world(SceneCtx* c, void* d) {
    Evo* n = d;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, 0);
    if(n->list) return; // the forget list has the screen
    const MonPic* p = (n->show == 0 || n->show == 1) ? &n->pa : &n->pb;
    if(!p->blk) return;
    if(n->show == 1 || n->show == 2) {
        draw_sil(lvl, p);
    } else {
        int x, y;
        pic_xy(p, &x, &y);
        blit_masked(lvl, &p->b, x, y);
    }
}

// ---- the forget list (same widget as the name picker's list: framed box, cursor glyph, one row per entry)
static void list_load(Evo* n) {
    const Mon* m = mon_of(n);
    const tchar* all[4];
    int wmax = 0;
    for(int k = 0; k < 4; k++) {
        n->mvn[k][0] = 0;
        text_load(reg_mv_txt(m->moves[k]), n->mvn[k], MV_CAP);
        all[k] = n->mvn[k];
        int w = font_text_width(n->mvn[k]);
        if(w > wmax) wmax = w;
    }
    text_prefetch_many(all, 4);
    n->lw = (int16_t)(LIST_PAD_X + font_cell_w() + CURSOR_GAP + wmax + LIST_PAD_X + 1);
    n->lh = (int16_t)(4 * font_pitch() + 2 * TB_PAD_Y);
}

static void e_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Evo* n = d;
    if(!n->list) return;
    const int pitch = font_pitch();
    ov_fill(LIST_X, 2, n->lw, n->lh, 0);
    ov_frame(LIST_X, 2, n->lw, n->lh, 1);
    const int tx = LIST_X + LIST_PAD_X + font_cell_w() + CURSOR_GAP;
    for(int k = 0; k < 4; k++) {
        int ly = 2 + TB_PAD_Y + k * pitch;
        text_draw_line(n->mvn[k], 0, tx, ly, 99);
        if(n->sel == k) font_draw_glyph(GLYPH_CURSOR, LIST_X + LIST_PAD_X, ly, 1);
    }
}

// ---- the state machine
static int swap_ms(uint32_t t) {
    int iv = SWAP_START_MS - (int)(t * SWAP_SLOPE / 1000u);
    return iv < SWAP_MIN_MS ? SWAP_MIN_MS : iv;
}

// Pushes a dialog; false = it could not open (the caller just goes on to the next phase).
static bool dlg(SceneCtx* c, Evo* n, uint16_t id, uint8_t next) {
    n->ph = next;
    return scene_dialog_open(c, id, -1);
}

static bool ask(SceneCtx* c, Evo* n, uint16_t id, uint8_t next) {
    n->ph = next;
    n->ans = 0;
    return scene_ask_open(c, id, &n->ans);
}

// Runs phases until one has to wait (a dialog opened, the animation running, the list on screen) or the scene is over.
static void run(SceneCtx* c, Evo* n) {
    for(int guard = 0; guard < 64; guard++) {
        switch(n->ph) {
        case S_INTRO:
            set_name_var(n);
            if(dlg(c, n, TXT_GL_EVO_WHAT, S_ANIM_GO)) return;
            break;
        case S_ANIM_GO:
            if(!n->pa.blk || !n->pb.blk) { // no pictures: no animation
                n->ph = S_APPLY;
                break;
            }
            n->show = 1;
            sound_play(SND_FX_EVO_START); // (contract 28)
            n->t = 0;
            n->next = SWAP_START_MS;
            n->ph = S_ANIM;
            scene_dirty(c, D_WORLD);
            scene_wake(c, SWAP_START_MS);
            return;
        case S_ANIM:
            if(!n->stone && (n->cancel || (c->held & KEY_BACK))) { // (a stone evolution cannot be stopped)
                n->show = 0;
                n->ph = S_CANCEL;
                scene_dirty(c, D_WORLD);
                break;
            }
            if(n->t >= EVO_ANIM_MS) {
                n->ph = S_APPLY;
                break;
            }
            while(n->t >= n->next) { // the swap times pass, the interval shrinks with the clock
                n->show = n->show == 1 ? 2 : 1;
                n->next += (uint32_t)swap_ms(n->t);
                scene_dirty(c, D_WORLD);
            }
            scene_wake(c, n->next - n->t);
            return;
        case S_CANCEL:
            if(dlg(c, n, TXT_GL_EVO_STOPPED, S_EXIT)) return;
            break;
        case S_APPLY: {
            text_set_var_id(1, reg_sp_txt(n->to)); // {v1} = the new species; {v0} stays the old display name
            apply_evolution(n);
            sound_play((uint16_t)(SND_CRY + n->to)); // contract 75: the new species' cry as it appears
            n->show = 3;
            n->li = 0;
            scene_dirty(c, D_WORLD);
            if(dlg(c, n, TXT_GL_EVO_DONE, S_FANFARE)) return;
            break;
        }
        case S_FANFARE:
            sound_play(SND_FX_EVO_DONE); // (contract 28)
            n->ph = S_LEARN;
            break;
        case S_LEARN: { // the moves of the new species' learnset at exactly its current level
            const BcData* d = party_data();
            Mon* m = mon_of(n);
            const SpeciesDef* s = bc_species(d, m->species);
            n->mv = 0;
            while(n->li < s->n_learn && n->li < BC_MAX_LEARN) {
                uint8_t i = n->li++;
                if(s->learn_lv[i] == m->level && s->learn_mv[i] < d->n_moves) {
                    n->mv = s->learn_mv[i];
                    break;
                }
            }
            if(!n->mv) {
                n->ph = S_EXIT;
                break;
            }
            uint8_t r = bc_learn_move(d, m, n->mv, NULL);
            if(r == 0xFF) break; // already known: nothing to do, next entry
            set_name_var(n);
            set_move_var(1, n->mv);
            if(r == 0) { // a free slot
                if(dlg(c, n, TXT_GL_BT_LEARNED, S_LEARN)) return;
                break;
            }
            if(dlg(c, n, TXT_GL_LEARN_WANTS, S_FULL)) return; // BC_LEARN_FULL: nothing changed, the player decides
            break;
        }
        case S_FULL:
            if(dlg(c, n, TXT_GL_LEARN_FULL, S_ASK)) return;
            break;
        case S_ASK:
            set_move_var(1, n->mv);
            if(ask(c, n, TXT_GL_LEARN_FORGET_Q, S_ANS)) return;
            n->ph = S_STOP; // cannot ask: give up on the move
            break;
        case S_ANS:
            if(n->ans) {
                list_load(n);
                n->sel = 0;
                n->list = true;
                n->ph = S_PICK;
                scene_dirty(c, D_WORLD | D_COMPOSE);
                return; // input picks
            }
            n->ph = S_STOP;
            break;
        case S_STOP:
            if(ask(c, n, TXT_GL_LEARN_STOP_Q, S_STOP_ANS)) return;
            n->ans = 1;
            n->ph = S_STOP_ANS;
            break;
        case S_STOP_ANS:
            if(n->ans) { // gave up
                if(dlg(c, n, TXT_GL_LEARN_NOT, S_LEARN)) return;
            } else {
                n->ph = S_ASK; // asks again
            }
            break;
        case S_PICK:
            return; // waits for input
        case S_FORGOT:
            if(dlg(c, n, TXT_GL_LEARN_FORGOT, S_AND)) return; // {v1} = the forgotten move (set at the pick)
            break;
        case S_AND:
            set_move_var(1, n->mv);
            if(dlg(c, n, TXT_GL_LEARN_AND, S_LEARN)) return;
            break;
        default: // S_EXIT
            scene_pop(c);
            scene_wake(c, 1); // the world's chain goes on
            return;
        }
    }
    scene_pop(c); // cannot happen
}

static void e_update(SceneCtx* c, void* d, uint32_t dt) {
    Evo* n = d;
    if(n->ph == S_ANIM) n->t += dt;
    run(c, n);
}

static void e_input(SceneCtx* c, void* d, const InputEv* ev) {
    Evo* n = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_RELEASE || ev->type == EV_REPEAT) return;
        if(n->ph == S_ANIM) { // any Back event (press, short, long) cancels; it never exits the app from here
            n->cancel = true;
            scene_wake(c, 1);
        } else if(n->ph == S_PICK && ev->type == EV_SHORT) { // back to "stop learning?"
            n->list = false;
            n->ph = S_STOP;
            scene_dirty(c, D_WORLD | D_COMPOSE);
            scene_wake(c, 1);
        } else if(ev->type == EV_LONG) {
            scene_clear(c);
        }
        return;
    }
    if(n->ph != S_PICK || (ev->type != EV_PRESS && ev->type != EV_REPEAT)) return;
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        n->sel = (uint8_t)((n->sel + (ev->key == KEY_DOWN ? 1 : 3)) % 4);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        Mon* m = mon_of(n);
        uint8_t old = m->moves[n->sel];
        bc_move_set(party_data(), m, n->sel, n->mv); // the move goes into the picked slot (full PP)
        set_name_var(n);
        set_move_var(1, old); // {v1} = the forgotten move for "X forgot Y"
        n->list = false;
        n->ph = S_FORGOT;
        scene_dirty(c, D_WORLD | D_COMPOSE);
        if(!scene_dialog_open(c, TXT_GL_LEARN_POOF, -1)) scene_wake(c, 1);
    }
}

const SceneOps scene_evo_ops = {
    .enter = e_enter,
    .leave = e_leave,
    .input = e_input,
    .update = e_update,
    .draw_world = e_draw_world,
    .draw_overlay = e_draw_overlay,
    .pauses_below = true,
};

#ifndef NUCLEO_DEVICE
int scene_evo_opened(void) {
    return g_opened;
}
// Host tests (evo_check): the EVO scene is on top of the stack (or below one dialog); phase S_*, what is drawn (0 old picture, 1 old silhouette, 2 new
// silhouette, 3 new picture), the animation clock (ms), whether the forget list is on.
bool scene_evo_probe(const SceneCtx* c, int* ph, int* show, uint32_t* t, bool* list) {
    for(int i = 0; i < c->n; i++)
        if(c->st[i].ops == &scene_evo_ops) {
            const Evo* n = c->st[i].data;
            if(ph) *ph = n->ph;
            if(show) *show = n->show;
            if(t) *t = n->t;
            if(list) *list = n->list;
            return true;
        }
    return false;
}
#endif
