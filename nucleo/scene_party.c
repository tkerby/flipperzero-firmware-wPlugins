// PARTY scene (contract section 11), the menu plugin's screen. flow: list of up to 6 creatures (name, Nv5 / Lv5, HP bar and
// numbers, status tag) -> OK -> a 3-row box (Datos / Cambiar / Cancelar) -> Datos -> the summary (Cambiar: pick a second row,
// the two creatures swap, so the first one is the one a battle sends out). The same list is the target of a healing item
// (scene_party_open_use, opened by the bag screen; contract 15): page 1 the front picture (medium
// size, framed, gray planes) + name, level, HP, status, types; page 2 the four stats + EXP; page 3 the moves with PP.
// Everything is text keys (gl.party.*, gl.bt.st.*, gl.sp.*, gl.mv.*, gl.ty.*); the numbers and the status tags use the
// tiny 3x5 font (hud_gfx.c), so they are the same in every language. 3 pages instead of 2: stats + EXP + 4 moves do
// not fit 64 px (contract 11.2).
#include <stdlib.h>
#include <string.h>
#include "scene_party.h"
#include "party.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "monpic.h"
#include "hud_gfx.h"
#include "gen/langs_gen.h"
#include "bag.h"
#include "assets.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "world.h"

#define CAP 16          // codes of a name / label (a name token is <= 15 codes, TEXT_VAR_LEN)
#define PAGES_N 3
#define PAD_X 3         // picture frame: blank columns / rows between the picture and the frame line
#define PAD_Y 2
#define LIST_X 2        // cursor x
#define LIST_RIGHT 118  // right edge of the "Nv5" / "Lv5" tag (the scroll arrows live at 121..125)
#define BAR_W 44        // list HP bar
#define SUM_BAR_MAX 34
#define COL2_X 92       // summary page 2: the EXP column
#define STAT_VAL_R 80   // summary page 2: right edge of a stat value

#define HEAL_STEP_MS 35 // the drained HP bar: one step per tick (a step is restored / 20 HP at least 1: <= 0.7 s)
#define NO_ANIM 0xFF
#define N_OPT 3

enum { M_LIST = 0, M_OPT, M_SUM, M_SWAP, M_FORGET, M_SOFT }; // M_FORGET (C2-S1): the 4 moves of the creature that learns from a disc; M_SOFT (contract 81): the list picks the creature that receives Hearty Broth's HP
enum { W_NONE = 0, W_STAY, W_CLOSE, W_TEACH }; // after the dialog above closes: back to the list / close the screen (item used) / go on with the disc's teaching chain (tph)
enum { T_FULL = 1, T_ASK, T_ANS, T_STOP, T_STOP_ANS, T_FORGOT, T_AND, T_CANDY, T_CEVO, T_ETHER }; // T_ETHER (contract 73): the move list is choosing the move an Ether restores, not one to forget; the teaching chain's phases (teach_run); T_CANDY / T_CEVO: after a Growth Seed (contract 64)

typedef struct {
    uint8_t mode, sel, top, opt, page, cur, n_ty;
    uint8_t use;                        // item being used on a creature (0 = the plain party screen)
    uint8_t cevo;                       // a Growth Seed was used on row `sel` (contract 64): when the screen closes the creature may evolve
    uint8_t pick;                       // C3-A: 0 plain, 1 picks a creature for a script, 2 the in-game trade (contract 32)
    uint8_t* out;                       // pick: where the result goes (a script variable of the world)
    uint8_t tph, tmv, ans, fsel;        // disc teaching (C2-S1): chain phase, the move being taught, the YES/NO answer, the forget list's cursor
    uint8_t from;                       // Cambiar / Hearty Broth: the first row
    uint8_t nfm, otop;                  // contract 81: field moves of the creature in the option box (fm[]: Dig / Teleport / Hearty Broth move ids, listed first), first visible row
    uint8_t fm[3];
    uint8_t wait;                       // W_*: a dialog is open above
    uint8_t an_k;                       // row whose HP bar is draining (NO_ANIM = none)
    uint16_t an_hp, an_to, an_inc, an_acc, an_gain; // displayed HP, final HP, HP per step, ms into the step, HP restored
    int16_t rh, vis;                    // list: row height, rows that fit the screen
    int16_t ox, oy, ow, oh;             // option box
    int16_t fx, fy, fw, fh;             // summary page 1: the picture frame (fw = 0: no picture)
    MonPic pic;
    tchar name[BC_MAX_PARTY][CAP];      // display names of the party (nickname, else species name)
    tchar l_stats[CAP], l_switch[CAP], l_cancel[CAP], l_exp[CAP], l_next[CAP];
    tchar st[4][CAP];                   // atk def spd spc labels
    tchar ty[2][CAP];                   // types of the summary's creature
    tchar mv[4][CAP];                   // moves of the summary's creature ("" = empty slot)
} PS;

static PS* g;

bool scene_party_active(void) {
    return g != NULL;
}

// ---------------------------------------------------------------------------------------------------- texts
static void ld(tchar* d, uint16_t id) {
    if(id == 0xFFFF || text_load(id, d, CAP) < 0) d[0] = 0;
}

static const Mon* mon_at(int k) {
    return &party_player()->mon[k];
}

static int count(void) {
    return party_player()->count;
}

// The strings the list and the option box draw (SD-backed fonts: the glyphs must be resident before drawing).
static void prefetch_list(PS* s) {
    const tchar* all[BC_MAX_PARTY + 3];
    int n = 0;
    for(int k = 0; k < count(); k++) all[n++] = s->name[k];
    all[n++] = s->l_stats;
    all[n++] = s->l_switch;
    all[n++] = s->l_cancel;
    text_prefetch_many(all, n);
}

static const tchar DIGITS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', 0}; // the stat values / empty move slots

static void prefetch_sum(PS* s) {
    const tchar* all[17];
    int n = 0;
    all[n++] = DIGITS;
    all[n++] = s->name[s->cur];
    for(int i = 0; i < 4; i++) all[n++] = s->st[i];
    all[n++] = s->l_exp;
    all[n++] = s->l_next;
    for(int i = 0; i < 2; i++) all[n++] = s->ty[i];
    for(int i = 0; i < 4; i++) all[n++] = s->mv[i];
    text_prefetch_many(all, n);
}

static void load_static(PS* s) {
    for(int k = 0; k < count(); k++) party_mon_name((uint8_t)k, s->name[k], CAP); // the nickname, else the species name
    ld(s->l_stats, TXT_GL_PARTY_STATS);
    ld(s->l_switch, TXT_GL_BT_SWITCH);
    ld(s->l_cancel, TXT_GL_PARTY_CANCEL);
    ld(s->l_exp, TXT_GL_PARTY_EXP);
    ld(s->l_next, TXT_GL_PARTY_NEXT);
    ld(s->st[0], TXT_GL_BT_ST_ATTACK);
    ld(s->st[1], TXT_GL_BT_ST_DEFENSE);
    ld(s->st[2], TXT_GL_BT_ST_SPEED);
    ld(s->st[3], TXT_GL_BT_ST_SPECIAL);
    // geometry that depends on the font
    int p = font_pitch();
    s->rh = (int16_t)(p + 8);
    s->vis = (int16_t)((SCREEN_H - 2) / s->rh);
    if(s->vis < 1) s->vis = 1;
    int w1 = font_text_width(s->l_stats), w2 = font_text_width(s->l_cancel), w3 = font_text_width(s->l_switch);
    if(w2 > w1) w1 = w2;
    if(w3 > w1) w1 = w3;
    s->ow = (int16_t)(font_cell_w() + 9 + w1 + 4);
    s->oh = (int16_t)(N_OPT * p + 4);
    s->ox = (int16_t)(SCREEN_W - s->ow);
    s->oy = (int16_t)(SCREEN_H - s->oh);
    prefetch_list(s);
}

// Types and moves of summary creature `cur`, and its picture.
static void load_summary(PS* s) {
    const Mon* m = mon_at(s->cur);
    const BcData* d = party_data();
    s->n_ty = 0;
    s->ty[0][0] = s->ty[1][0] = 0;
    if(d && m->species && m->species < d->n_species) {
        const SpeciesDef* sd = bc_species(d, m->species);
        ld(s->ty[0], reg_ty_txt(sd->type1));
        s->n_ty = 1;
        if(sd->type2 != sd->type1) {
            ld(s->ty[1], reg_ty_txt(sd->type2));
            s->n_ty = 2;
        }
    }
    for(int i = 0; i < 4; i++) {
        uint8_t mv = m->moves[i];
        ld(s->mv[i], mv && reg_mv_txt(mv));
    }
    prefetch_sum(s);
    monpic_free(&s->pic);
    s->fw = 0;
    if(monpic_load_kind(&s->pic, m->species, BP_FRONT_MED)) {
        if(bc_shiny(m)) monpic_shiny(&s->pic); // contract 65: a variocolor creature shows its tones
        s->fw = (int16_t)(s->pic.b.w + 2 * PAD_X);
        s->fh = (int16_t)(s->pic.b.h + 2 * PAD_Y);
        s->fx = 2;
        s->fy = (int16_t)((SCREEN_H - s->fh) / 2);
        if(s->fy < 0) s->fy = 0;
    }
}

// ---------------------------------------------------------------------------------------------------- scene
static void p_enter(SceneCtx* c, void* d) {
    PS* s = d;
    s->an_k = NO_ANIM;
    load_static(s);
    scene_dirty(c, D_WORLD);
}

static void p_leave(SceneCtx* c, void* d) {
    (void)c;
    PS* s = d;
    monpic_free(&s->pic);
    if(g == s) g = NULL;
    free(s);
}

// The list cursor follows the creature last looked at in the summary (Up / Down browse the party there).
static void sel_follow(PS* s, uint8_t k) {
    s->sel = k;
    if(s->sel < s->top) s->top = s->sel;
    if(s->sel >= s->top + s->vis) s->top = (uint8_t)(s->sel - s->vis + 1);
}

static void to_list(SceneCtx* c, PS* s) {
    if(s->mode == M_SUM) sel_follow(s, s->cur);
    monpic_free(&s->pic);
    s->mode = M_LIST;
    prefetch_list(s);
    scene_dirty(c, D_WORLD);
}

static void to_summary(SceneCtx* c, PS* s, int cur) {
    s->mode = M_SUM;
    s->cur = (uint8_t)cur;
    s->page = 0;
    load_summary(s);
    scene_dirty(c, D_WORLD);
}

static void close_screen(SceneCtx* c) {
    if(g && g->cevo) { // a Growth Seed was used: a creature whose new level calls for it evolves now (the stone's hand-over, scene_menu.c: the item byte 0 = nothing to give back)
        const Mon* cm = &party_player()->mon[g->sel];
        const SpeciesDef* csp = bc_species(party_data(), cm->species); // (bc_evolution is not in the app's API table: the same test inline)
        const uint8_t to = bc_level_evo(csp, cm); // (contract 65: a friendship evolution waits for friendship >= 220)
        if(to) {
            c->stone_to = to;
            c->stone_evo = (uint16_t)(0x80 | g->sel);
            scene_pop(c); // this list (its state is freed: `g` is not used again)
            scene_pop(c); // the bag
            scene_wake(c, 1);
            return;
        }
    }
    scene_pop(c);
    scene_wake(c, 1);
}

// Opens a dialog above the screen; `after` says what to do once it has closed (resume()).
static void say(SceneCtx* c, PS* s, uint16_t id, uint8_t after) {
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) {
        s->wait = W_NONE;
        if(after == W_CLOSE) close_screen(c);
    }
}

// The dialog above has closed (the screen is on top again).
static void teach_run(SceneCtx* c, PS* s);
static void resume(SceneCtx* c, PS* s) {
    uint8_t a = s->wait;
    s->wait = W_NONE;
    if(a == W_CLOSE) close_screen(c);
    else if(a == W_TEACH) teach_run(c, s);
    else scene_dirty(c, D_WORLD);
}

// A status-cure item on row k (M4-A, contract 16): only a living creature that has that status; else "no effect", item kept.
static void cure_on(SceneCtx* c, PS* s, int k, uint8_t sk) {
    Mon* m = &party_player()->mon[k];
    if(!m->hp || !bc_has_status(m, sk)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    if(!bag_take(s->use)) {
        close_screen(c);
        return;
    }
    m->status = 0;
    text_set_var(0, s->name[k]);
    say(c, s,
        sk == SK_SLEEP       ? TXT_GL_BT_WAKE
        : sk == SK_FREEZE    ? TXT_GL_BT_THAW
        : sk == SK_POISON    ? TXT_GL_BT_CURED_PSN
        : sk == SK_PARALYSIS ? TXT_GL_BT_CURED_PAR
                             : TXT_GL_BT_CURED_BRN,
        W_CLOSE);
    scene_dirty(c, D_COMPOSE);
}

// ---- technique discs (C2-S1, contract 29): disc flow. Compatible? -> already known / free slot (learned at once, the disc is used up) / 4 moves: the
// forget dialogue of the battle and the evolution (texts gl.learn.*). Giving up keeps the disc. Stack: WORLD / MENU / BAG / PARTY / DIALOG / CHOICE = 6.
static void tm_vars(const PS* s, uint8_t mv) { // {v0} = the creature on the list's cursor, {v1} = a move
    text_set_var(0, s->name[s->sel]);
    text_set_var_id(1, reg_mv_txt(mv));
}

static bool disc_ok(uint8_t item, uint8_t species) { // bit `aux` of the item record (byte 2) in the species' tm mask (tm.bin, contract 64: a u64 per species after the 12-byte header)
    uint8_t bit = 0xFF, m[8];
    if(!read_at("items.bin", 12u + item * 8u + 2, &bit, 1) || bit >= 64 || !read_at("tm.bin", 12u + species * 8u, m, 8)) return false;
    return (m[bit >> 3] >> (bit & 7)) & 1;
}

static void tsay(SceneCtx* c, PS* s, uint16_t id, uint8_t next) { // a dialog, then teach_run goes on at `next`
    s->tph = next;
    say(c, s, id, W_TEACH);
}

static void task(SceneCtx* c, PS* s, uint16_t id, uint8_t next) { // a YES/NO question, the answer lands in s->ans (1 = YES), then teach_run goes on at `next`
    s->tph = next;
    s->ans = 0;
    s->wait = W_TEACH;
    if(!scene_ask_open(c, id, &s->ans)) s->wait = W_NONE;
}

static void teach_run(SceneCtx* c, PS* s) {
    Mon* m = &party_player()->mon[s->sel];
    switch(s->tph) {
    case T_FULL: tsay(c, s, TXT_GL_LEARN_FULL, T_ASK); break;
    case T_ASK:
        tm_vars(s, s->tmv);
        task(c, s, TXT_GL_LEARN_FORGET_Q, T_ANS);
        break;
    case T_ANS:
        if(s->ans) { // YES: the four moves
            const tchar* all[4];
            for(int i = 0; i < 4; i++) {
                ld(s->mv[i], reg_mv_txt(m->moves[i]));
                all[i] = s->mv[i];
            }
            text_prefetch_many(all, 4);
            s->fsel = 0;
            s->mode = M_FORGET;
            scene_dirty(c, D_COMPOSE);
            break;
        }
        // NO -> "give up learning?"
        __attribute__((fallthrough));
    case T_STOP:
        tm_vars(s, s->tmv);
        task(c, s, TXT_GL_LEARN_STOP_Q, T_STOP_ANS);
        break;
    case T_STOP_ANS:
        if(s->ans) { // gave up: the disc is kept
            tm_vars(s, s->tmv);
            say(c, s, TXT_GL_LEARN_NOT, W_STAY);
        } else {
            s->tph = T_ASK;
            teach_run(c, s);
        }
        break;
    case T_CANDY: // the "grew to level N" line of a Growth Seed is read: the move of the new level (s->tmv; s->ans = BC_LEARN_FULL when the four slots are taken), else the evolution check
        if(s->tmv) {
            tm_vars(s, s->tmv);
            if(s->ans == BC_LEARN_FULL) tsay(c, s, TXT_GL_LEARN_WANTS, T_FULL);
            else tsay(c, s, TXT_GL_BT_LEARNED, T_CEVO);
            break;
        }
        __attribute__((fallthrough));
    case T_CEVO: close_screen(c); break; // (close_screen starts the evolution when the candy's level calls for it)
    case T_FORGOT: tsay(c, s, TXT_GL_LEARN_FORGOT, T_AND); break; // {v1} = the forgotten move (set at the pick)
    default:       // T_AND
        tm_vars(s, s->tmv);
        say(c, s, TXT_GL_LEARN_AND, W_CLOSE);
        break;
    }
}

static void forget_pick(SceneCtx* c, PS* s) { // OK on slot fsel of the forget list
    Mon* m = &party_player()->mon[s->sel];
    uint8_t old = m->moves[s->fsel];
    bc_move_set(party_data(), m, s->fsel, s->tmv); // the new move, full PP
    if(party_data()->items[s->use].kind == IK_DISC) bag_take(s->use); // the disc is used up (a Growth Seed was taken when it was used)
    tm_vars(s, old);
    s->mode = M_LIST;
    tsay(c, s, TXT_GL_LEARN_POOF, T_FORGOT);
}

// Ether / Max Ether (contract 73): `arg` PP (255 = all) back to ONE move of the creature, which the player picks in the moves' list (the forget list's box); "no effect" and the item kept when that move is not short of PP.
// PP Extra (contract 77, kind IK_PPUP): the same picker; the chosen move gains one PP Plus (top two bits of its PP byte, at most 3) and base / 5 PP, and is refused when it has 3 already.
static int pp_base(const Mon* m, int i) {
    return m->moves[i] ? bc_move(party_data(), m->moves[i])->pp : 0;
}
static bool pp_item_ok(const PS* s, const Mon* m, int i) { // may the item in use act on move i?
    const int b = pp_base(m, i);
    if(!b) return false;
    if(party_data()->items[s->use].kind == IK_PPUP) return m->pp[i] < 3 * BC_PP_UP && b >= 5;
    return (m->pp[i] & BC_PP_LEFT) < bc_pp_max((uint8_t)b, m->pp[i]);
}
static void ether_on(SceneCtx* c, PS* s, int k) {
    const Mon* m = &party_player()->mon[k];
    bool any = false;
    const tchar* all[4];
    for(int i = 0; i < 4; i++) {
        any |= pp_item_ok(s, m, i);
        ld(s->mv[i], reg_mv_txt(m->moves[i]));
        all[i] = s->mv[i];
    }
    if(!any) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    text_prefetch_many(all, 4);
    s->fsel = 0;
    s->tph = T_ETHER;
    s->mode = M_FORGET;
    scene_dirty(c, D_COMPOSE);
}

static void ether_pick(SceneCtx* c, PS* s) { // OK on slot fsel of the list while T_ETHER
    const BcData* d = party_data();
    Mon* m = &party_player()->mon[s->sel];
    const int i = s->fsel, b = pp_base(m, i);
    const bool up = d->items[s->use].kind == IK_PPUP;
    s->mode = M_LIST;
    if(!pp_item_ok(s, m, i) || !bag_take(s->use)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    int to = (m->pp[i] & BC_PP_LEFT) + (up ? b / 5 : d->items[s->use].arg);
    if(up) m->pp[i] += BC_PP_UP;
    const int mx = bc_pp_max((uint8_t)b, m->pp[i]);
    m->pp[i] = (uint8_t)((m->pp[i] & ~BC_PP_LEFT) | (to > mx ? mx : to));
    text_set_var(0, s->name[s->sel]);
    say(c, s, up ? TXT_GL_F4_PPUP : TXT_GL_C64_PP, W_CLOSE);
}

static void disc_on(SceneCtx* c, PS* s, int k) {
    const BcData* d = party_data();
    Mon* m = &party_player()->mon[k];
    s->tmv = d->items[s->use].arg;
    tm_vars(s, s->tmv);
    if(!disc_ok(s->use, m->species)) {
        say(c, s, TXT_GL_DISC_CANT, W_STAY);
        return;
    }
    uint8_t r = bc_learn_move(d, m, s->tmv, NULL);
    if(r == 0xFF) {
        say(c, s, TXT_GL_DISC_KNOWS, W_STAY);
    } else if(r == 0) { // a free slot: learned (full PP), the disc is used up
        bag_take(s->use);
        say(c, s, TXT_GL_BT_LEARNED, W_CLOSE);
    } else {          // BC_LEARN_FULL
        tsay(c, s, TXT_GL_LEARN_WANTS, T_FULL);
    }
}

// ---- C4-B (contract 36). Evolution stone. A species that lists the stone in evostone.bin evolves (the stone is used up, the evolution scene cannot be
// cancelled); any other creature: "no effect", the stone is kept and the list stays. The scene itself lives in the UI plugin, and one plugin is loaded at a time: this
// screen and the bag pop themselves and leave the request in SceneCtx.stone_evo; scene_menu.c's update opens it (a dialog-free hand-over: nothing here is touched afterwards).
static void stone_on(SceneCtx* c, PS* s, int k) {
    const BcData* d = party_data();
    const uint8_t to = bc_stone_evo(d, party_player()->mon[k].species, d->items[s->use].arg);
    if(!to || !bag_take(s->use)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    c->stone_to = to;
    c->stone_evo = (uint16_t)(s->use << 8 | 0x80 | k);
    scene_pop(c); // this list (its state is freed in leave: `s` is not used again)
    scene_pop(c); // the bag
    scene_wake(c, 1);
}

// Vitamin (vitamin branch): +2560 stat EXP to one stat, refused at 25,600 or more (and the stat EXP is capped at 65,535 like battle_run.c); the stats are
// recomputed, HP Up also raises the current HP by the maximum's gain.
static const uint16_t VIT_TXT[5] = {TXT_GL_C4B_ST_HP, TXT_GL_BT_ST_ATTACK, TXT_GL_BT_ST_DEFENSE, TXT_GL_BT_ST_SPEED, TXT_GL_BT_ST_SPECIAL};
static void vit_on(SceneCtx* c, PS* s, int k) {
    const BcData* d = party_data();
    Mon* m = &party_player()->mon[k];
    const uint8_t st = d->items[s->use].arg;
    if(st > 4 || m->statexp[st] >= 25600 || !bag_take(s->use)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    uint32_t v = (uint32_t)m->statexp[st] + 2560;
    m->statexp[st] = (uint16_t)(v > 0xFFFF ? 0xFFFF : v);
    const uint16_t old = m->max_hp;
    bc_mon_recalc(d, m);
    if(st == 0 && m->hp && m->max_hp > old) m->hp = (uint16_t)(m->hp + m->max_hp - old);
    text_set_var(0, s->name[k]);
    text_set_var_id(1, VIT_TXT[st]);
    say(c, s, TXT_GL_BT_STATUP, W_CLOSE);
    scene_dirty(c, D_COMPOSE);
}

// A healing item on row k.
// Growth Seed (contract 64): one level, the stats and the HP gain, the move of the new level (a free slot learns it, four moves ask like a disc), then the evolution if the
// new level calls for it (close_screen). The candy is taken here.
static void candy_on(SceneCtx* c, PS* s, int k) {
    const BcData* d = party_data();
    Mon* m = &party_player()->mon[k];
    if(m->level >= 100 || !bag_take(s->use)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    const uint16_t old = m->max_hp;
    m->level++;
    m->exp = bc_exp_for_level(bc_species(d, m->species)->growth, m->level);
    bc_mon_recalc(d, m);
    if(m->hp) m->hp = (uint16_t)(m->hp + m->max_hp - old);
    bc_friend_add(m, bc_friend_level_gain(m)); // contract 65: a level is a level
    s->cevo = 1;
    s->tmv = 0;
    for(uint8_t i = 0; i < bc_species(d, m->species)->n_learn; i++) {
        const SpeciesDef* sp = bc_species(d, m->species);
        if(sp->learn_lv[i] != m->level) continue;
        const uint8_t mv = sp->learn_mv[i], r = bc_learn_move(d, m, mv, NULL);
        if(r == 0xFF) continue; // already known
        s->tmv = mv;
        s->ans = r; // 0 = learned in a free slot, BC_LEARN_FULL = it asks which move to forget
        break;
    }
    text_set_var(0, s->name[k]);
    text_set_var_num(2, m->level);
    tsay(c, s, TXT_GL_BT_LEVELUP, T_CANDY);
    scene_dirty(c, D_COMPOSE);
}

// Vigor Drop / Vigor Flask (contract 64): `arg` PP back to every move of the creature (255 = all); "no effect" and the item kept when no move is short of PP.
static void elixir_on(SceneCtx* c, PS* s, int k) {
    const BcData* d = party_data();
    Mon* m = &party_player()->mon[k];
    bool any = false;
    for(int i = 0; i < 4; i++) {
        const int mx = m->moves[i] ? bc_pp_max(bc_move(d, m->moves[i])->pp, m->pp[i]) : 0, cur = m->pp[i] & BC_PP_LEFT, to = cur + d->items[s->use].arg;
        if(cur >= mx) continue;
        m->pp[i] = (uint8_t)((m->pp[i] & ~BC_PP_LEFT) | (to > mx ? mx : to));
        any = true;
    }
    if(!any || !bag_take(s->use)) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    text_set_var(0, s->name[k]);
    say(c, s, TXT_GL_C64_PP, W_CLOSE);
    scene_dirty(c, D_COMPOSE);
}

// A healing item on row k. Contract 64: Life Spark / Life Blaze want a FAINTED creature, Vital Draught also cures the status.
static void use_on(SceneCtx* c, PS* s, int k) {
    Mon* m = &party_player()->mon[k];
    const BcData* bd = party_data();
    const uint8_t kind = bd && s->use < bd->n_items ? bd->items[s->use].kind : 0, arg = kind ? bd->items[s->use].arg : 0;
    switch(kind) {
    case IK_CURE: cure_on(c, s, k, arg); return;
    case IK_DISC: disc_on(c, s, k); return;
    case IK_STONE: stone_on(c, s, k); return;
    case IK_VITAMIN: vit_on(c, s, k); return;
    case IK_CANDY: candy_on(c, s, k); return;
    case IK_ELIXIR: elixir_on(c, s, k); return;
    case IK_ETHER:
    case IK_PPUP: ether_on(c, s, k); return;
    default: break;
    }
    const bool rv = kind == IK_REVIVE, rs = kind == IK_RESTORE;
    int amt = kind == IK_HEAL ? arg : rv ? (arg == 255 ? 255 : (m->max_hp / 2 ? m->max_hp / 2 : 1)) : rs ? 255 : 0;
    if(!amt || (rv ? m->hp != 0 : (m->hp == 0 || (m->hp >= m->max_hp && !(rs && m->status))))) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    if(!bag_take(s->use)) { // the bag changed under us: nothing to use
        close_screen(c);
        return;
    }
    if(rs) m->status = 0;
    int to = amt >= 255 || m->hp + amt > m->max_hp ? m->max_hp : m->hp + amt;
    s->an_k = (uint8_t)k;
    s->an_hp = m->hp;
    s->an_to = (uint16_t)to;
    s->an_inc = (uint16_t)((to - m->hp) / 20 > 1 ? (to - m->hp) / 20 : 1);
    s->an_acc = 0;
    s->an_gain = (uint16_t)(to - m->hp);
    m->hp = (uint16_t)to; // the creature is healed at once; the row shows the HP draining up to it
    scene_wake(c, HEAL_STEP_MS);
    scene_dirty(c, D_COMPOSE);
}

// ---- C3-A (contract 32): the party list as a picker (native 23) and the in-game trade
// The traded-for creature: same level, random DVs and the moves of that level (bc_mon_create), full HP, a fixed nickname (World.tr[2..3] = its text id; the text must be
// ASCII, so the Unicode of PartyState and the code cache of every language are the same string), registered as caught in the Registro. It is marked MX_TRADED (contract 74; the byte exists since contract 65), so it is not the player's own: the name changer refuses it. The save format does not change (contract 32.4).
static void trade_do(World* w, int k) {
    PartyState* ps = party_state();
    const uint8_t give = w->tr[1];
    bc_mon_create(party_data(), &ps->party.mon[k], give, ps->party.mon[k].level, party_rng());
    ps->party.mon[k].xf |= MX_TRADED; // contract 74: an NPC trade is not the player's own either
    reg_set_caught(give);
    tchar t[CAP];
    int n = text_load((uint16_t)(w->tr[2] | w->tr[3] << 8), t, CAP);
    for(int i = 0; i < PARTY_NICK_CAP; i++) {
        tchar ch = i < n && i < PARTY_NICK_CAP - 1 && t[i] >= 0x20 && t[i] < 0x7F ? t[i] : 0;
        ps->nick_u[k][i] = party_nick_cache()[k][i] = ch;
    }
}

static void pick_on(SceneCtx* c, PS* s, int k) {
    World* w = c->world;
    if(s->pick == 1) *s->out = (uint8_t)k;
    else if(party_player()->mon[k].species != w->tr[0] || !w->tr[1] || w->tr[1] >= party_data()->n_species) *s->out = 0; // not the wanted species (or a bad given one): the script answers
    else {
        trade_do(w, k);
        *s->out = 1;
    }
    close_screen(c);
}

// ---------------------------------------------------------------------------------------------------- field moves (contract 81)
// party menu lists the field moves a creature knows above Stats / Switch / Cancel. The three of this contract: Dig (the move `burrow`: in a cave the Exit Wisp's effect), Teleport (`blink`: outside, back to the last
// Centro) and Hearty Broth (`hearty`: gives 1/5 of its max HP to another creature). The bracelet's modules (Cortador ...) stay story flags and are not here.
// (the option box's field-move labels are s->mv[0 .. 2], the summary's move labels: the two are never on screen together, the box reloads them each time it opens)
static const uint8_t FIELD_MV[3] = {MV_BURROW, MV_BLINK, MV_HEARTY};
static int opt_rows(const PS* s) {
    return s->nfm + N_OPT;
}
// Opens the option box on creature `sel`: the field moves it knows (alive only) in the order of its move slots, then Stats / Switch / Cancel; the box grows upwards and scrolls when the rows do not fit.
static void opt_open(PS* s) {
    const Mon* m = mon_at(s->sel);
    s->nfm = 0;
    for(int i = 0; i < 4 && m->hp; i++)
        for(int f = 0; f < 3; f++)
            if(m->moves[i] == FIELD_MV[f]) {
                s->fm[s->nfm] = FIELD_MV[f];
                ld(s->mv[s->nfm++], reg_mv_txt(FIELD_MV[f]));
            }
    const tchar* all[3];
    for(int i = 0; i < s->nfm; i++) all[i] = s->mv[i];
    text_prefetch_many(all, s->nfm);
    const int p = font_pitch();
    int rows = opt_rows(s);
    int w = font_text_width(s->l_stats), x;
    if(font_text_width(s->l_switch) > w) w = font_text_width(s->l_switch);
    if(font_text_width(s->l_cancel) > w) w = font_text_width(s->l_cancel);
    for(int i = 0; i < s->nfm; i++)
        if((x = font_text_width(s->mv[i])) > w) w = x;
    s->ow = (int16_t)(font_cell_w() + 9 + w + 4);
    if(rows > (SCREEN_H - 4) / p) rows = (SCREEN_H - 4) / p;
    s->oh = (int16_t)(rows * p + 4);
    s->ox = (int16_t)(SCREEN_W - s->ow);
    s->oy = (int16_t)(SCREEN_H - s->oh);
    s->mode = M_OPT;
    s->opt = s->otop = 0;
}

void scene_escape(SceneCtx* c) {
    while(c->n > 1) scene_pop(c); // the screens (their state is freed here) and the pause menu
    world_go_respawn(c->world);   // the last Centro, with the fade
}

// Dig / Teleport: the Exit Wisp's effect (scene_bag.c do_use): everything above the world closes and the fade takes the hero to the last Centro. Dig needs a cave map, Teleport an outside one (not an interior, a cave or a dark
// map); neither works inside the Reserva's session (its steps and crystals would stay). `s` is freed by the pops: nothing of it is touched afterwards.
static void field_escape(SceneCtx* c, PS* s, bool dig) {
    const uint8_t mf = c->world->map.flags;
    if(party_state()->rv_steps || (dig ? !(mf & MAPF_CAVE) : (mf & MAPF_NOBIKE))) {
        s->mode = M_LIST;
        say(c, s, TXT_GL_BAG_NO_HERE, W_STAY);
        return;
    }
    scene_escape(c);
}

// Hearty Broth, first step: the user needs more than 1/5 of its max HP ("Not healthy enough!" otherwise); the list then picks the receiver.
static void soft_begin(SceneCtx* c, PS* s) {
    const Mon* m = mon_at(s->sel);
    if(m->hp <= m->max_hp / 5) {
        s->mode = M_LIST;
        say(c, s, TXT_GL_MISC_SOFT_LOW, W_STAY);
        return;
    }
    s->mode = M_SOFT;
    s->from = s->sel;
    scene_dirty(c, D_COMPOSE);
}

// Hearty Broth, second step: the receiver must be another creature that is alive and hurt; the user loses 1/5 of ITS max HP, the receiver gains that much, never beyond its own maximum.
static void soft_give(SceneCtx* c, PS* s, int k) {
    Mon* u = &party_player()->mon[s->from];
    Mon* r = &party_player()->mon[k];
    s->mode = M_LIST;
    if(k == s->from || !r->hp || r->hp >= r->max_hp) {
        say(c, s, TXT_GL_BT_NO_EFFECT, W_STAY);
        return;
    }
    const uint16_t amt = u->max_hp / 5, room = (uint16_t)(r->max_hp - r->hp), gain = amt < room ? amt : room;
    u->hp = (uint16_t)(u->hp - amt);
    r->hp = (uint16_t)(r->hp + gain);
    text_set_var(0, s->name[k]);
    text_set_var_num(2, gain);
    sel_follow(s, (uint8_t)k);
    say(c, s, TXT_GL_BT_HEALED, W_STAY);
}

// OK on row `r` of the option box.
static void opt_pick(SceneCtx* c, PS* s, int r) {
    if(r < s->nfm) {
        if(s->fm[r] == MV_HEARTY) soft_begin(c, s);
        else field_escape(c, s, s->fm[r] == MV_BURROW);
    } else if(r == s->nfm) {
        to_summary(c, s, s->sel);
    } else if(r == s->nfm + 1 && count() > 1) { // Cambiar: pick the row to swap with (Back / the same row cancels)
        s->mode = M_SWAP;
        s->from = s->sel;
        scene_dirty(c, D_COMPOSE);
    } else {
        s->mode = M_LIST;
        scene_dirty(c, D_COMPOSE);
    }
}



static void p_update(SceneCtx* c, void* d, uint32_t dt) {
    PS* s = d;
    if(s->wait) {
        resume(c, s);
        return;
    }
    if(s->an_k == NO_ANIM) return;
    s->an_acc = (uint16_t)(s->an_acc + (dt < 200 ? dt : 200));
    if(s->an_acc >= HEAL_STEP_MS) {
        s->an_acc = (uint16_t)(s->an_acc % HEAL_STEP_MS);
        s->an_hp = (uint16_t)(s->an_hp + s->an_inc > s->an_to ? s->an_to : s->an_hp + s->an_inc);
        scene_dirty(c, D_COMPOSE);
    }
    if(s->an_hp < s->an_to) {
        scene_wake(c, HEAL_STEP_MS);
        return;
    }
    text_set_var(0, s->name[s->an_k]);
    text_set_var_num(2, s->an_gain);
    s->an_k = NO_ANIM;
    const uint8_t kind = party_data()->items[s->use].kind;
    say(c, s, kind == IK_REVIVE ? TXT_GL_C64_REVIVED : kind == IK_RESTORE ? TXT_GL_C64_REGAIN : TXT_GL_BT_HEALED, W_CLOSE);
}

static void swap_bytes(void* a, void* b, size_t n) {
    uint8_t *x = a, *y = b;
    while(n--) {
        uint8_t t = *x;
        *x++ = *y;
        *y++ = t;
    }
}

static void swap_rows(PS* s, int a, int b) {
    PartyState* ps = party_state(); // the creatures and their nicknames (Unicode + the code cache)
    swap_bytes(&ps->party.mon[a], &ps->party.mon[b], sizeof(Mon));
    swap_bytes(ps->nick_u[a], ps->nick_u[b], sizeof(ps->nick_u[0]));
    swap_bytes(party_nick_cache()[a], party_nick_cache()[b], sizeof(party_nick_cache()[0]));
    tchar n[CAP];
    memcpy(n, s->name[a], sizeof(n));
    memcpy(s->name[a], s->name[b], sizeof(n));
    memcpy(s->name[b], n, sizeof(n));
}

static void p_input(SceneCtx* c, void* d, const InputEv* ev) {
    PS* s = d;
    const int n = count();
    if(s->wait) resume(c, s);       // (the key arrived before the update that notices the closed dialog)
    if(!c->n || scene_top_ops(c) != &scene_party_ops) return;
    if(s->an_k != NO_ANIM) return; // the bar is draining
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        else if(ev->type == EV_SHORT) {
            if(s->mode == M_LIST) close_screen(c);
            else if(s->mode == M_FORGET) { // Back from the forget list: "give up learning?" (an Ether: back to the creatures)
                s->mode = M_LIST;
                if(s->tph != T_ETHER) {
                    s->tph = T_STOP;
                    teach_run(c, s);
                }
                scene_dirty(c, D_COMPOSE);
            } else {
                if(s->mode == M_SWAP || s->mode == M_SOFT) sel_follow(s, s->from);
                to_list(c, s);
            }
        }
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    if(s->mode == M_FORGET) {
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            s->fsel = (uint8_t)((s->fsel + (ev->key == KEY_DOWN ? 1 : 3)) % 4);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->tph == T_ETHER) ether_pick(c, s);
            else forget_pick(c, s);
            scene_dirty(c, D_COMPOSE);
        }
    } else if(s->mode == M_LIST || s->mode == M_SWAP || s->mode == M_SOFT) {
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            sel_follow(s, (uint8_t)((s->sel + (ev->key == KEY_DOWN ? 1 : n - 1)) % n));
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->mode == M_SWAP) {
                if(s->sel != s->from) swap_rows(s, s->from, s->sel);
                to_list(c, s);
            } else if(s->mode == M_SOFT) {
                soft_give(c, s, s->sel);
            } else if(s->pick) {
                pick_on(c, s, s->sel);
            } else if(s->use) {
                use_on(c, s, s->sel);
            } else {
                opt_open(s);
                scene_dirty(c, D_COMPOSE);
            }
        }
    } else if(s->mode == M_OPT) {
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            const int rows = opt_rows(s), vis = (SCREEN_H - 4) / font_pitch();
            s->opt = (uint8_t)((s->opt + (ev->key == KEY_DOWN ? 1 : rows - 1)) % rows);
            if(s->opt < s->otop) s->otop = s->opt;
            if(s->opt >= s->otop + vis) s->otop = (uint8_t)(s->opt - vis + 1);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            opt_pick(c, s, s->opt);
        }
    } else { // summary
        if(!press) return;
        if(ev->key == KEY_LEFT && s->page > 0) {
            s->page--;
            scene_dirty(c, D_WORLD);
        } else if(ev->key == KEY_RIGHT && s->page + 1 < PAGES_N) {
            s->page++;
            scene_dirty(c, D_WORLD);
        } else if(ev->key == KEY_OK) {
            if(s->page + 1 < PAGES_N) {
                s->page++;
                scene_dirty(c, D_WORLD);
            } else {
                to_list(c, s);
            }
        } else if((ev->key == KEY_UP && s->cur > 0) || (ev->key == KEY_DOWN && s->cur + 1 < n)) {
            uint8_t pg = s->page;
            to_summary(c, s, s->cur + (ev->key == KEY_DOWN ? 1 : -1)); // Up / Down browse the party
            s->page = pg;
        }
    }
}

// ---------------------------------------------------------------------------------------------------- drawing
static void draw_world(SceneCtx* c, void* d) {
    PS* s = d;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, 0);
    if(s->mode != M_SUM || s->page != 0 || !s->pic.blk || !s->fw) return;
    for(int y = 0; y < s->fh; y++) {
        if(y == 0 || y == s->fh - 1) lvl_span(lvl, s->fy + y, s->fx + 1, s->fx + s->fw - 1, 3); // trimmed corners
        else {
            lvl_put(lvl, s->fx, s->fy + y, 3);
            lvl_put(lvl, s->fx + s->fw - 1, s->fy + y, 3);
        }
    }
    blit_masked(lvl, &s->pic.b, s->fx + PAD_X, s->fy + PAD_Y);
}

static int num_px(int v) {
    char t[12];
    int n = hud_num(t, v), w = 0;
    for(int i = 0; i < n; i++) w += font_adv((tchar)t[i]);
    return w;
}

// Font digits, right edge at xr.
static void num_r(int v, int xr, int y) {
    char t[12];
    int n = hud_num(t, v), x = xr - num_px(v);
    for(int i = 0; i < n; i++) {
        font_draw_glyph((tchar)t[i], x, y, 1);
        x += font_adv((tchar)t[i]);
    }
}

// "20/20" in the tiny font; returns its width.
static int hp_nums(char* out, int hp, int max) {
    int n = hud_num(out, hp);
    out[n++] = '/';
    hud_num(out + n, max);
    return tiny_width(out);
}

static int tiny_y(int row_y) { // tiny glyphs sit on the language font's baseline
    return row_y + font_baseline() - 5;
}

static void draw_list(PS* s) {
    const int p = font_pitch(), n = count();
    const int nx = LIST_X + font_cell_w() + 2;
    for(int v = 0; v < s->vis && s->top + v < n; v++) {
        const int k = s->top + v;
        const Mon* m = mon_at(k);
        const int y = 1 + v * s->rh;
        if(k == s->sel) font_draw_glyph(GLYPH_CURSOR, LIST_X, y, 1);
        else if((s->mode == M_SWAP || s->mode == M_SOFT) && k == s->from) { // Cambiar / Hearty Broth: the row picked first keeps a hollow arrow
            static const uint8_t HOLLOW[7] = {0x80, 0xC0, 0xA0, 0x90, 0xA0, 0xC0, 0x80};
            ov_bitmap(LIST_X, y + font_baseline() - 7, 5, 7, HOLLOW, 1, 1);
        }
        font_draw_text(s->name[k], nx, y);
        char t[12];
        hud_level(t, m->level);
        tiny_draw(t, LIST_RIGHT - tiny_width(t), tiny_y(y));
        if(bc_shiny(m)) star_draw(LIST_RIGHT - tiny_width(t) - 7, tiny_y(y)); // contract 65: variocolor
        const int by = y + p + 1;
        const int hp = s->an_k == k ? s->an_hp : m->hp; // a healing item drains the bar up to the new HP
        hpbar_draw(nx, by, BAR_W, hp, m->max_hp);
        char nums[16];
        hp_nums(nums, hp, m->max_hp);
        tiny_draw(nums, nx + BAR_W + 3, by);
        const char* tag = hud_status_tag(m->status, hp);
        if(tag) tiny_draw(tag, LIST_RIGHT - tiny_width(tag), by);
    }
    // scroll arrows (5 x 3)
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(122, 1, 5, 3, UP, 1, 1);
    if(s->top + s->vis < n) ov_bitmap(122, SCREEN_H - 4, 5, 3, DN, 1, 1);
}

static void draw_option_box(PS* s) {
    const int p = font_pitch();
    ov_fill(s->ox, s->oy, s->ow, s->oh, 0);
    ov_frame(s->ox, s->oy, s->ow, s->oh, 1);
    for(int v = 0, i = s->otop; v < (s->oh - 4) / p; v++, i++) {
        const int y = s->oy + 2 + v * p, r = i - s->nfm;
        if(s->opt == i) font_draw_glyph(GLYPH_CURSOR, s->ox + 3, y, 1);
        font_draw_text(r < 0 ? s->mv[i] : r == 0 ? s->l_stats : r == 1 ? s->l_switch : s->l_cancel, s->ox + 3 + font_cell_w() + 2, y);
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20}; // contract 81: more rows above / below (the box scrolls when the field moves do not fit)
    if(s->otop > 0) ov_bitmap(s->ox + s->ow - 6, s->oy, 5, 3, UP, 1, 1);
    if(s->otop + (s->oh - 4) / p < opt_rows(s)) ov_bitmap(s->ox + s->ow - 6, s->oy + s->oh - 3, 5, 3, DN, 1, 1);
}

// The forget list: the learner's 4 moves in a box at the bottom right (like the option box).
static void draw_forget(PS* s) {
    const int p = font_pitch();
    int w = 0;
    for(int i = 0; i < 4; i++) {
        int t = font_text_width(s->mv[i]);
        if(t > w) w = t;
    }
    w += font_cell_w() + 9 + 4;
    const int h = 4 * p + 4, x = SCREEN_W - w, y = SCREEN_H - h;
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
    for(int i = 0; i < 4; i++) {
        if(s->fsel == i) font_draw_glyph(GLYPH_CURSOR, x + 3, y + 2 + i * p, 1);
        font_draw_text(s->mv[i], x + 3 + font_cell_w() + 2, y + 2 + i * p);
    }
}

static void draw_page_no(PS* s) {
    char t[4] = {(char)('1' + s->page), '/', (char)('0' + PAGES_N), 0};
    tiny_draw(t, SCREEN_W - 2 - tiny_width(t), SCREEN_H - 6);
}

// Page 1: picture + name, level, status, HP, types.
static void draw_sum_info(PS* s, const Mon* m) {
    const int p = font_pitch();
    const int x0 = s->fw ? s->fx + s->fw + 4 : 8;
    int y = 1;
    font_draw_text(s->name[s->cur], x0, y);
    y += p + 1;
    char t[12];
    hud_level(t, m->level);
    tiny_draw(t, x0, y);
    const char* tag = hud_status_tag(m->status, m->hp);
    int sx = x0 + tiny_width(t) + 6;
    if(tag) {
        tiny_draw(tag, sx, y);
        sx += tiny_width(tag) + 4;
    }
    if(bc_shiny(m)) star_draw(sx, y); // contract 65: variocolor
    y += 8;
    char nums[16];
    int nw = hp_nums(nums, m->hp, m->max_hp);
    const char* lab = text_language() == LANG_IDX_ES ? "PS" : "HP"; // as on the battle plates
    tiny_draw(lab, x0, y);
    int bx = x0 + tiny_width(lab) + 2;
    int bw = SCREEN_W - 2 - nw - 3 - bx;
    if(bw > SUM_BAR_MAX) bw = SUM_BAR_MAX;
    if(bw < 16) bw = 16;
    hpbar_draw(bx, y, bw, m->hp, m->max_hp);
    tiny_draw(nums, bx + bw + 3, y);
    y += 9;
    for(int i = 0; i < s->n_ty; i++) font_draw_text(s->ty[i], x0, y + i * p);
}

// Pages 2 and 3 share a header: name, level, page number, a rule. Returns the y of the first row.
static int draw_sum_head(PS* s, const Mon* m) {
    const int p = font_pitch();
    font_draw_text(s->name[s->cur], 4, 0);
    char t[12];
    hud_level(t, m->level);
    tiny_draw(t, 4 + font_text_width(s->name[s->cur]) + 5, tiny_y(0));
    if(bc_shiny(m)) star_draw(4 + font_text_width(s->name[s->cur]) + 9 + tiny_width(t), tiny_y(0)); // contract 65
    char pg[4] = {(char)('1' + s->page), '/', (char)('0' + PAGES_N), 0};
    tiny_draw(pg, SCREEN_W - 2 - tiny_width(pg), tiny_y(0));
    ov_fill(2, p + 1, SCREEN_W - 4, 1, 1);
    return p + 4;
}

static void draw_sum_stats(PS* s, const Mon* m) {
    const int p = font_pitch();
    const int y0 = draw_sum_head(s, m);
    for(int i = 0; i < 4; i++) {
        font_draw_text(s->st[i], 4, y0 + i * p);
        num_r(m->stat[i], STAT_VAL_R, y0 + i * p);
    }
    // EXP: points, and what is left to the next level
    const BcData* d = party_data();
    uint32_t next_left = 0;
    if(d && m->species < d->n_species && m->level < 100) {
        uint32_t need = bc_exp_for_level(bc_species(d, m->species)->growth, (uint8_t)(m->level + 1));
        next_left = need > m->exp ? need - m->exp : 0;
    }
    char t[12];
    font_draw_text(s->l_exp, COL2_X, y0);
    hud_num(t, (int)m->exp);
    tiny_draw(t, COL2_X, tiny_y(y0 + p));
    font_draw_text(s->l_next, COL2_X, y0 + 2 * p);
    hud_num(t, (int)next_left);
    tiny_draw(t, COL2_X, tiny_y(y0 + 3 * p));
}

static void draw_sum_moves(PS* s, const Mon* m) {
    const int p = font_pitch();
    const BcData* d = party_data();
    const int y0 = draw_sum_head(s, m);
    for(int i = 0; i < 4; i++) {
        const int y = y0 + i * p;
        if(!m->moves[i] || !s->mv[i][0]) {
            font_draw_text((const tchar[]){'-', 0}, 4, y);
            continue;
        }
        font_draw_text(s->mv[i], 4, y);
        int max = d && m->moves[i] < d->n_moves ? bc_pp_max(bc_move(d, m->moves[i])->pp, m->pp[i]) : 0;
        char nums[16];
        int nw = hp_nums(nums, m->pp[i] & BC_PP_LEFT, max);
        const int nx = SCREEN_W - 4 - nw;
        tiny_draw(nums, nx, tiny_y(y));
        tiny_draw("PP", nx - 2 - tiny_width("PP"), tiny_y(y));
    }
}

static void draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    PS* s = d;
    if(s->mode != M_SUM) {
        draw_list(s);
        if(s->mode == M_OPT) draw_option_box(s);
        else if(s->mode == M_FORGET) draw_forget(s);
        return;
    }
    const Mon* m = mon_at(s->cur);
    if(s->page == 0) {
        draw_sum_info(s, m);
        draw_page_no(s);
    } else if(s->page == 1) draw_sum_stats(s, m);
    else draw_sum_moves(s, m);
}

const SceneOps scene_party_ops = {
    .enter = p_enter,
    .leave = p_leave,
    .input = p_input,
    .update = p_update,
    .draw_world = draw_world,
    .draw_overlay = draw_overlay,
    .pauses_below = true,
};

// item != 0: the list is the target of that healing item (opened by the bag screen).
static bool open_screen(SceneCtx* c, uint8_t item) {
    if(g || c->n >= MAX_SCENES || count() <= 0) return false;
    PS* s = calloc(1, sizeof(PS));
    if(!s) return false;
    s->use = item;
    g = s;
    if(!scene_push(c, &scene_party_ops, s)) {
        g = NULL;
        free(s);
        return false;
    }
    return true;
}

bool scene_party_open(SceneCtx* c) {
    return open_screen(c, 0);
}

bool scene_party_open_use(SceneCtx* c, uint8_t item) {
    return item && open_screen(c, item);
}

bool scene_party_open_pick(SceneCtx* c, uint8_t mode, uint8_t* out) {
    if(!out || !c->world || mode > 1) return false;
    *out = 255;
    if(!open_screen(c, 0)) return false;
    g->pick = (uint8_t)(mode + 1);
    g->out = out;
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_party_mode(void) { return g ? g->mode : -1; }
int scene_party_page(void) { return g ? g->page : -1; }
int scene_party_sel(void) { return g ? g->sel : -1; }
int scene_party_cur(void) { return g ? g->cur : -1; }
int scene_party_top(void) { return g ? g->top : -1; }
int scene_party_opt(void) { return g ? g->opt : -1; }
int scene_party_nfm(void) { return g ? g->nfm : -1; }
void scene_party_box(int* x, int* y, int* w, int* h) {
    *x = g ? g->ox : 0;
    *y = g ? g->oy : 0;
    *w = g ? g->ow : 0;
    *h = g ? g->oh : 0;
}
void scene_party_heap(uint32_t* bytes, uint32_t* blocks) {
    *bytes = *blocks = 0;
    if(!g) return;
    *bytes = sizeof(PS);
    *blocks = 1;
    if(g->pic.blk) {
        *bytes += (uint32_t)(((g->pic.b.w + 3) / 4 + (g->pic.b.w + 7) / 8) * g->pic.b.h);
        (*blocks)++;
    }
}
#endif
