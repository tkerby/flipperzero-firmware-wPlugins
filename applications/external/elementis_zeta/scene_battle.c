// BATTLE scene (contract section 10).
//
// Composition: NORMAL view (layout in 3/4 view: the enemy's front picture top-right, the player's back picture
// bottom-left, each on an iso platform, compact plates, a short text box) and FOCUS shots (one creature at native size
// on a bigger platform, no plates / box: send-out, attack, hit), plus the trainer INTRO view. Everything the core says
// (battle_core.h events) is turned, one event at a time, into a short queue of ops (cut / message / wait / anim / hp):
// the runner executes them in order; user input (menus) happens only when the queue is empty and the core waits.
// Pictures, platforms and effects are drawn into the gray planes (lvl) by draw_world; plates, boxes, menus and text
// are constant 1-bit overlay pixels.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene_battle.h"
#include "battle_features.h"
#include "link_host.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "party.h"
#include "affinity.h"
#include "bag.h"
#include "render.h"
#include "assets.h"
#include "sound.h"
#include "settings.h"
#include "battle_gfx.h"
#include "battle_fx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "gen/langs_gen.h"

// contract 82: a heal / cure event of side 1 is the trainer AI's item (BF_AI_ITEMS); without the switch the scene is the old one (side 0 only)
#if BF_AI_ITEMS
#define EV_SIDE(e) ((e)->side)
#else
#define EV_SIDE(e) 0
#endif
// ------------------------------------------------------------------------------------------------ layout
#define EN_RIGHT     125 // enemy picture: right edge (exclusive)
#define EN_BOT       43 //   last row
#define PL_LEFT      1 // player picture: left edge
#define PL_BOT       48 //   last row (the one-line text box starts at row 49 in Latin)
#define FOC_BOT      61 // focus shot: last row of the native picture
#define FOC_PLAT_RX  33
#define FOC_PLAT_RY  7
#define TR_PLAT_RX   17 // trainers' platforms (intro)
#define TR_PLAT_RY   5
#define RIVAL_RIGHT  20 // rival: right margin, bottom row
#define RIVAL_BOT    33
#define HERO_LEFT    20
#define HERO_BOT     34
#define BAR_W        28 // plate HP bar (frame included)
#define NAME_CAP     16
#define LBL_CAP      12
#define HOLD_MS      360 // a message that does not wait for OK stays this long once typed
#define HOLD_USED_MS 700 // "X used Y!" stays readable this long once typed, before anything moves
#define HOLD_LONG_MS 520 // outcome lines (missed, critical, super effective ...)
#define BREATH_MS    520
#define DRAIN_MIN_MS 500 // an HP bar drains in 500 ms + 18 ms per bar pixel (at most 1 s)
#define DRAIN_PX_MS  18
#define DRAIN_MAX_MS 1000
// attack timeline (ms since the attack animation started; contract 10.3)
#define AT_WIND      110 // the user steps back ...
#define AT_PEAK      220 // ... dashes in (peak = contact) ...
#define AT_END       420 // ... and returns
#define AT_PROJ0     150 // a projectile leaves here ...
#define AT_PROJ_MS   FX_PROJ_MS // ... and flies this long (300)
#define AT_HIT_PCT   55 // the hit lands (shake + blinks) at this percent of the effect
#define AT_BLINK_MS  90 // one half of a blink
#define AT_IMPACT_MS (6 * AT_BLINK_MS) // 3 blinks
#define AT_SHAKE_MS  280
#define AT_WHIFF_PCT 55 // a miss: the effect dies at this percent of its length, then a puff
#define Q_MAX \
    10 // ops of one event: the worst is a trainer's FIRST send-out of a variocolor creature (AN_TR_OUT, intro done, cut, fighter, message, focus cut, send-out, wait, cut, sparkle = 10; contract 72: it was 9 and overflowed); the Eclipse Form is 7 - 8. The host build exits when the queue overflows. Was 24, then 16, then 9 (contract 64 / 67); the queue is drained before the next event is translated
#define BT_BUF \
    64 // codes of one message of this scene (the longest battle text is 56 codes in the four languages (contract 67 checked: build/b3/maxlen.py); the general TB_BUF is 200): -240 B of heap, and -32 B more since contract 67
#define TRANS_FLASH_MS 85
#define TRANS_WIPE_MS  56
#define BAG_ROWS       3 // item rows visible in the bag box
#define CRY_SHAKE_MS   640 // one wobble of the capture crystal + the pause before the next
// The LEGENDARY intro (contract 52): 3 full-screen flashes of the world's last frame (LG_FLASH_MS each way, no wipe), a black beat, a white flash, the creature as a black silhouette on its platform
// (its cry sounds), then the silhouette fills in from the top; ~1.4 s in all, OK skips it.
#define LG_FLASH_MS    60
#define LG_BLACK_MS    100 // AN_LEGEND: the screen is black for this long ...
#define LG_WHITE_MS    170 // ... a white flash up to here ...
#define LG_CRY_MS      180 // ... the silhouette stands and its cry starts ...
#define LG_FILL_MS     600 // ... holds, and from here fills in over LG_FILL_LEN ms
#define LG_FILL_LEN    400

// ------------------------------------------------------------------------------------------------ types
typedef enum {
    V_NORMAL = 0,
    V_FOCUS,
    V_INTRO,
    V_BLACK
} View;
typedef enum {
    M_TRANS = 0,
    M_RUN,
    M_MENU,
    M_FIGHT,
    M_EXIT,
    M_BAG,
    M_PARTY
} Mode;
enum {
    PP_SWITCH = 0,
    PP_POTION,
    PP_FORCED,
    PP_SHIFT
}; // what the party list is for (PP_SHIFT: the free swap of battle style Shift, contract 75)
enum {
    AFTER_MENU = 0,
    AFTER_PARTY
}; // where the scene goes when a one-off message ends (the party list or the menu)

enum {
    BOP_NONE = 0,
    BOP_CUT,
    BOP_MSG,
    BOP_WAIT,
    BOP_ANIM,
    BOP_SET,
    BOP_HP,
    BOP_WAITHP,
    BOP_END
};
enum {
    MSF_OK = 1,
    MSF_NUM = 2,
    MSF_USED = 4,
    MSF_MONEY = 8,
    MSF_LONG = 16,
    MSF_ASK = 32,
    MSF_STOP = 64,
    MSF_SHIFT = 128
}; // BOP_MSG a flags (ASK: the message stays for a YES / NO (the forget-a-move dialog, contract 23; STOP = the 'stop learning?' one); USED: the 'used' line holds HOLD_USED_MS; LONG: HOLD_LONG_MS; MONEY: p[2] / p[3] = high / low 16 bits of the amount, shown as {v1})
enum {
    AM_NONE = 0,
    AM_HIT,
    AM_MISS,
    AM_CAST = 4
}; // AN_ATTACK p[0]: outcome (+ AM_CAST: a status move cast, the user hops instead of lunging)
enum {
    SET_FIGHTER = 0,
    SET_LEVEL,
    SET_STATUS,
    SET_SYNC,
    SET_HIDE,
    SET_INTRO_DONE,
    SET_RIVAL,
    SET_GONE,
    SET_FORM,
    SET_DUG
};
enum {
    AN_SLIDE = 1, // creature slides in from the edge (wild enemy)
    AN_SENDOUT, // white flash + grow
    AN_ATTACK, // lunge (or hop) + projectile + effect on the target + shake and 3 blinks, one timeline
    AN_PUMP, // status move: little hop
    AN_FX, // effect only
    AN_BLINK, // 3 blinks only
    AN_FAINT, // sink into the platform
    AN_TR_IN, // trainers slide in (intro)
    AN_TR_OUT, // trainers slide out
    AN_WIN_IN, // the beaten trainer slides in
    AN_THROW, // the capture crystal arcs from the player's side to the enemy
    AN_ABSORB, // white flash, the enemy shrinks into the crystal
    AN_DROP, // the crystal falls to the ground and bounces
    AN_WOBBLE, // `an_fx` wobbles, one per CRY_SHAKE_MS
    AN_CLICK, // caught: dim crystal + sparkles
    AN_BURST, // broke free: burst, the crystal vanishes
    AN_LEGEND, // a legendary wild creature appears: black screen, white flash, silhouette + cry, fills in (contract 52)
    AN_ECLIPSE // contract 67: the Eclipse Form (focus shot; an_fx 0 = a black disc closes over the creature, the sky goes dark and a white corona swells; 1 = after the pictures changed, the same backwards behind a white flash)
};
#define ECL_MS \
    1000 // each part of AN_ECLIPSE: the disc closes (0 .. 600), the sky darkens (480 .. 570), the corona swells (600 ..); the second part (after a white flash of 90) is the first played backwards
#define ECL_ICON_W 7

typedef struct {
    uint8_t op, a, b, c;
    uint16_t p[4];
} Op;

typedef struct {
    // contract 77: fields ordered by how often the code touches them (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access, above it a 32-bit one): hot small fields first, big blocks last
    uint8_t species; // 0 = nothing on this side
    uint8_t level, status; // displayed
    bool visible;
    int8_t ox, oy; // animation offsets
    uint8_t blink; // 0 normal, 1 inverted, 2 hidden
    uint8_t
        flat; // 0, else the picture is a flat silhouette of level (flat - 1) (absorbed into the crystal)
    uint8_t
        shiny; // contract 65: a variocolor creature (bc_shiny): its pictures get the swapped tones
    uint8_t
        form; // contract 67: its Eclipse Form (1 .. 3) is on: the pictures are the form's (mon.bin, after the trainer pictures)
    uint8_t sc_n; // 0 = unscaled, else n / 8
    uint8_t
        dug; // contract 85: underground / in the air (Dig, Fly charging): not drawn, the plate stays; it outlives the blinks and hops (blink does not)
    int16_t hp, max_hp, hp_to;
    uint16_t drain_acc, drain_ms,
        drain_delay; // HP bar: time since the last step, ms per step, wait before the first step
    int16_t sink; // faint: rows sunk into the platform
    int16_t x, y, cx, cy; // medium picture top-left, platform centre
    MonPic med, nat;
    Plat plat;
} Fighter;

typedef struct {
    // contract 77: fields ordered by how often the code touches them (a byte / half-word / word below offset 32 / 64 / 124 is a 16-bit Thumb access, above it a 32-bit one): hot small fields first, big blocks last
    bool more, one_line, wait_ok, typing, ok_pressed, arrow;
    uint16_t hold_ms;
    uint32_t tacc, hold, blink;
    int page, l0, l1, n0, n1, total, shown, w0, w1;
    tchar buf[BT_BUF];
} Msg;

typedef struct {
    // CONTRACT 77: the fields are ORDERED BY HOW OFTEN THE CODE TOUCHES THEM (tools: the `s->x` reference counts + the ARM offsets). A byte / half-word / word at an offset below 32 / 64 / 124 is a 16-bit
    // Thumb load or store, above that a 32-bit one: the 32 hottest bytes, then the hottest half-words, then the hottest words come first (-370 B of plugin code against the declaration order of the features),
    // and the big blocks (queue, message, fighters, platforms, text labels) come last. A new field goes where its use count puts it, not at the end.
    Mode mode;
    uint8_t an_kind;
    uint8_t pp; // PP_*
    uint8_t pm_sel;
    uint8_t bg_top;
    bool ghost; // C4-B (contract 36): an unidentified ghost (a wild battle in a MAPF_GHOSTS map (mapc `@ghosts yes`, contract 35) without the Lente Espectral in the bag): black silhouette, nothing the player does reaches it, Run always works
    uint8_t bg_sel; // bag cursor, first visible row, kinds
    bool trainer;
    uint8_t qn;
    uint8_t
        lrn_mv; // the move waiting to be learned (the forget-a-move dialog is on while it is not 0, contract 23)
    bool cur_active;
    uint8_t mv_sel;
    uint8_t an_side;
    uint8_t pm_opt;
    uint8_t qi;
    bool pm_box; // the Cambiar / Cancelar box is open
    uint8_t fx_kind; // effect being drawn (0 none)
    View view; // drawn view
    uint8_t pm_top;
    bool reserve; // C5-B (contract 43): a wild battle inside the Reserva Salvaje: the menu is Crystal / Rock / Bait / Run and Battle.reserve is set (bc_step's reserve_step)
#if BF_MOVE_COPY
    uint8_t
        cpy; // contract 73: Echoing is choosing: the move list shows the FOE's moves (cpy = 1) and `cpy_slot` is Echoing's own slot
    uint8_t cpy_slot;
#endif
    uint8_t
        ecl_sel; // contract 67: the cursor is on the Eclipse chip above the move list (not on a move)
    int8_t shx; // whole-world shake offset
    bool legend; // contract 52: a LEGENDARY wild battle (the map idiom `legend=`: the wild level had bit 7): the intro above, the `legendario` music (battle_open.c)
    uint8_t an_mode; // AN_ATTACK: AM_*, hit strength 1..4, fx_move_style()
    uint8_t bg_n;
    int8_t shy;
    uint8_t an_style;
    uint8_t after; // AFTER_*
    uint8_t mv_top;
    uint8_t fx_side;
    View tview; // view as of the end of the op queue (translation time)
    uint16_t an_t;
    uint16_t trans_t;
    bool tr_vis[2];
    uint16_t an_dur;
    uint8_t
        tsp[2]; // species on each side as of the end of the queue (messages need names after a faint)
    uint16_t fx_t;
    int16_t cry_x; // centre column, last body row
    int16_t cry_y;
    uint16_t exit_t;
    int16_t cry_py;
    uint16_t cur_t; // ms into the current op
    int16_t cry_px; // where it absorbs the enemy / where it rests
    int16_t cry_gx;
    int16_t cry_gy;
    uint16_t lrn_nm; // the learner's name for the messages (Op name field)
    Battle* b;
    int16_t tr_dx[2]; // trainer slide offsets (0 = rival, 1 = hero)
    int ev_n;
    int ev_i;
    uint32_t prize; // prize money of a beaten trainer (EV_MONEY), shown after the win text
    uint8_t* result_out;
    const tchar* fname; // ... the friend's name (codes; {v3} of the challenge / win lines)
    uint32_t clk;
    LinkFoe*
        own; // IR LINK exhibition (contract 45): the block that holds the COPY of our party the battle is fought with (the real one is never touched) and the friend's name; freed with the scene
    uint8_t an_fx;
    uint8_t menu_sel;
    uint8_t an_pw;
    uint8_t
        mv_vert; // contract 72: the move list is ONE scrolling column (the two columns do not fit the screen); its first visible slot
    uint8_t
        inv; // transition: the flash inversion currently applied to `lvl` (the frozen world frame)
    uint8_t fside; // focus side
    uint8_t tside;
    uint8_t
        tslot; // party slot of the player's creature as of the end of the queue (its nickname, task K)
    Op q[Q_MAX];
    Op cur;
    Msg m;
    uint8_t fx_big; // heavy version / a miss (drawn beside the target)
    uint8_t fx_miss;
    FxPlayer
        fxp; // the frames of the effect / projectile being played (streamed from files/fx.bin, battle_fx.c)
#ifndef NUCLEO_DEVICE
    uint8_t uses; // host harness: how many "X used Y!" lines have started
#endif
    Fighter f[2];
    MonPic rival; // trainer picture (trainer battles)
    Plat xplat; // contract 82: the trainers' platform (the intro / win view) or the focus platform (a focus shot): never both at once, so one block
    bool intro_done;
    bool have_rival; // the trainer picture exists (the win scene shows him)
    uint8_t exit_step;
    // the main menu's four labels, the move list (+ the type of the highlighted move), the bag rows and the party list are never on screen together and each is reloaded when its mode opens (menu_enter / fight_open /
    // bag_open / party_open): one block of the largest (contract 64: -240 B of heap in every battle; contract 67: the menu labels joined it, -96 B)
    union {
        tchar lbl[4][LBL_CAP];
        struct {
            tchar mvn[4][NAME_CAP + 2];
            tchar tname[NAME_CAP];
        };
        tchar itn[BAG_ROWS][NAME_CAP];
        tchar pname[BC_MAX_PARTY][NAME_CAP];
    };
    uint8_t
        mv_wl; // the widest name of the left / right column (px), the number of moves, the rows of the move box (2, or 1 .. FIGHT_ROWS in the one-column list): set by fight_open
    uint8_t mv_wr;
    uint8_t mv_n;
    uint8_t mv_rows;
    bool auto_switch;
    bool shifted; // contract 75: the "change creatures?" question of the trainer's next send-out has been asked (the same EV_SENDOUT is translated again after the answer)
    uint8_t item; // the salve / cure waiting for its target
    tchar l_switch[LBL_CAP], l_cancel[LBL_CAP];
    bool cry_on;
    uint8_t cry_dim;
    int8_t cry_tilt;
    uint8_t cur_mv_power;
    uint8_t breath;
    uint16_t last_dt;
} S;

// The trainer's own picture (mon.bin entry = trainer id - 1, contract 10.1); the rival's (entry 0) when that one is missing.
static bool load_trainer_pic(S* s) {
    uint8_t id = s->b->trainer_id;
    if(id > 1 && bpic_load(&s->rival, (uint8_t)(id - 1), BP_TRAINER)) return true;
    return bpic_load(&s->rival, 0, BP_TRAINER);
}

static S* g_s; // the scene state (heap, one battle at a time)

// ------------------------------------------------------------------------------------------------ small helpers
static const uint16_t STAT_TXT[6] = {
    TXT_GL_BT_ST_ATTACK,
    TXT_GL_BT_ST_DEFENSE,
    TXT_GL_BT_ST_SPEED,
    TXT_GL_BT_ST_SPECIAL,
    TXT_GL_BT_ST_ACCURACY,
    TXT_GL_BT_ST_EVASION};

_Static_assert(
    TXT_GL_BT_BROKE3 == TXT_GL_BT_BROKE0 + 3,
    "gl.bt.broke0..3 must be consecutive (es.txt)");

static uint16_t sp_txt(uint8_t sp) {
    return reg_sp_txt(sp);
}
// The name of a creature in a message (a text id of the Op, or NICK_REF + party slot when the player's creature has a nickname, task K).
#define NICK_REF 0xFE00u
static uint16_t nick_ref(uint8_t slot, uint8_t sp) {
    return party_nick(slot) ? (uint16_t)(NICK_REF + slot) : sp_txt(sp);
}
static __attribute__((noinline)) uint16_t
    nm_of(const S* s, int side) { // the name of the creature of `side` as the queue has it
    return side == 0 ? nick_ref(s->tslot, s->tsp[0]) : sp_txt(s->tsp[side]);
}
static uint16_t mv_txt(uint8_t mv) {
    if(mv == MV_STRUGGLE) return TXT_GL_BT_STRUGGLE;
    return reg_mv_txt(mv);
}
static __attribute__((noinline)) uint16_t it_txt(uint8_t it) {
    return reg_it_txt(it);
}
static uint16_t status_txt(uint8_t kind) {
    switch(kind) {
    case SK_SLEEP:
        return TXT_GL_BT_ASLEEP;
    case SK_POISON:
        return TXT_GL_BT_POISONED;
    case SK_BURN:
        return TXT_GL_BT_BURNED;
    case SK_FREEZE:
        return TXT_GL_BT_FROZEN;
    case SK_PARALYSIS:
        return TXT_GL_BT_PARALYZED;
    case SK_CONFUSION:
        return TXT_GL_BT_CONFUSED;
    default:
        return 0xFFFF;
    }
}
static __attribute__((noinline)) int pitch(void) {
    return font_pitch();
}
static int box1_h(void) {
    return pitch() + 2 * TB_PAD_Y;
}
static int box2_h(void) {
    return 2 * pitch() + 2 * TB_PAD_Y;
}
static inline int imax(int a, int b) {
    return a > b ? a : b;
}
static inline int imin(int a, int b) {
    return a < b ? a : b;
}

#define status_tag(st) hud_status_tag((st), 1) // SLP PSN BRN FRZ PAR / NULL (hud_gfx.c)

// ------------------------------------------------------------------------------------------------ queue
static Op* q_push(
    S* s,
    uint8_t op,
    uint8_t a,
    uint8_t b,
    uint8_t c,
    uint16_t p0,
    uint16_t
        p1) { // the new op (p[2], p[3] = 0; the few callers that need them set them: fewer arguments on the stack at every call, contract 67)
#ifndef NUCLEO_DEVICE
    if(s->qn >=
       Q_MAX) { // cannot happen (one event translates to <= 10 ops, Q_MAX is 10): the host tests must never see this
        fprintf(stderr, "scene_battle: the op queue is full (Q_MAX %d)\n", Q_MAX);
        exit(3);
    }
#endif
    Op* o = &s->q[s->qn < Q_MAX ? s->qn++ : Q_MAX - 1]; // (a full queue overwrites its last op)
    o->op = op;
    o->a = a;
    o->b = b;
    o->c = c;
    o->p[0] = p0;
    o->p[1] = p1;
    o->p[2] = o->p[3] = 0;
    return o;
}
static void q_op(S* s, uint8_t op) { // an op with no arguments (BOP_WAITHP, BOP_END)
    q_push(s, op, 0, 0, 0, 0, 0);
}
static void q_cut(S* s, View v, uint8_t side) {
    q_push(s, BOP_CUT, v, side, 0, 0, 0);
    s->tview = v;
    s->tside = side;
}
static void q_want(S* s, View v, uint8_t side) { // cut only when the view changes
    if(s->tview == v && (v != V_FOCUS || s->tside == side)) return;
    q_cut(s, v, side);
}
static void q_msg(S* s, uint16_t txt, uint16_t v0, uint16_t v1, uint16_t num, uint8_t flags) {
    if(s->tview != V_INTRO)
        q_want(
            s, V_NORMAL, 0); // (the trainer intro / the beaten trainer talk over their own view)
    Op* o = q_push(s, BOP_MSG, flags | (num ? MSF_NUM : 0), 0, 0, txt, v0);
    o->p[2] = v1;
    o->p[3] = num;
}
static __attribute__((noinline)) void q_learn(
    S* s,
    uint16_t txt,
    uint8_t mv,
    uint8_t flags) { // one line of the forget-a-move dialog: the learner (v0), a move (v1, 0 none)
    q_msg(s, txt, s->lrn_nm, mv ? mv_txt(mv) : 0, 0, MSF_OK | flags);
}
#define learn_ask(s, stop)                                    \
    q_learn(                                                  \
        s,                                                    \
        (stop) ? TXT_GL_LEARN_STOP_Q : TXT_GL_LEARN_FORGET_Q, \
        (s)->lrn_mv,                                          \
        MSF_ASK | ((stop) ? MSF_STOP : 0)) // the YES / NO question (answered by learn_answer)
// the two commonest shapes of q_msg as calls of their own (the translator has ~30 of them: fewer argument moves at every site; contract 67)
static void q_m1(
    S* s,
    uint16_t txt,
    uint16_t v0,
    uint8_t
        flags) { // a line with one name (v0) and no number (contract 85: four arguments, none on the stack; q_msg's 6 cost ~6 B at each of 11 more sites)
    q_msg(s, txt, v0, 0, 0, flags);
}
static void q_m0(S* s, uint16_t txt, uint8_t flags) { // a line with no names in it
    q_m1(s, txt, 0, flags);
}
static void
    q_mn(S* s, uint16_t txt, int side, uint8_t flags) { // a line that names the creature of `side`
    q_m1(s, txt, nm_of(s, side), flags);
}
static void q_prize(
    S* s) { // "<player> got $N for winning!" after the beaten trainer's line (the amount was paid at EV_MONEY; contract 82: a won or caught WILD battle with Pilfer's coins says it too)
    if(!s->prize) return;
    if(s->tview != V_INTRO) q_want(s, V_NORMAL, 0);
    Op* o = q_push(s, BOP_MSG, MSF_OK | MSF_MONEY, 0, 0, TXT_GL_BT_MONEY, 0);
    o->p[2] = (uint16_t)(s->prize >> 16);
    o->p[3] = (uint16_t)s->prize;
}
static void q_wait(S* s, uint16_t ms) {
    q_push(s, BOP_WAIT, 0, 0, 0, ms, 0);
}
static void q_anim(S* s, uint8_t kind, uint8_t side, uint8_t fx) {
    q_push(s, BOP_ANIM, kind, side, fx, 0, 0);
}
#ifndef NUCLEO_DEVICE
unsigned
    g_sb_sparkles; // host tests: how many variocolor sparkles the scene has queued (contract 65)
int g_sb_shiny_foe; // host tests: scene_battle_trainer makes the trainer's creatures variocolors (contract 72)
#endif
static void q_sparkle(
    S* s,
    uint8_t
        side) { // contract 65: a variocolor creature sparkles when it appears (the three star bursts of the fighting-type effect over its picture)
    q_anim(s, AN_FX, side, FX_STARS3);
#ifndef NUCLEO_DEVICE
    g_sb_sparkles++;
#endif
}
static void q_set2(
    S* s,
    uint8_t what,
    uint8_t side,
    uint16_t val,
    uint16_t val2) { // (the two ops with a second number: SET_FIGHTER's HP, SET_FORM's species)
    q_push(s, BOP_SET, what, side, 0, val, val2);
}
static void q_set(
    S* s,
    uint8_t what,
    uint8_t side,
    uint16_t
        val) { // (contract 85: four arguments, none on the stack: the 5th of the old signature cost ~4 B at each of the 19 call sites)
    q_push(s, BOP_SET, what, side, 0, val, 0);
}
static void q_hp(
    S* s,
    uint8_t side,
    uint16_t hp,
    uint16_t delay) { // delay: ms before the bar starts to move
    q_push(s, BOP_HP, side, 0, 0, hp, delay);
}

// ------------------------------------------------------------------------------------------------ fighters
static void fighter_free_pics(Fighter* f) {
    monpic_free(&f->med);
    monpic_free(&f->nat);
    plat_free(&f->plat);
}

static int plat_rx_for(int w) {
    int rx = w * 62 / 100;
    return rx < 16 ? 16 : (rx > 30 ? 30 : rx);
}

static void fighter_layout(
    Fighter* f,
    int side) { // where the medium picture and its platform stand, and the platform
    int w = f->med.b.w, h = f->med.b.h;
    if(side == 1) {
        f->x = (int16_t)(EN_RIGHT - w);
        f->y = (int16_t)(EN_BOT - h + 1);
    } else {
        f->x = PL_LEFT;
        f->y = (int16_t)(PL_BOT - h + 1);
    }
    int rx = plat_rx_for(w);
    int cx = f->x + w / 2;
    if(cx + rx > SCREEN_W - 2) cx = SCREEN_W - 2 - rx;
    if(cx - rx < 1) cx = 1 + rx;
    f->cx = (int16_t)cx;
    f->cy = (int16_t)(f->y + h - 3);
    plat_free(&f->plat);
    int ry = rx / 3;
    plat_build(&f->plat, rx, ry < 4 ? 4 : ry, 2);
}

static void plats_back(
    S* s) { // contract 82: the platforms of the normal view that a focus shot let go (fighter_layout puts them where they were and builds them again; plat_build reads the baked shape: nothing big is held meanwhile)
    for(int k = 0; k < 2; k++)
        if(!s->f[k].plat.blk && s->f[k].med.blk) fighter_layout(&s->f[k], k);
}

// Contract 67: a creature's picture of kind `kind` (BP_*): its species', or its Eclipse Form's. The form's four pictures are entries of mon.bin after the trainer pictures (tools/monc.py): "trainer picture" T + 4 * (form - 1) + kind
// with T = the real trainer pictures = n_trainers - 1 (the loader is the trainers' one: no new code, no new file).
#define fighter_pic(s, p, species, form, kind)                                         \
    ((form) ? bpic_load(                                                               \
                  (p),                                                                 \
                  (uint8_t)((s)->b->data->n_trainers - 1 + 4 * ((form) - 1) + (kind)), \
                  BP_TRAINER) :                                                        \
              bpic_load((p), (species), (kind)))

// A creature takes the field (hidden until its send-out animation).
static void fighter_set(
    S* s,
    int side,
    uint8_t species,
    uint8_t level,
    uint8_t status,
    int hp,
    int max_hp,
    bool shiny,
    uint8_t
        form) { // form: its Eclipse Form (contract 67; SET_FORM calls this again with the creature's own values), else 0
    Fighter* f = &s->f[side];
    f->shiny = shiny;
    f->form = form;
    monpic_free(&f->med);
    monpic_free(&f->nat);
    f->species = species;
    f->level = level;
    f->status = status;
    f->hp = f->hp_to = (int16_t)hp;
    f->max_hp = (int16_t)max_hp;
    f->drain_acc = f->drain_delay = 0;
    f->visible = false;
    f->ox = f->oy = 0;
    f->blink = f->dug = 0;
    f->flat = 0;
    f->sc_n = 0;
    f->sink = 0;
    tchar nm[NAME_CAP]; // (warms the glyph cache of the SD-backed fonts; it was a 32 B field of the Fighter that nothing read: contract 67.1)
    nm[0] = 0;
    if(sp_txt(species) != 0xFFFF) text_load(sp_txt(species), nm, NAME_CAP);
    const tchar* one[1] = {nm};
    text_prefetch_many(one, 1);
    const uint8_t kind = side == 1 ? BP_FRONT_MED : BP_BACK_MED;
    if(!fighter_pic(s, &f->med, species, form, kind) &&
       form) { // (a form picture the card cannot give: the normal one)
        f->form = 0;
        (void)fighter_pic(s, &f->med, species, 0, kind);
    }
    if(shiny) monpic_shiny(&f->med);
    if(f->med.blk)
        fighter_layout(f, side);
    else { // no art for this species: the platform and the plate still work
        f->x = (int16_t)(side == 1 ? EN_RIGHT - 24 : PL_LEFT);
        f->y = (int16_t)(side == 1 ? EN_BOT - 30 : PL_BOT - 30);
        f->cx = (int16_t)(f->x + 12);
        f->cy = (int16_t)(f->y + 28);
    }
}

static bool fighter_load_native(const S* s, Fighter* f, int side) {
    if(f->nat.blk) return true;
    const bool ok =
        fighter_pic(s, &f->nat, f->species, f->form, side == 1 ? BP_FRONT_NAT : BP_BACK_NAT);
    if(f->shiny) monpic_shiny(&f->nat);
    return ok;
}

// Placement of the picture currently drawn for a fighter (focus or medium).
static void fighter_pos(const S* s, const Fighter* f, bool focus, int* x, int* y, int* w, int* h) {
    const MonPic* p = focus ? &f->nat : &f->med;
    *w = p->b.w ? p->b.w : 24;
    *h = p->b.h ? p->b.h : 30;
    if(focus) {
        *x = (SCREEN_W - *w) / 2;
        *y = FOC_BOT - *h + 1;
    } else {
        *x = f->x;
        *y = f->y;
    }
    *x += f->ox + s->shx;
    *y += f->oy + s->shy;
}

// ------------------------------------------------------------------------------------------------ messages
static int sep_px(void) {
    uint8_t l = text_language();
    return (l == LANG_IDX_ZH || l == LANG_IDX_JA) ? 0 : font_adv(' ');
}

static void msg_page(Msg* m, int page) {
    m->page = page;
    m->l0 = page;
    m->l1 = text_line_start(m->buf, page, 1);
    m->n0 = text_line_glyphs(m->buf, m->l0);
    m->n1 = m->l1 >= 0 ? text_line_glyphs(m->buf, m->l1) : 0;
    m->w0 = text_line_px(m->buf, m->l0);
    m->w1 = m->l1 >= 0 ? text_line_px(m->buf, m->l1) : 0;
    m->total = m->n0 + m->n1;
    m->shown = 0;
    m->more = text_next_page(m->buf, page) >= 0;
    int limit = (m->more || m->wait_ok) ? (TB_W - TB_MORE_RIGHT - font_cell_w() - TB_TEXT_X) :
                                          (TB_W - 2 * TB_TEXT_X);
    m->one_line = m->l1 < 0 || (m->w0 + sep_px() + m->w1 <= limit);
    m->typing = true;
    m->ok_pressed = false;
    m->arrow = true;
    m->tacc = m->hold = m->blink = 0;
    text_prefetch_page(m->buf, page);
}

#ifndef NUCLEO_DEVICE
void (*scene_battle_msg_hook)(
    const tchar* buf); // host tests: every message that starts (its codes, {vN} still unexpanded)
#endif

// {v3} = the trainer's class + name ("Explorador Teo", trainers.bin byte 26) for the lines that name him; the rival (or a trainer without
// a text key) is the {rival} name the player chose.
static void trainer_name_var(const S* s) {
    if(s->fname) { // the friend of an exhibition battle
        text_set_var(3, s->fname);
        return;
    }
    uint8_t b[2] = {0xFF, 0xFF};
    read_at(
        "trainers.bin",
        12u + s->b->trainer_id * 32u + 26u,
        b,
        2); // the name's text id is in the trainer's record (regc.py, contract 64)
    const uint16_t t = (uint16_t)(b[0] | b[1] << 8);
    if(t >= 0xFFFE || !text_set_var_id(3, t)) text_set_var_rival(3);
}

static bool msg_begin(S* s, const Op* o) {
    Msg* m = &s->m;
    text_clear_vars();
    if(s->trainer && (o->p[0] == TXT_GL_BT_CHALLENGE || o->p[0] == TXT_GL_BT_RIVAL_SENDOUT ||
                      o->p[0] == TXT_GL_BT_WIN || o->p[0] == TXT_GL_BT_SHIFT_NEXT
#if BF_AI_ITEMS || BF_AI_SWITCH
                      || o->p[0] == TXT_GL_TAI_AIUSE || o->p[0] == TXT_GL_TAI_AIOUT
#endif
                      ))
        trainer_name_var(s);
    if(o->p[1] >= NICK_REF && o->p[1] < NICK_REF + BC_MAX_PARTY)
        text_set_var(0, party_nick((uint8_t)(o->p[1] - NICK_REF)));
    else if(o->p[1] != 0 && o->p[1] != 0xFFFF)
        text_set_var_id(0, o->p[1]);
    if(o->a & MSF_MONEY)
        text_set_var_num(1, (int)(((uint32_t)o->p[2] << 16) | o->p[3]));
    else if(o->p[2] != 0 && o->p[2] != 0xFFFF)
        text_set_var_id(1, o->p[2]);
    if(o->a & MSF_NUM) text_set_var_num(2, o->p[3]);
    m->wait_ok = (o->a & MSF_OK) != 0;
    m->hold_ms = (o->a & MSF_USED) ? HOLD_USED_MS : ((o->a & MSF_LONG) ? HOLD_LONG_MS : HOLD_MS);
    if(o->p[0] == 0xFFFF || text_load(o->p[0], m->buf, BT_BUF) < 0) return false;
    msg_page(m, 0);
#ifndef NUCLEO_DEVICE
    if(scene_battle_msg_hook) scene_battle_msg_hook(m->buf);
#endif
    return true;
}

// Advances a message by dt ms. Returns true when it is finished (the box then closes).
static bool msg_tick(SceneCtx* c, S* s, uint32_t dt) {
    Msg* m = &s->m;
    if(dt > 100) dt = 100;
    if(m->typing) {
        uint32_t ms = text_glyph_ms();
        m->tacc += dt;
        int n = ms ? (int)(m->tacc / ms) : m->total;
        if(n > 0) {
            m->tacc = ms ? m->tacc % ms : 0;
            m->shown = n < m->total - m->shown ? m->shown + n : m->total;
            scene_dirty(c, D_COMPOSE);
        }
        if(m->shown >= m->total) {
            m->typing = false;
            m->hold = 0;
            m->blink = 0;
            m->arrow = true;
            scene_dirty(c, D_COMPOSE);
        }
        return false;
    }
    bool needs_ok = m->wait_ok;
    m->hold += dt;
    if(needs_ok) {
        m->blink += dt;
        if(m->blink >= 400) {
            m->blink %= 400;
            m->arrow = !m->arrow;
            scene_dirty(c, D_COMPOSE);
        }
    }
    bool go = m->ok_pressed || (!needs_ok && m->hold >= m->hold_ms);
    if(!go) return false;
    int nx = text_next_page(m->buf, m->page);
    if(nx >= 0) {
        msg_page(m, nx);
        scene_dirty(c, D_COMPOSE);
        return false;
    }
    return true;
}

// A message is on screen / the YES / NO question of the forget-a-move dialog (a BOP_MSG with MSF_ASK; its box is drawn once it is typed).
static bool msg_on(const S* s) {
    return s->mode == M_RUN && s->cur_active && s->cur.op == BOP_MSG;
}
static bool ask_on(const S* s) {
    return msg_on(s) && (s->cur.a & MSF_ASK);
}

// ------------------------------------------------------------------------------------------------ the event translator
static int hp_before(
    const S* s,
    int from,
    int side,
    int fallback) { // HP a creature had when it was sent out
    for(int i = from + 1; i < s->ev_n; i++) {
        const BcEvent* e = &s->b->ev[i];
        if(e->side != side) continue;
        if(e->type == EV_DAMAGE || e->type == EV_SELF_HIT || e->type == EV_RESIDUAL ||
           e->type == EV_RECOIL)
            return (int)e->w + (int)e->v;
    }
    return fallback;
}

// ms into an AN_ATTACK at which the hit lands (the shake and the blinks start): the effect has played 55 %.
static unsigned attack_fx0(uint8_t style) { // when the effect on the target starts
    return (style & MST_PROJ) ? AT_PROJ0 + AT_PROJ_MS : AT_PEAK;
}
static unsigned attack_impact_ms(uint8_t style) {
    return attack_fx0(style) + (unsigned)fx_duration(style & MST_KIND) * AT_HIT_PCT / 100;
}

static int pick_switch(const S* s);
static bool shift_wanted(
    const S*
        s) { // contract 75: the player's creature is in play and well, another could take its place; not in an exhibition battle
    const Battle* b = s->b;
    return !(g_settings.battle & SET_BATTLE_SET) && !b->no_reward && b->phase == BP_CHOOSE &&
           b->party[0]->mon[b->side[0].active].hp && pick_switch(s) >= 0;
}

static __attribute__((noinline)) void translate(S* s, int i) {
    const BcEvent* e = &s->b->ev[i];
    Battle* b = s->b;
    switch(e->type) {
    case EV_SENDOUT: {
        int side = e->side;
        const Mon* m = &b->party[side]->mon[e->a];
        if(side == 1 && s->trainer && s->intro_done &&
           !(BF_AI_SWITCH &&
             e->b)) { // contract 75 (contract 82: not when the trainer's AI switched, e->b = 1: asks only after a faint): battle style Shift: "<trainer> will use X! Will you change creatures?" before the next one comes; the answer comes back here (the same event is translated again)
            if(s->shifted)
                s->shifted = false;
            else if(shift_wanted(s)) {
                q_m1(s, TXT_GL_BT_SHIFT_NEXT, sp_txt(m->species), 0);
                q_m0(s, TXT_GL_BT_SHIFT_Q, MSF_OK | MSF_ASK | MSF_SHIFT);
                s->shifted = true;
                s->ev_i--;
                break;
            }
        }
        int hp = hp_before(s, i, side, m->hp);
        int lvl = m->level;
        if(side == 0)
            for(int k = i + 1; k < s->ev_n; k++)
                if(b->ev[k].type == EV_LEVELUP &&
                   b->ev[k].a ==
                       e->a) { // (contract 74: the level-up of THIS creature; the Reparto Total makes others')
                    lvl = (int)b->ev[k].b - 1;
                    break;
                }
        const uint16_t prev_nm = nm_of(s, side);
        s->tsp[side] = m->species;
        if(side == 0) s->tslot = e->a;
        if(side == 1 && !s->ghost && !s->b->no_reward)
            reg_set_seen(m->species); // Registro: seen (not an unidentified ghost)
        if(side == 1) {
            if(s->trainer) {
                if(!s->intro_done) {
                    q_anim(s, AN_TR_OUT, 0, 0);
                    q_set(s, SET_INTRO_DONE, 0, 0);
                }
                q_cut(s, V_NORMAL, 0);
                q_set2(s, SET_FIGHTER, 1, (uint16_t)(e->a | (lvl << 8)), (uint16_t)hp);
                q_m1(s, TXT_GL_BT_RIVAL_SENDOUT, sp_txt(m->species), 0);
            } else {
                q_cut(s, V_NORMAL, 0);
                q_set2(s, SET_FIGHTER, 1, (uint16_t)(e->a | (lvl << 8)), (uint16_t)hp);
                q_anim(s, s->legend ? AN_LEGEND : AN_SLIDE, 1, 0);
                if(bc_shiny(m) && !s->ghost) q_sparkle(s, 1); // contract 65
                q_m1(s, TXT_GL_BT_WILD, s->ghost ? TXT_GL_C4B_GHOST_NAME : sp_txt(m->species), 0);
                if(s->ghost)
                    q_m0(s, TXT_GL_C4B_GHOST_CANT, 0); // "Darn! The GHOST can't be identified!"
                s->intro_done = true;
            }
        } else {
            if(s->trainer && !s->intro_done) {
                q_anim(s, AN_TR_OUT, 0, 0);
                q_set(s, SET_INTRO_DONE, 0, 0);
            }
            q_cut(s, V_NORMAL, 0);
            if(s->f[0].species &&
               s->f[0].visible) // a voluntary switch: the old creature is called back first
                q_m1(s, TXT_GL_BT_COMEBACK, prev_nm, 0);
            q_set2(s, SET_FIGHTER, 0, (uint16_t)(e->a | (lvl << 8)), (uint16_t)hp);
            q_m1(s, TXT_GL_BT_GO, nick_ref(e->a, m->species), 0);
        }
        if(!(side == 1 && !s->trainer)) { // a wild enemy slides in instead of popping out
            if(g_settings.battle & SET_BATTLE_ANIM_OFF)
                q_anim(
                    s,
                    AN_SENDOUT,
                    (uint8_t)side,
                    0); // contract 75: no focus shot, the op only shows the creature (and its cry)
            else {
                q_cut(s, V_FOCUS, (uint8_t)side);
                q_anim(s, AN_SENDOUT, (uint8_t)side, 0);
                q_wait(s, 220);
                q_cut(s, V_NORMAL, 0);
            }
            if(bc_shiny(m) && !s->ghost) q_sparkle(s, (uint8_t)side); // contract 65
        }
        break;
    }
    case EV_USE_MOVE: {
        const MoveDef* mv = bc_move_def(b, e->a);
        s->cur_mv_power = mv ? mv->power : 0;
        uint8_t st = fx_move_style(e->a, mv ? mv->type : TY_NORMAL), kind = st & MST_KIND;
        q_msg(s, TXT_GL_BT_USED, nm_of(s, e->side), mv_txt(e->a), 0, MSF_USED);
        q_set(
            s,
            SET_DUG,
            e->side,
            0); // contract 85: the creature that dug in / flew up is not drawn (its plate stays) until its second turn's attack starts (here), or the charge is broken off (a line that says it cannot move, a confusion hit, a cure: below; a faint or a switch: fighter_set)
        const BcEvent* n = i + 1 < s->ev_n ?
                               &b->ev[i + 1] :
                               NULL; // what the move does decides the whole animation
        if((!s->cur_mv_power && !kind) ||
           (n &&
            n->type ==
                EV_MSG)) { // (a line straight after "used": charging, storing, a stat_reset ...: the user only hops)
            q_anim(s, AN_PUMP, e->side, 0);
            break;
        }
        uint8_t mode = n && n->type == EV_MISS ? AM_MISS : AM_NONE, pw = 2;
        if(s->cur_mv_power && n && n->type == EV_DAMAGE && n->b && n->v) {
            mode = AM_HIT;
            pw = n->a ? 4 : (n->b > 10 ? 3 : (n->b < 10 ? 1 : 2));
            q_hp(s, n->side, n->w, (uint16_t)(attack_impact_ms(st) + AT_BLINK_MS));
        }
        if(!s->cur_mv_power) mode |= AM_CAST;
        q_push(s, BOP_ANIM, AN_ATTACK, e->side, kind, mode, pw)->p[2] = st;
        break;
    }
    case EV_MISS:
        q_mn(s, TXT_GL_BT_MISS, e->side, MSF_LONG);
        break;
    case EV_DAMAGE: { // the animation and the HP drain were queued with EV_USE_MOVE
        uint8_t t = e->side;
        if(i == 0 ||
           b->ev[i - 1].type !=
               EV_USE_MOVE) { // a later hit of a multi-hit move, Hold Fast's release: the hit lands here (contract 64)
            if(e->v) {
                q_hp(s, t, e->w, AT_BLINK_MS);
                q_anim(s, AN_BLINK, t, 0);
            }
            break;
        }
        if(e->b == 0 || e->v == 0) {
            q_mn(s, TXT_GL_BT_NOEFFECT, t, MSF_LONG);
            break;
        }
        if(e->a) q_m0(s, TXT_GL_BT_CRIT, MSF_LONG);
        if(e->b > 10)
            q_m0(s, TXT_GL_BT_SUPER, MSF_LONG);
        else if(e->b < 10)
            q_m0(s, TXT_GL_BT_NOTVERY, MSF_LONG);
        break;
    }
    case EV_STAT: {
        uint8_t t = e->side;
        int delta = (int16_t)e->v;
        if(delta == 0) {
            q_m0(s, TXT_GL_BT_NOTHING, MSF_LONG);
            break;
        }
        q_push(
            s,
            BOP_ANIM,
            AN_FX,
            t,
            delta > 0 ? FX_UP : FX_DOWN,
            (delta > 1 || delta < -1) ? 1 : 0,
            0); // two stages: more arrows
        q_msg(
            s,
            delta > 0 ? TXT_GL_BT_STATUP : TXT_GL_BT_STATDOWN,
            nm_of(s, t),
            e->a < 6 ? STAT_TXT[e->a] : 0,
            0,
            MSF_LONG);
        break;
    }
    case EV_STATUS: {
        uint8_t t = e->side;
        if(e->b) {
            q_m0(s, TXT_GL_BT_NOTHING, MSF_LONG);
            break;
        }
        q_anim(s, AN_FX, t, fx_for_status(e->a));
        q_set(s, SET_STATUS, t, e->a);
        q_mn(s, status_txt(e->a), t, MSF_LONG);
        break;
    }
    case EV_CURED: {
        uint16_t txt = e->a == SK_SLEEP ?
                           TXT_GL_BT_WAKE :
                           (e->a == SK_FREEZE ? TXT_GL_BT_THAW : TXT_GL_BT_CONF_OVER);
        if(e->a != SK_CONFUSION) q_set(s, SET_STATUS, e->side, 0xFF);
        q_mn(s, txt, e->side, 0);
        break;
    }
    case EV_CANT_MOVE: {
        static const uint16_t CM_TXT[7] = {
            TXT_GL_BT_FULLY_PARALYZED,
            TXT_GL_BT_SLEEPING,
            TXT_GL_BT_FROZEN_SOLID,
            TXT_GL_BT_FLINCH,
            TXT_GL_BT_FULLY_PARALYZED,
            TXT_GL_C64_TRAPPED,
            TXT_GL_F1_RECHARGE}; // (by CM_*; a stray code is the paralysis)
        const uint16_t txt = CM_TXT[e->a % 7];
        q_set(s, SET_DUG, e->side, 0);
        q_mn(s, txt, e->side, 0);
        break;
    }
    case EV_CONF_TURN:
        q_mn(s, TXT_GL_BT_CONFUSED, e->side, 0);
        break;
    case EV_SELF_HIT:
        q_set(s, SET_DUG, e->side, 0);
        q_mn(s, TXT_GL_BT_HURT_ITSELF, e->side, 0);
        q_hp(s, e->side, e->w, AT_BLINK_MS);
        q_anim(s, AN_BLINK, e->side, 0);
        break;
    case EV_RESIDUAL:
        q_mn(s, e->a == SK_POISON ? TXT_GL_BT_HURT_PSN : TXT_GL_BT_HURT_BRN, e->side, 0);
        q_hp(s, e->side, e->w, AT_BLINK_MS);
        q_anim(s, AN_BLINK, e->side, 0);
        break;
    case EV_FOCUS:
        q_anim(s, AN_FX, e->side, FX_UP);
        q_mn(s, TXT_GL_BT_FOCUS, e->side, 0);
        break;
    case EV_SCREEN:
        q_mn(s, TXT_GL_BT_SCREEN, e->side, 0);
        break;
    case EV_RECOIL: { // an HP change of the creature: the recoil, a drain / Recover / Rest (it goes up), the seed, a decoy paid for (contract 64: RC_*)
        static const uint16_t RC_TXT[7] = {
            TXT_GL_BT_RECOIL,
            TXT_GL_C64_ABSORB,
            TXT_GL_C64_REGAIN,
            TXT_GL_C64_SEEDHURT,
            TXT_GL_C64_NAP,
            TXT_GL_C64_SUB,
            TXT_GL_F1_CRASH};
        q_mn(s, RC_TXT[e->a % 7], e->side, 0);
        q_hp(s, e->side, e->w, AT_BLINK_MS);
        if((0x16u >> e->a) & 1)
            q_anim(s, AN_FX, e->side, FX_UP); // (RC_ABSORB, RC_REGAIN, RC_NAP)
        else
            q_anim(s, AN_BLINK, e->side, 0);
        break;
    }
    case EV_MSG: { // a line of the new moves (contract 64); e->b = the number it shows
        static const uint16_t MSG_TXT[MSG_N] = {
            TXT_GL_C64_HITS,
            TXT_GL_C64_OHKO,
            TXT_GL_C64_CHG_WIND,
            TXT_GL_C64_CHG_FLY,
            TXT_GL_C64_CHG_BASH,
            TXT_GL_C64_CHG_SKY,
            TXT_GL_C64_FLED,
            TXT_GL_C64_STAT_RESET,
            TXT_GL_C64_MIST,
            TXT_GL_C64_SEEDED,
            TXT_GL_C64_STORE,
            TXT_GL_C64_UNLEASH,
            TXT_GL_C64_SUBHIT,
            TXT_GL_C64_SUBEND,
            [MSG_RAGE] = TXT_GL_F1_RAGE,
            [MSG_DIG] = TXT_GL_TAI_CHG_DIG,
            [MSG_COINS] = TXT_GL_TAI_COINS,
            [MSG_AIUSE] = TXT_GL_TAI_AIUSE,
            [MSG_AIOUT] = TXT_GL_TAI_AIOUT,
            [MSG_FLUTE] = TXT_GL_TAI_FLUTE,
            [MSG_FLUTE0] = TXT_GL_TAI_FLUTE0,
            [MSG_WOKE] = TXT_GL_TAI_WOKE,
#if BF_DISABLE
            [MSG_UNDIS] = TXT_GL_F1_UNDIS,
            [MSG_DISABLED] = TXT_GL_F1_DISABLED,
#endif
#if BF_MOVE_COPY
            [MSG_MOVE_COPY] = TXT_GL_F1_MOVE_COPY,
#endif
#if BF_TRANSFORM
            [MSG_TRANSFORM] = TXT_GL_F1_TRANSFORM,
#endif
#if BF_CONVERT
            [MSG_CONVERT] = TXT_GL_F1_CONVERT,
#endif
#if BF_OBEY
            [MSG_LOAF] = TXT_GL_F1_LOAF,
            [MSG_NOOBEY] = TXT_GL_F1_NOOBEY,
            [MSG_TURNED] = TXT_GL_F1_TURNED,
            [MSG_IGNORED] = TXT_GL_F1_IGNORED,
            [MSG_NAP] = TXT_GL_F1_NAP,
#endif
        };
        if(e->a >= MSG_N) break;
#if BF_TRANSFORM
        uint16_t v1 = e->a == MSG_TRANSFORM ?
                          sp_txt(e->b) :
                      e->a >= MSG_MV0 && e->a <= MSG_MOVE_COPY ?
                          mv_txt(e->b) :
                          0; // contract 73: `b` names a move or a species in those lines
#else
        uint16_t v1 = e->a >= MSG_MV0 && e->a <= MSG_MOVE_COPY ?
                          mv_txt(e->b) :
                          0; // (contract 73: `b` names a move in those lines)
#endif
#if BF_AI_ITEMS
        if(e->a == MSG_AIUSE) v1 = it_txt(e->b); // contract 82: the trainer AI's item
#endif
#if BF_TRANSFORM
        if(e->a ==
           MSG_TRANSFORM) { // the creature changes behind a closing disc (the Eclipse Form's animation) into the picture of the target's species; its name does not
            q_cut(s, V_FOCUS, e->side);
            q_anim(s, AN_ECLIPSE, e->side, 0);
            q_set2(s, SET_FORM, e->side, 0, e->b);
            q_anim(s, AN_ECLIPSE, e->side, 1);
            q_cut(s, V_NORMAL, 0);
        }
#endif
        if(e->a == MSG_DIG || e->a == MSG_CHG_FLY)
            q_set(s, SET_DUG, e->side, 1); // (out of sight from the line on)
        q_msg(s, MSG_TXT[e->a], nm_of(s, e->side), v1, v1 ? 0 : e->b, MSF_LONG);
#if BF_AI_SWITCH
        if(e->a == MSG_AIOUT)
            q_set(s, SET_HIDE, 1, 0); // the trainer's creature is called back (contract 82)
#endif
        break;
    }
    case EV_FAILED:
        q_m0(s, TXT_GL_BT_FAILED, 0);
        break;
    case EV_FAINT:
        q_op(s, BOP_WAITHP);
        q_want(s, V_NORMAL, 0);
        q_wait(s, 200);
        q_anim(s, AN_FAINT, e->side, 0);
        q_set(s, SET_HIDE, e->side, 0);
        q_mn(s, TXT_GL_BT_FAINTED, e->side, 0);
        break;
    case EV_EXP: // (contract 74, the Reparto Total: a = 0xFF is the line of the others; the creatures' names are read by slot, the event's `a` / `w`)
        if(e->a == 0xFF)
            q_msg(s, TXT_GL_F2_SHARE, 0, 0, e->v, 0);
        else
            q_msg(s, TXT_GL_BT_EXP, nick_ref(e->a, b->party[0]->mon[e->a].species), 0, e->v, 0);
        break;
    case EV_LEVELUP:
        sound_play(SND_FX_LEVELUP); // (contract 28)
        g_battle_leveled |=
            (uint8_t)(1u
                      << e->a); // the party order does not change in a battle: the slot is the final one
        if(e->a == s->tslot) { // (a creature that is not in play has no plate to update)
            q_set(s, SET_LEVEL, 0, e->b);
            q_set(s, SET_SYNC, 0, e->a);
        }
        q_msg(
            s, TXT_GL_BT_LEVELUP, nick_ref(e->a, b->party[0]->mon[e->a].species), 0, e->b, MSF_OK);
        break;
    case EV_LEARN:
        if(e->v != BC_LEARN_FULL) {
            q_msg(
                s,
                TXT_GL_BT_LEARNED,
                nick_ref((uint8_t)e->w, b->party[0]->mon[e->w].species),
                mv_txt(e->a),
                0,
                MSF_OK);
            break;
        }
        s->lrn_mv =
            e->a; // 4 moves known: wants / knows 4 / forget one? (the Mon is untouched until the player picks)
        s->lrn_nm = nm_of(s, 0);
        q_learn(s, TXT_GL_LEARN_WANTS, e->a, 0);
        q_learn(s, TXT_GL_LEARN_FULL, 0, 0);
        learn_ask(s, 0);
        break;
    case EV_RUN_OK:
        q_m0(s, TXT_GL_BT_RAN, 0);
        break;
    case EV_RUN_FAIL:
        q_m0(s, e->a ? TXT_GL_BT_CANT_RUN : TXT_GL_BT_RUN_FAIL, 0);
        break;
    case EV_RESERVE: { // contract 43: rock, bait, the creature's mood, and its flight
        static const uint16_t SF_TXT[5] = {
            TXT_GL_C5B_SF_THREW_ROCK,
            TXT_GL_C5B_SF_THREW_BAIT,
            TXT_GL_C5B_SF_ANGRY,
            TXT_GL_C5B_SF_EATING,
            TXT_GL_C5B_SF_FLED};
        if(e->a == SF_FLED) q_set(s, SET_GONE, 1, 0); // it runs off: picture and plate leave
        q_mn(s, SF_TXT[e->a - 1], 1, 0);
        break;
    }
    case EV_THROW:
        q_m1(s, TXT_GL_BT_THREW, it_txt(e->a), 0);
        q_anim(s, AN_THROW, 0, 0);
        q_anim(s, AN_ABSORB, 1, 0);
        q_anim(s, AN_DROP, 0, 0);
        break;
    case EV_SHAKE:
        if(e->a)
            q_anim(s, AN_WOBBLE, 0, e->a);
        else
            q_wait(s, 500);
        break;
    case EV_BROKE:
        q_anim(s, AN_BURST, 0, 0);
        q_anim(s, AN_SENDOUT, 1, 0); // the enemy pops out again where the crystal was
        q_m0(s, (uint16_t)(TXT_GL_BT_BROKE0 + (e->a & 3)), 0);
        break;
    case EV_CAUGHT:
        sound_play(SND_FX_CAPTURE); // (contract 28)
        reg_set_caught(s->tsp[1]);
        q_anim(s, AN_CLICK, 0, 0);
        q_set(s, SET_GONE, 1, 0); // the enemy's plate goes
        q_m1(s, TXT_GL_BT_CAUGHT, sp_txt(s->tsp[1]), MSF_OK);
        if(e->a ==
           0xFF) { // party full: the creature waits in the pending slot for the PC box (contract 23)
            party_pending_set(&b->enemy.mon[b->side[1].active]);
            q_m1(s, TXT_GL_BT_PARTY_FULL, sp_txt(s->tsp[1]), MSF_OK);
        }
        break;
    case EV_HEAL:
        if(e->a ==
           b->side[EV_SIDE(e)]
               .active) { // (contract 82: side 1 = the trainer AI's salve; no line of its own, MSG_AIUSE said it)
            q_anim(s, AN_FX, EV_SIDE(e), FX_UP);
            q_hp(s, EV_SIDE(e), e->w, 0);
        }
        if(!EV_SIDE(e))
            q_msg(
                s,
                b->data->items[e->b].kind == IK_REVIVE ? TXT_GL_C64_REVIVED : TXT_GL_BT_HEALED,
                nick_ref(e->a, b->party[0]->mon[e->a].species),
                0,
                e->v,
                0);
        break;
    case EV_CURE: {
        static const uint16_t CT[5] = {
            TXT_GL_BT_WAKE,
            TXT_GL_BT_CURED_PSN,
            TXT_GL_BT_CURED_BRN,
            TXT_GL_BT_THAW,
            TXT_GL_BT_CURED_PAR}; // by SK_* - 1 (anything else: burn)
        uint16_t txt = e->v >= SK_SLEEP && e->v <= SK_PARALYSIS ? CT[e->v - 1] :
                                                                  TXT_GL_BT_CURED_BRN;
        if(e->a == b->side[EV_SIDE(e)].active) {
            q_anim(s, AN_FX, EV_SIDE(e), FX_UP);
            q_set(s, SET_STATUS, EV_SIDE(e), 0xFF); // the plate's status tag goes
        }
        if(!EV_SIDE(e)) q_m1(s, txt, nick_ref(e->a, b->party[0]->mon[e->a].species), 0);
        break;
    }
    case EV_MONEY:
        s->prize = (uint32_t)e->v | ((uint32_t)e->w << 16);
        money_add(
            s->prize); // the prize is paid when the trainer is beaten (shown after the win text)
        break;
    case EV_NEED_SWITCH:
        s->auto_switch = true;
        break;
    case EV_END:
        if(e->a == BR_WIN)
            sound_play(
                s->trainer ? SND_WIN_TRAINER : SND_WIN_WILD); // the victory fanfare (contract 28)
        q_op(s, BOP_WAITHP);
        if(e->a == BR_WIN && s->trainer && s->have_rival) {
            q_set(s, SET_RIVAL, 0, 0);
            q_cut(s, V_INTRO, 0);
            q_anim(s, AN_WIN_IN, 0, 0);
            q_m0(s, TXT_GL_BT_WIN, MSF_OK);
            q_prize(s);
        } else if(e->a == BR_WIN || e->a == BR_CAUGHT) {
            q_prize(s);
        } else if(e->a == BR_LOSE) {
            q_m0(s, TXT_GL_BT_LOSE, MSF_OK);
        }
        q_op(s, BOP_END);
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------------------------------------------ ops
static __attribute__((noinline)) void load_events(S* s) {
    s->ev_i = 0;
    s->ev_n = s->b->n_ev;
}

static int pick_switch(const S* s) {
    const Party* p = s->b->party[0];
    for(int i = 0; i < p->count; i++)
        if(i != s->b->side[0].active && p->mon[i].hp) return i;
    return -1;
}

static void anim_start(S* s, const Op* o) {
    s->an_kind = o->a;
    s->an_side = o->b;
    s->an_fx = o->c;
    s->an_t = 0;
    static const uint16_t DUR[AN_ECLIPSE + 1] = {// ms of each animation (0: worked out below)
                                                 [AN_SLIDE] = 380,
                                                 [AN_LEGEND] = LG_FILL_MS + LG_FILL_LEN,
                                                 [AN_SENDOUT] = 330,
                                                 [AN_PUMP] = 420,
                                                 [AN_BLINK] = AT_IMPACT_MS + 20,
                                                 [AN_FAINT] = 500,
                                                 [AN_TR_IN] = 360,
                                                 [AN_TR_OUT] = 320,
                                                 [AN_WIN_IN] = 360,
                                                 [AN_THROW] = 560,
                                                 [AN_ABSORB] = 420,
                                                 [AN_DROP] = 300,
                                                 [AN_CLICK] = 760,
                                                 [AN_BURST] = 340,
                                                 [AN_ECLIPSE] = ECL_MS};
    s->an_dur = o->a <= AN_ECLIPSE && DUR[o->a] ? DUR[o->a] : 1;
    switch(o->a) {
    case AN_SLIDE:
    case AN_LEGEND:
    case AN_SENDOUT:
        s->f[o->b].visible = true;
        break;
    case AN_ATTACK: {
        s->an_mode = (uint8_t)o->p[0];
        s->an_pw = (uint8_t)o->p[1];
        s->an_style = (uint8_t)o->p[2];
        unsigned f0 = attack_fx0(s->an_style), d = fx_duration(o->c), end;
        if((s->an_mode & 3) == AM_HIT)
            end = attack_impact_ms(s->an_style) + AT_IMPACT_MS;
        else if((s->an_mode & 3) == AM_MISS)
            end = f0 + d * AT_WHIFF_PCT / 100 + fx_duration(FX_WHIFF) + 80;
        else
            end = f0 + d;
        if(end < f0 + d && (s->an_mode & 3) == AM_HIT) end = f0 + d;
        if(end < AT_END) end = AT_END;
        s->an_dur = (uint16_t)(end + 120); // a short beat after the last blink
        break;
    }
    case AN_FX:
        s->an_dur = (uint16_t)(fx_duration(o->c) + 40);
        break;
    case AN_TR_IN:
        s->tr_vis[0] = s->tr_vis[1] = true;
        break;
    case AN_WIN_IN:
        s->tr_vis[0] = true;
        s->tr_dx[0] = 70;
        s->tr_vis[1] = false;
        break;
    case AN_THROW: {
        const Fighter* e = &s->f[1];
        int w = e->med.b.w ? e->med.b.w : 24, h = e->med.b.h ? e->med.b.h : 30;
        s->cry_px =
            (int16_t)(e->x + w / 2); // absorbs the enemy at its middle, rests on its platform
        s->cry_py = (int16_t)(e->y + h / 3 + CRYSTAL_H / 2);
        s->cry_gx = e->cx;
        s->cry_gy = e->cy;
        s->cry_on = true;
        s->cry_dim = 0;
        s->cry_tilt = 0;
        break;
    }
    case AN_WOBBLE:
        s->an_dur = (uint16_t)(CRY_SHAKE_MS * o->c);
        break;
    case AN_ECLIPSE:
        if(o->c)
            sound_play(
                SND_FX_LEGEND_CRY); // the flash: the legendary cry of contract 52 (no new sound)
        break;
    default:
        break;
    }
    s->fx_big = 0;
    s->fx_miss = 0;
    if(o->a == AN_FX) {
        s->fx_kind = o->c;
        s->fx_side = o->b;
        s->fx_big = (uint8_t)(o->p[0] & 1);
        s->fx_t = 0;
    } else if(o->a == AN_CLICK || o->a == AN_BURST) { // effect over the crystal (fx_side 2)
        s->fx_kind = o->a == AN_CLICK ? FX_STARS3 : FX_STAR;
        s->fx_side = 2;
        s->fx_t = 0;
    }
}

static int q16(int v, int num, int den) { // v * num / den, safe
    return (int)((long)v * num / den);
}

static void anim_apply(S* s) {
    Fighter* f = &s->f[s->an_side];
    int t = s->an_t, d = s->an_dur;
    switch(s->an_kind) {
    case AN_SLIDE: {
        int rem = 256 - imin(256, t * 256 / d);
        f->ox = (int8_t)(((rem * rem) >> 8) * 100 / 256);
        if(s->an_side == 0) f->ox = (int8_t)-f->ox;
        break;
    }
    case AN_LEGEND:
        f->flat = 4;
        break; // (the fill-in is drawn by draw_fighter from an_t)
    case AN_SENDOUT: {
        // white flash, then grow in steps
        if(t < 70)
            f->sc_n = 0, f->blink = 2;
        else if(t < 130)
            f->sc_n = 3, f->blink = 0;
        else if(t < 190)
            f->sc_n = 5;
        else if(t < 250)
            f->sc_n = 7;
        else
            f->sc_n = 0;
        break;
    }
    case AN_ATTACK: {
        Fighter* tg = &s->f[s->an_side ^ 1];
        uint8_t mode = s->an_mode & 3, kind = s->an_fx, st = s->an_style;
        int big = (st & MST_BIG) != 0,
            tdir = s->an_side == 0 ? 1 : -1; // tdir: +1 = the target is to the right
        unsigned d = fx_duration(kind), f0 = attack_fx0(st);
        if(s->an_mode & AM_CAST) { // a status move: the user hops
            int tt = imin(t, 340);
            f->ox = 0;
            f->oy = (int8_t)(-(6 * 4 * tt * (340 - tt)) / (340 * 340));
        } else {
            int reach = (st & MST_CONTACT) ? (big ? 22 : 15) : 7, k;
            if(t < AT_WIND)
                k = -4 * t / AT_WIND;
            else if(t < AT_PEAK) {
                int q = 256 - (t - AT_WIND) * 256 / (AT_PEAK - AT_WIND);
                k = -4 + (reach + 4) * (256 - q * q / 256) / 256;
            } else if(t < AT_END) {
                int q = 256 - (t - AT_PEAK) * 256 / (AT_END - AT_PEAK);
                k = reach * q * q / 65536;
            } else
                k = 0;
            f->ox = (int8_t)(tdir * k);
            f->oy = (int8_t)(-tdir * k * 2 / 5);
        }
        // the effect on the target; a miss fizzles earlier, beside the target, in a puff
        s->fx_kind = 0;
        if(t >= (int)f0) {
            unsigned ft = (unsigned)t - f0, cut = d * AT_WHIFF_PCT / 100;
            s->fx_side = s->an_side ^ 1;
            s->fx_big = (uint8_t)big;
            s->fx_miss = mode == AM_MISS;
            if(mode == AM_MISS && ft >= cut) {
                s->fx_kind = FX_WHIFF;
                s->fx_t = (uint16_t)(ft - cut);
            } else if(ft < d) {
                s->fx_kind = kind;
                s->fx_t = (uint16_t)ft;
            }
        }
        // the hit: whole-world shake and three blinks of the target
        s->shx = s->shy = 0;
        tg->blink = 0;
        if(mode == AM_HIT && t >= (int)attack_impact_ms(st)) {
            int n = t - (int)attack_impact_ms(st);
            if(n < 40)
                sound_play(
                    SND_FX_HIT); // the hit (n < 40: a frame or two; a playing effect is not restarted; contract 28)
            static const int8_t AMP[5] = {0, 2, 3, 5, 6};
            if(n < AT_IMPACT_MS) tg->blink = ((n / AT_BLINK_MS) & 1) ? (s->an_pw == 4 ? 1 : 2) : 0;
            if(n < AT_SHAKE_MS) {
                int step = n / 40, a = AMP[s->an_pw] * (7 - step) / 7 + (big ? 1 : 0);
                s->shx = (int8_t)((step & 1) ? a : -a);
                if(s->an_pw >= 3) s->shy = (int8_t)(((step >> 1) & 1) ? a / 2 : -(a / 2));
            }
        }
        if(kind == FX_ROCKS && big && t >= (int)f0 && t < (int)(f0 + d * 8 / 10) &&
           !s->shx) // an earthquake
            s->shx = (int8_t)(((t / 45) & 1) ? 2 : -2);
        break;
    }
    case AN_PUMP: {
        static const int8_t hop[] = {0, -2, -4, -5, -4, -2, 0, -2, -3, -2, 0};
        int idx = t * (int)(sizeof(hop) - 1) / d;
        f->oy = hop[imin(idx, (int)sizeof(hop) - 1)];
        break;
    }
    case AN_BLINK: { // the creature disappears and reappears three times
        int k = t / AT_BLINK_MS;
        f->blink = (k & 1) && k < 6 ? 2 : 0;
        break;
    }
    case AN_FAINT: {
        int h = f->med.b.h ? f->med.b.h : 30;
        f->sink = (int16_t)q16(h + 2, t, d);
        break;
    }
    case AN_TR_IN: {
        int rem = 256 - imin(256, t * 256 / d);
        int off = ((rem * rem) >> 8) * 70 / 256;
        s->tr_dx[0] = (int16_t)off;
        s->tr_dx[1] = (int16_t)-off;
        break;
    }
    case AN_TR_OUT: {
        int p = imin(256, t * 256 / d);
        int off = ((p * p) >> 8) * 80 / 256;
        s->tr_dx[0] = (int16_t)off;
        s->tr_dx[1] = (int16_t)-off;
        break;
    }
    case AN_WIN_IN: {
        int rem = 256 - imin(256, t * 256 / d);
        s->tr_dx[0] = (int16_t)(((rem * rem) >> 8) * 70 / 256);
        break;
    }
    case AN_THROW: { // a parabola from just above the player's creature to the enemy
        int u = imin(256, t * 256 / d);
        s->cry_x = (int16_t)(PL_LEFT + 22 + (s->cry_px - PL_LEFT - 22) * u / 256);
        s->cry_y = (int16_t)(PL_BOT - 14 + (s->cry_py - PL_BOT + 14) * u / 256 -
                             22 * 4 * u * (256 - u) / 65536);
        break;
    }
    case AN_ABSORB: { // white silhouette shrinking into the crystal
        if(t < 70)
            f->flat = 3;
        else if(t < 330) {
            f->flat = 3;
            f->sc_n = (uint8_t)(t < 130 ? 6 : (t < 190 ? 4 : (t < 250 ? 2 : 1)));
            // the scale is about the picture's bottom centre: slide that point up to the crystal as it shrinks
            int h = f->med.b.h ? f->med.b.h : 30;
            f->oy = (int8_t)((s->cry_py - CRYSTAL_H / 2 - (f->y + h)) * (8 - f->sc_n) / 8);
        } else {
            f->blink = 2;
        }
        break;
    }
    case AN_DROP: {
        int u = imin(256, t * 256 / d);
        if(u < 180) {
            int v = u * 256 / 180;
            s->cry_x = (int16_t)(s->cry_px + (s->cry_gx - s->cry_px) * v / 256);
            s->cry_y = (int16_t)(s->cry_py + (s->cry_gy - s->cry_py) * (v * v >> 8) / 256);
        } else { // one little bounce
            int v = (u - 180) * 256 / 76;
            s->cry_x = s->cry_gx;
            s->cry_y = (int16_t)(s->cry_gy - 4 * 3 * v * (256 - v) / 65536);
        }
        break;
    }
    case AN_WOBBLE: { // left, right, centre, then a pause: one "tick" per wobble
        int ph = t % CRY_SHAKE_MS;
        s->cry_tilt = (int8_t)(ph < 110 ? -3 : (ph < 220 ? 3 : (ph < 300 ? -1 : 0)));
        break;
    }
    case AN_CLICK:
        s->cry_dim = t >= 110;
        break;
    case AN_BURST:
        if(t >= 100) s->cry_on = false;
        break;
    default:
        break;
    }
}

static void anim_end(S* s) {
    Fighter* f = &s->f[s->an_side];
    switch(s->an_kind) {
    case AN_TR_OUT:
        s->tr_vis[0] = s->tr_vis[1] = false;
        break;
    case AN_TR_IN:
        s->tr_dx[0] = s->tr_dx[1] = 0;
        break;
    case AN_WIN_IN:
        s->tr_dx[0] = 0;
        break;
    case AN_FAINT:
        f->visible = false;
        f->sink = 0;
        break;
    default:
        break;
    }
    if(s->an_kind != AN_TR_IN && s->an_kind != AN_TR_OUT && s->an_kind != AN_WIN_IN) {
        f->ox = f->oy = 0;
        f->blink = s->an_kind == AN_ABSORB ?
                       2 :
                       0; // the absorbed enemy stays hidden until it breaks free
        f->flat = 0;
        f->sc_n = 0;
    }
    if(s->an_kind == AN_WOBBLE) s->cry_tilt = 0;
    if(s->an_kind == AN_ATTACK) {
        Fighter* tg = &s->f[s->an_side ^ 1];
        tg->blink = 0;
        s->shx = s->shy = 0;
    }
    s->fx_kind = 0;
    s->an_kind = 0;
}

static void exit_start(SceneCtx* c, S* s);
static void load_label(uint16_t id, tchar* dst, int cap);

// Executes the instantaneous part of an op. Returns true when the op needs ticks (stays current).
static __attribute__((noinline)) bool op_begin(SceneCtx* c, S* s, const Op* o) {
    switch(o->op) {
    case BOP_CUT:
        if(o->a == V_FOCUS) {
            Fighter* f = &s->f[o->b & 1];
            monpic_free(&s->f[(o->b & 1) ^ 1].nat); // one native picture at a time (heap)
            for(int k = 0; k < 2; k++)
                plat_free(
                    &s->f[k]
                         .plat); // contract 82: a focus shot draws the focus platform only: the two of the normal view (about 600 B) give way and are built again when it is over (the heap peak of the battle was the focus shot)
            plat_free(&s->xplat);
            plat_build(&s->xplat, FOC_PLAT_RX, FOC_PLAT_RY, 2);
            if(!fighter_load_native(s, f, o->b & 1)) { // no picture: stay in the normal view
                plat_free(&s->xplat);
                plats_back(s);
                return false;
            }
        } else if(o->a == V_NORMAL || o->a == V_INTRO) {
            // drop the focus pictures / platform (heap): they are loaded again on the next focus shot (the trainers' platform of the intro is the same block: it stays for V_INTRO)
            for(int k = 0; k < 2; k++)
                monpic_free(&s->f[k].nat);
            if(o->a == V_NORMAL) plat_free(&s->xplat);
            plats_back(s);
        }
        s->view = (View)o->a;
        s->fside = o->b & 1;
        scene_dirty(c, D_WORLD);
        return false;
    case BOP_MSG:
#ifndef NUCLEO_DEVICE
        if(o->a & MSF_USED) s->uses++;
#endif
        if(o->a &
           MSF_ASK) { // the YES / NO box is the party list's two-option box: its labels (party_open loads them again) and cursor
            s->pm_opt = 0;
            load_label(TXT_GL_CHOICE_YES, s->l_switch, LBL_CAP);
            load_label(TXT_GL_CHOICE_NO, s->l_cancel, LBL_CAP);
            const tchar* yn[2] = {s->l_switch, s->l_cancel};
            text_prefetch_many(yn, 2);
        }
        if(!msg_begin(s, o)) return false;
        scene_dirty(c, D_COMPOSE);
        return true;
    case BOP_WAIT:
        s->cur_t = 0;
        return true;
    case BOP_ANIM:
        if((o->a == AN_SLIDE || o->a == AN_SENDOUT) && !s->ghost && s->f[o->b & 1].species)
            sound_play((
                uint16_t)(SND_CRY +
                          s->f[o->b & 1].species)); // contract 75: the creature's cry as it appears
        if((g_settings.battle & SET_BATTLE_ANIM_OFF) &&
           ((1u << o->a) &
            ((1u << AN_SENDOUT) | (1u << AN_ATTACK) | (1u << AN_PUMP) | (1u << AN_FX) |
             (1u << AN_BLINK)))) { // contract 75: battle animations Off
            if(o->a ==
               AN_SENDOUT) { // (the creature just appears: what the animation would have set)
                Fighter* f = &s->f[o->b & 1];
                f->visible = true;
                f->blink = f->flat = f->sc_n = 0;
            }
            s->cur.op =
                BOP_WAITHP; // the effect is skipped; the HP bar still drains before the next line
            scene_dirty(c, D_WORLD);
            return true;
        }
        anim_start(s, o);
        scene_dirty(c, D_WORLD);
        return true;
    case BOP_SET: {
        Fighter* f = &s->f[o->b & 1];
        switch(o->a) {
        case SET_FIGHTER: {
            const Mon* m = &s->b->party[o->b & 1]->mon[o->p[0] & 0xFF];
            fighter_set(
                s,
                o->b & 1,
                m->species,
                (uint8_t)(o->p[0] >> 8),
                m->status,
                o->p[1],
                m->max_hp,
                bc_shiny(m) && !s->ghost,
                0);
            break;
        }
        case SET_LEVEL:
            f->level = (uint8_t)o->p[0];
            break;
        case SET_STATUS:
            if(o->p[0] == 0xFF)
                f->status = f->dug =
                    0; // (a cure also ends a charge that was broken off by sleep / ice: contract 85)
            else if(o->p[0] <= SK_PARALYSIS)
                f->status = (uint8_t[]){
                    0,
                    1,
                    BS_PSN,
                    BS_BRN,
                    BS_FRZ,
                    BS_PAR}[o->p[0]]; // (the tag of SK_* 1 .. 5; a confusion has none)
            break;
        case SET_SYNC: {
            const Mon* m = &s->b->party[0]->mon[o->p[0] & 0xFF];
            f->max_hp = (int16_t)m->max_hp;
            f->hp = f->hp_to = (int16_t)m->hp;
            break;
        }
        case SET_HIDE:
            f->visible = false;
            f->hp = f->hp_to = 0;
            break;
        case SET_DUG:
            f->dug = (uint8_t)o->p[0];
            break;
        case SET_GONE: // caught: picture and plate leave (the HP stay)
            f->visible = false;
            break;
        case SET_FORM: // contract 67: the Eclipse Form's pictures (behind the black of the animation)
            fighter_set(
                s,
                o->b & 1,
                o->p[1] ? (uint8_t)o->p[1] : f->species,
                f->level,
                f->status,
                f->hp,
                f->max_hp,
                f->shiny && !o->p[1],
                (uint8_t)o
                    ->p[0]); // the same creature, its form's pictures (and platform); contract 73: p[1] = the species of Transform's target
            f->visible = true;
            fighter_load_native(s, f, o->b & 1);
            break;
        case SET_INTRO_DONE: // the trainers are gone: free their picture and platform (reloaded for the win scene)
            s->intro_done = true;
            monpic_free(&s->rival);
            plat_free(&s->xplat);
            render_cache_release(); // the hero's back view (1.6 KB cache, read from the card when he slid in): read again if the win scene shows him
            break;
        case SET_RIVAL:
            if(!s->rival.blk) load_trainer_pic(s);
            if(!s->xplat.blk) plat_build(&s->xplat, TR_PLAT_RX, TR_PLAT_RY, 2);
            break;
        }
        scene_dirty(
            c,
            D_WORLD |
                D_COMPOSE); // (contract 85: one redraw for every state change: the ops are instantaneous, and 7 calls were ~40 B of the plugin)
        return false;
    }
    case BOP_HP: {
        Fighter* f = &s->f[o->a & 1];
        f->hp_to = (int16_t)o->p[0];
        f->drain_acc = 0;
        f->drain_delay = (g_settings.battle & SET_BATTLE_ANIM_OFF) ?
                             0 :
                             o->p[1]; // (Off: no hit to wait for, contract 75)
        { // the bar takes longer for a bigger fall: 500 ms + 18 ms per pixel, at most 1 s
            int diff = f->hp > f->hp_to ? f->hp - f->hp_to : f->hp_to - f->hp;
            int mx = f->max_hp > 0 ? f->max_hp : 1, step = mx / BAR_W < 1 ? 1 : mx / BAR_W;
            int ticks = (diff + step - 1) / step, px = diff * BAR_W / mx;
            int total = DRAIN_MIN_MS + DRAIN_PX_MS * (px < 1 ? 1 : px);
            if(total > DRAIN_MAX_MS) total = DRAIN_MAX_MS;
            f->drain_ms = (uint16_t)(total / (ticks < 1 ? 1 : ticks));
            if(f->drain_ms < 1) f->drain_ms = 1;
        }
        return false;
    }
    case BOP_WAITHP:
        return true;
    case BOP_END:
        exit_start(c, s);
        return false;
    default:
        return false;
    }
}

// Ticks the current op. Returns true when it is finished.
static __attribute__((noinline)) bool op_tick(SceneCtx* c, S* s, uint32_t dt) {
    Op* o = &s->cur;
    switch(o->op) {
    case BOP_MSG:
        return msg_tick(c, s, dt);
    case BOP_WAIT:
        s->cur_t = (uint16_t)imin(65535, s->cur_t + (int)dt);
        return s->cur_t >= o->p[0];
    case BOP_ANIM: {
        const uint16_t t_was = s->an_t;
        s->an_t = (uint16_t)imin(65535, s->an_t + (int)imin((int)dt, 60));
        if(s->an_kind == AN_LEGEND && t_was < LG_CRY_MS && s->an_t >= LG_CRY_MS &&
           s->an_t < s->an_dur)
            sound_play(
                SND_FX_LEGEND_CRY); // the silhouette's cry, once (an OK that skips the rest never gets here)
        if(s->an_kind == AN_SENDOUT || s->an_kind == AN_ABSORB || s->an_kind == AN_LEGEND ||
           s->an_kind == AN_ECLIPSE) { // white flash
            uint8_t fl = s->an_kind == AN_LEGEND ?
                             (s->an_t >= LG_BLACK_MS && s->an_t < LG_WHITE_MS ? 3 : 0) :
                             (s->an_t < (s->an_kind == AN_ECLIPSE ? 90 * s->an_fx : 70) ?
                                  3 :
                                  0); // (the Eclipse flash opens its second part only)
            if(c->fade != fl) {
                c->fade = fl;
                scene_dirty(c, D_COMPOSE);
            }
        }
        if(s->an_kind == AN_FX || s->an_kind == AN_CLICK || s->an_kind == AN_BURST)
            s->fx_t = s->an_t;
        if(s->an_t >= s->an_dur) {
            anim_apply(s);
            anim_end(s);
            c->fade = 0;
            scene_dirty(c, D_WORLD);
            return true;
        }
        anim_apply(s);
        scene_dirty(c, D_WORLD);
        return false;
    }
    case BOP_WAITHP:
        return s->f[0].hp == s->f[0].hp_to && s->f[1].hp == s->f[1].hp_to;
    default:
        return true;
    }
}

static void menu_enter(SceneCtx* c, S* s);
static void party_open(SceneCtx* c, S* s, uint8_t purpose);

// Called when the queue is empty: next event, forced switch, or hand control to the player.
static bool do_choose(SceneCtx* c, S* s, BcAction act);
static void start_turn(SceneCtx* c, S* s);
static void idle_next(SceneCtx* c, S* s) {
    Battle* b = s->b;
    if(s->ev_i < s->ev_n) {
        translate(s, s->ev_i++);
        return;
    }
    if(b->phase == BP_OVER) { // events consumed but no END op queued (cannot happen); leave anyway
        q_op(s, BOP_END);
        return;
    }
    if(s->f[0].hp != s->f[0].hp_to || s->f[1].hp != s->f[1].hp_to) {
        q_op(s, BOP_WAITHP);
        return;
    }
    if(s->tview != V_NORMAL) {
        q_cut(s, V_NORMAL, 0);
        return;
    }
    if(b->phase == BP_SWITCH || s->auto_switch) {
        s->auto_switch = false;
        if(pick_switch(s) >= 0) { // the player CHOOSES who goes next
            party_open(c, s, PP_FORCED);
            return;
        }
        // no healthy creature (the core should have ended the battle): finish as a loss
        q_op(s, BOP_END);
        return;
    }
    if(bc_forced_go(
           b)) { // contract 79: Rage, Rampage, Hold Fast, a binding move, Fly / Dig's second turn and the recharge turn have no menu: the turn runs by itself
        start_turn(c, s);
        return;
    }
    menu_enter(c, s);
}

static void run_ops(SceneCtx* c, S* s, uint32_t dt) {
    for(int guard = 0; guard < 64; guard++) {
        if(s->mode != M_RUN) return;
        if(!s->cur_active) {
            if(s->qi >= s->qn) {
                s->qi = s->qn = 0;
                idle_next(c, s);
                if(s->mode != M_RUN) return;
                if(s->qn == 0) continue;
            }
            s->cur = s->q[s->qi++];
            s->cur_active = op_begin(c, s, &s->cur);
            if(s->mode != M_RUN) return;
            if(!s->cur_active) continue;
            dt = 0; // the op was just begun: no time passes in its first tick
        }
        if(op_tick(c, s, dt)) {
            s->cur_active = false;
            continue;
        }
        return;
    }
}

// ------------------------------------------------------------------------------------------------ menus
static void load_label(uint16_t id, tchar* dst, int cap) {
    dst[0] = 0;
    if(id != 0xFFFF) text_load(id, dst, cap);
}

static void menu_enter(SceneCtx* c, S* s) {
    s->lrn_mv =
        0; // (no forget-a-move dialog can be open here: a stale one must never turn the move list into its forget list)
    if(s->after == AFTER_PARTY) { // a one-off message from the party list: back to it
        party_open(c, s, s->pp);
        return;
    }
    s->after = AFTER_MENU;
    if(s->reserve)
        text_set_var_num(
            2,
            bag_count(
                IT_CRISTAL_RESERVA)); // "Crystal x{v2}": the crystals left (Reserva Salvaje, contract 43)
    // the labels (the active language; they share their block with the move list / bag / party names, so they are loaded each time) and, for the SD-backed fonts (zh), their glyphs
    static const uint16_t LBL[2][4] = {
        {TXT_GL_BT_FIGHT, TXT_GL_BT_BAG, TXT_GL_BT_PARTY, TXT_GL_BT_RUN},
        {TXT_GL_C5B_SF_BALL, TXT_GL_C5B_SF_ROCK, TXT_GL_C5B_SF_BAIT, TXT_GL_BT_RUN}};
    const tchar* all[4];
    for(int k = 0; k < 4; k++) {
        load_label(LBL[s->reserve][k], s->lbl[k], LBL_CAP);
        all[k] = s->lbl[k];
    }
    text_prefetch_many(all, 4);
    s->mode = M_MENU;
    const Mon* lm = &s->b->party[0]->mon[s->b->side[0].active];
    if(lm->hp && lm->hp * 5 <= lm->max_hp)
        sound_play(
            SND_FX_LOWHP); // the low-HP warning: two soft beeps each time the menu shows (contract 28)
    scene_dirty(c, D_WORLD | D_COMPOSE);
}

// ---- the move list (contract 72). Two columns (shows a column; here the screen is 128 px) when the longest name of each column fits next to the other: the box is the cursor column + name + gap + cursor column + name, drawn flush
// right; when it does not (67 % of the default es movesets, 51 % en: names are up to 12 glyphs = 72 px) the moves are ONE column of FIGHT_ROWS rows that scrolls like the bag's box (the same right-aligned box and arrows).
// Why 3 rows and not 4: the type / PP box (and the Eclipse chip) stands on the list, so 4 rows would be 48 + 13 px of boxes before the player's plate, which in es / en / zh already leaves nothing of the 64 px
// (and a taller stack hides the whole foe, whose picture is the first thing above the boxes). The player's plate is NOT drawn while the one-column list is open (its HP was on screen in the menu one press ago): that is what lets
// the foe's head stay visible above the stack (it ends at y = 14 in es / en, 18 in ja, 10 in zh).
#define FIGHT_ROWS 3
#if BF_MOVE_COPY
#define S_CPY(s) ((s)->cpy)
#else
#define S_CPY(s) 0
#endif
// contract 73: the move in slot k of the list on screen: the creature's own as the battle has them (a Echoing copy or Transform's four included), the FOE's while Echoing is choosing (s->cpy), or the real ones in the forget-a-move list
static uint8_t fmv(const S* s, int k) {
    return s->lrn_mv ? s->b->party[0]->mon[s->b->side[0].active].moves[k] :
                       bc_mv(s->b, S_CPY(s), k);
}
static void fight_info_load(S* s) {
    s->tname[0] = 0;
    uint8_t mid = fmv(s, s->mv_sel);
    const MoveDef* mv = mid ? bc_move_def(s->b, mid) : NULL;
    if(mv && mv->type < TY_COUNT) load_label(reg_ty_txt(mv->type), s->tname, NAME_CAP);
    const tchar* one[1] = {s->tname};
    text_prefetch_many(one, 1);
}

static void fight_open(SceneCtx* c, S* s) {
    const tchar* all[4];
    for(int i = 0; i < 4; i++) {
        s->mvn[i][0] = 0;
        if(fmv(s, i)) load_label(mv_txt(fmv(s, i)), s->mvn[i], NAME_CAP);
        all[i] = s->mvn[i];
    }
    text_prefetch_many(all, 4);
    if(!fmv(s, s->mv_sel)) s->mv_sel = 0;
    s->ecl_sel = 0;
    // the layout, decided once per opening (the names are loaded): the widest name of each column, and the box that holds them
    int w[2] = {0, 0}, n = 0;
    for(int k = 0; k < 4; k++) {
        n += s->mvn[k][0] && n == k; // (a creature holds its moves from slot 0 on, without a hole)
        w[k & 1] = imax(w[k & 1], font_text_width(s->mvn[k]));
    }
    const bool vert = 3 + (font_cell_w() + 2) * 2 + w[0] + 2 + w[1] + 4 > SCREEN_W;
    s->mv_wl = (uint8_t)w[0];
    s->mv_wr = (uint8_t)w[1];
    s->mv_n = (uint8_t)n;
    s->mv_vert = vert;
    s->mv_rows = vert ? (uint8_t)imin(n, FIGHT_ROWS) : 2;
    s->mv_top = vert && s->mv_sel >= FIGHT_ROWS ? (uint8_t)(s->mv_sel - FIGHT_ROWS + 1) : 0;
    fight_info_load(s);
    s->mode = M_FIGHT;
    scene_dirty(c, D_COMPOSE);
}

// The action is stored: resolve the turn and hand the events to the runner.
static void start_turn(SceneCtx* c, S* s) {
    bc_step(s->b);
    load_events(s);
    s->mode = M_RUN;
    s->cur_active = false;
    s->qi = s->qn = 0;
    scene_dirty(c, D_WORLD | D_COMPOSE);
    scene_wake(c, 1);
}

static void say(SceneCtx* c, S* s, uint16_t txt, uint16_t v0, uint8_t after);
static __attribute__((always_inline)) inline bool do_choose(SceneCtx* c, S* s, BcAction act) {
    if(s->ghost &&
       act.kind !=
           ACT_RUN) { // contract 36: against an unidentified ghost nothing but Run reaches the core (no turn passes, nothing is used)
        say(c,
            s,
            act.kind == ACT_MOVE ? TXT_GL_C4B_GHOST_SCARED : TXT_GL_C4B_GHOST_FROZEN,
            0,
            act.kind == ACT_SWITCH ? AFTER_PARTY : AFTER_MENU);
        return false;
    }
    if(!bc_choose(s->b, act)) return false;
    start_turn(c, s);
    return true;
}

// A one-off message (cannot run, no effect ...) and back to the menu (`after` = AFTER_PARTY: to the party list).
static void run_ops_from(
    SceneCtx* c,
    S* s,
    uint8_t after) { // an empty queue to fill, and the runner to hand it to (say, the Eclipse Form)
    s->mode = M_RUN;
    s->cur_active = false;
    s->qi = s->qn = 0;
    s->ev_i = s->ev_n = 0;
    s->after = after;
    scene_dirty(c, D_WORLD | D_COMPOSE);
    scene_wake(c, 1);
}
static void say(SceneCtx* c, S* s, uint16_t txt, uint16_t v0, uint8_t after) {
    run_ops_from(c, s, after);
    q_m1(s, txt, v0, 0);
}

// ---- the Eclipse Form (contract 67). The chip (the 7 x 7 mark in the info box above the move list) shows while the core allows the form (starter final, the player's own, not used yet) and the Eclipse Stone is in the bag; UP
// from the top row of moves puts the cursor on it, OK takes the form (DOWN goes back to the moves, Back leaves the list as always). No turn passes: the animation plays and the main menu comes back.
static bool ecl_ready(const S* s) {
    return !s->ghost && !s->lrn_mv && bc_ecl_can(s->b) && bag_count(IT_PIEDRA_ECLIPSE);
}

static void eclipse_go(SceneCtx* c, S* s) {
    const uint8_t form = bc_ecl_go(s->b);
    if(!form) return;
    s->ecl_sel = 0;
    run_ops_from(c, s, AFTER_MENU);
    q_cut(s, V_FOCUS, 0);
    q_anim(s, AN_ECLIPSE, 0, 0);
    q_set(s, SET_FORM, 0, form);
    q_anim(s, AN_ECLIPSE, 0, 1);
    q_cut(
        s,
        V_NORMAL,
        0); // (the sparkle plays in the normal view: the effect's frames and the focus shot's native picture and platform must not be alive together)
    q_anim(s, AN_FX, 0, FX_STARS3);
    q_m1(s, TXT_GL_B3_ECL_DONE, nick_ref(s->tslot, s->tsp[0]), MSF_OK);
}

// ---- forget a move (contract 23). The core left the Mon untouched (BC_LEARN_FULL); the queue ended with the MSF_ASK message. YES -> the move list of the
// learner (the fight box again: M_FIGHT with lrn_mv set), OK on a move forgets it, Back asks again; NO -> "stop learning?" (YES: it is not learned, NO: back).
static void learn_go(SceneCtx* c, S* s) { // back to the runner with what was just queued
    s->mode = M_RUN;
    s->cur_active = false;
    scene_dirty(c, D_WORLD | D_COMPOSE);
    scene_wake(c, 1);
}
static void learn_ask_go(SceneCtx* c, S* s, uint8_t stop) {
    s->qi = s->qn = 0;
    learn_ask(s, stop);
    learn_go(c, s);
}
static void learn_answer(SceneCtx* c, S* s) { // OK on the YES / NO (Back = NO)
    if(s->cur.a &
       MSF_SHIFT) { // contract 75: "change creatures?": YES opens the party list (the free swap), NO goes on to the trainer's next creature
        s->qi = s->qn = 0;
        s->cur_active = false;
        if(s->pm_opt)
            learn_go(c, s);
        else
            party_open(c, s, PP_SHIFT);
        return;
    }
    uint8_t stop = s->cur.a & MSF_STOP;
    if(s->pm_opt) {
        learn_ask_go(c, s, !stop);
        return;
    }
    s->qi = s->qn = 0;
    s->cur_active = false;
    if(!stop) {
        fight_open(c, s);
        return;
    }
    q_learn(s, TXT_GL_LEARN_NOT, s->lrn_mv, 0);
    s->lrn_mv = 0;
    learn_go(c, s);
}
static void learn_forget(SceneCtx* c, S* s, uint8_t k) { // OK on move k of the list
    Mon* m = &s->b->party[0]->mon[s->b->side[0].active];
    uint8_t old = m->moves[k], nw = s->lrn_mv;
    m->moves[k] = nw;
    m->pp[k] = bc_move_def(s->b, nw)->pp; // full PP
    s->lrn_mv = 0;
    s->qi = s->qn = 0;
    q_learn(s, TXT_GL_LEARN_POOF, 0, 0);
    q_learn(s, TXT_GL_LEARN_FORGOT, old, 0);
    q_learn(s, TXT_GL_LEARN_AND, nw, 0);
    learn_go(c, s);
}

// ---- bag (MOCHILA): a box of up to BAG_ROWS items with their counts; the names of the visible rows are loaded
static void bag_names(S* s) {
    const tchar* all[BAG_ROWS];
    for(int i = 0; i < BAG_ROWS; i++) {
        uint8_t it = 0, n = 0;
        s->itn[i][0] = 0;
        if(bag_get((uint8_t)(s->bg_top + i), &it, &n)) load_label(it_txt(it), s->itn[i], NAME_CAP);
        all[i] = s->itn[i];
    }
    text_prefetch_many(all, BAG_ROWS);
}

static __attribute__((always_inline)) inline void bag_open(SceneCtx* c, S* s) {
    s->after = AFTER_MENU;
    s->bg_n = bag_kinds();
    if(s->bg_sel >= s->bg_n) s->bg_sel = 0;
    if(s->bg_top > s->bg_sel) s->bg_top = s->bg_sel;
    if(s->bg_sel >= s->bg_top + BAG_ROWS) s->bg_top = (uint8_t)(s->bg_sel - BAG_ROWS + 1);
    bag_names(s);
    s->mode = M_BAG;
    scene_dirty(c, D_WORLD | D_COMPOSE);
}

static void bag_pick(SceneCtx* c, S* s) {
    uint8_t it = 0, n = 0;
    if(!bag_get(s->bg_sel, &it, &n) || it >= s->b->data->n_items) return;
    uint8_t kind = s->b->data->items[it].kind;
    if(it ==
       IT_PELUCHE_FUGA) { // contract 74 (the doll role): the escape is certain (the Run formula with the attempts maxed, like the ghost's and the Reserva's) and it works on the unidentified ghost too; a trainer says no
        if(s->trainer)
            say(c, s, TXT_GL_BT_CANT_RUN, 0, AFTER_MENU);
        else if(bag_take(it)) {
            s->b->side[0].run_attempts = 255;
            do_choose(c, s, (BcAction){ACT_RUN, 0, 0});
        }
        return;
    }
    if(s->ghost) { // a crystal cannot be thrown at a ghost; anything else is "too scared"
        say(c,
            s,
            kind == IK_CATCH ? TXT_GL_C4B_GHOST_NOCATCH : TXT_GL_C4B_GHOST_FROZEN,
            0,
            AFTER_MENU);
        return;
    }
    if(BF_FLUTE && it == IT_FLAUTA) {
        do_choose(
            c,
            s,
            (BcAction){
                ACT_ITEM,
                it,
                0}); // contract 82 (the flute item): takes the turn, the Flute stays in the bag
    } else if(it == IT_CRISTAL_RESERVA) {
        say(c,
            s,
            TXT_GL_BAG_NOT_NOW,
            0,
            AFTER_MENU); // the Reserve Crystal is thrown in the Reserva Salvaje only (contract 43)
    } else if(
        kind == IK_CATCH ||
        (BF_ITEMS &&
         kind ==
             IK_BATTLE)) { // (contract 73: the boost items, Mist Veil and Crit Focus are used on the creature in play, like a crystal: they take the turn)
        if(s->trainer && kind == IK_CATCH)
            say(c, s, TXT_GL_BT_NO_STEAL, 0, AFTER_MENU); // nothing is lost, the turn is kept
        else if(do_choose(c, s, (BcAction){ACT_ITEM, it, 0}))
            bag_take(it);
    } else if(kind == IK_HEAL || kind == IK_CURE || kind == IK_REVIVE || kind == IK_RESTORE) {
        s->item = it;
        party_open(c, s, PP_POTION);
    } else {
        say(c,
            s,
            it == IT_PIEDRA_ECLIPSE ? TXT_GL_B3_ECL_STONE : TXT_GL_BAG_NOT_NOW,
            0,
            AFTER_MENU); // a key item; the Eclipse Stone says how it is used (contract 67)
    }
}

// ---- party list (EQUIPO / the salve's target / the forced switch)
static void party_open(SceneCtx* c, S* s, uint8_t purpose) {
    const Party* p = s->b->party[0];
    s->after = AFTER_MENU;
    if(purpose == PP_FORCED && s->b->phase != BP_SWITCH)
        purpose = PP_SWITCH; // (a stale request must never trap the player)
    const tchar* all[BC_MAX_PARTY + 2];
    int n = 0;
    for(int k = 0; k < p->count; k++) {
        s->pname[k][0] = 0;
        party_mon_name((uint8_t)k, s->pname[k], NAME_CAP);
        all[n++] = s->pname[k];
    }
    load_label(TXT_GL_BT_SWITCH, s->l_switch, LBL_CAP);
    load_label(TXT_GL_PARTY_CANCEL, s->l_cancel, LBL_CAP);
    all[n++] = s->l_switch;
    all[n++] = s->l_cancel;
    text_prefetch_many(all, n);
    s->pp = purpose;
    s->pm_box = false;
    s->pm_opt = 0;
    int sel = purpose >= PP_FORCED ? pick_switch(s) : s->b->side[0].active;
    s->pm_sel = (uint8_t)(sel < 0 ? 0 : sel);
    s->pm_top = 0;
    s->mode = M_PARTY;
    scene_dirty(c, D_WORLD | D_COMPOSE);
}

static int pm_rows(void) { // list rows that fit the screen (3 in every language)
    int v = (SCREEN_H - 2) / (pitch() + 8);
    return v < 1 ? 1 : v;
}

static void pm_move(SceneCtx* c, S* s, int dir) {
    int n = s->b->party[0]->count;
    s->pm_sel = (uint8_t)((s->pm_sel + (dir > 0 ? 1 : n - 1)) % n);
    int vis = pm_rows();
    if(s->pm_sel < s->pm_top) s->pm_top = s->pm_sel;
    if(s->pm_sel >= s->pm_top + vis) s->pm_top = (uint8_t)(s->pm_sel - vis + 1);
    scene_dirty(c, D_COMPOSE);
}

static void pm_pick(SceneCtx* c, S* s) {
    const Mon* m = &s->b->party[0]->mon[s->pm_sel];
    uint16_t nm = nick_ref(s->pm_sel, m->species);
    if(s->pp == PP_POTION) {
        if(bc_choose(s->b, (BcAction){ACT_ITEM, s->item, s->pm_sel})) {
            bag_take(s->item);
            start_turn(c, s);
        } else {
            say(c, s, TXT_GL_BT_NO_EFFECT, 0, AFTER_PARTY);
        }
    } else if(!m->hp) {
        say(c, s, TXT_GL_BT_NO_WILL, nm, AFTER_PARTY);
    } else if(s->pm_sel == s->b->side[0].active) {
        say(c, s, TXT_GL_BT_ALREADY_OUT, nm, AFTER_PARTY);
    } else if(
        s->pp ==
        PP_SHIFT) { // contract 75: the free swap; its send-out event goes in front of the trainer's (parked one step back by translate), no turn passes
        Battle* b = s->b;
        const int k = s->ev_i; // (translate left it at the trainer's EV_SENDOUT)
        if(bc_shift(b, s->pm_sel)) {
            const BcEvent e = b->ev[b->n_ev - 1];
            memmove(&b->ev[k + 1], &b->ev[k], sizeof(BcEvent) * (size_t)(b->n_ev - 1 - k));
            b->ev[k] = e;
            s->ev_n = b->n_ev;
        }
        learn_go(c, s);
    } else {
        do_choose(c, s, (BcAction){ACT_SWITCH, s->pm_sel, 0});
    }
}

// ------------------------------------------------------------------------------------------------ drawing: world part
static void inv_all(LvlBuf lvl) {
    lvl_invert(lvl);
}

static void draw_fighter(S* s, LvlBuf lvl, int side, bool focus) {
    Fighter* f = &s->f[side];
    if(!f->visible || f->blink == 2 || f->dug) return;
    const MonPic* p = focus ? &f->nat : &f->med;
    if(!p->blk) return;
    int x, y, w, h;
    fighter_pos(s, f, focus, &x, &y, &w, &h);
    BlitFx fx = {0};
    fx.invert = f->blink == 1;
    fx.flat = side == 1 && s->ghost ?
                  4 :
                  f->flat; // an unidentified ghost: its own picture, all black (level 3)
    if(s->an_kind == AN_LEGEND && side == 1 && s->an_t >= LG_FILL_MS)
        fx.real_y = (int16_t)(y + h * (s->an_t - LG_FILL_MS) /
                                      LG_FILL_LEN); // the silhouette fills in from the top
    if(f->sc_n) {
        fx.sc_n = f->sc_n;
        fx.sc_d = 8;
    }
    if(f->sink) {
        fx.clip_y = (int16_t)(y + h - 2);
        y += f->sink;
    }
    if(!fx.invert && !fx.sc_d && !fx.clip_y && !fx.flat)
        blit_masked(lvl, &p->b, x, y);
    else
        blit_masked_fx(lvl, &p->b, x, y, &fx);
}

static void draw_effect(S* s, LvlBuf lvl, bool focus) {
    if(!s->fx_kind) return;
    if(s->fx_side == 2) { // over the capture crystal
        fx_draw(
            &s->fxp,
            lvl,
            s->fx_kind,
            s->fx_t,
            s->cry_x - 9,
            s->cry_y - CRYSTAL_H - 5,
            s->cry_x + 9,
            s->cry_y + 4,
            1,
            0);
        return;
    }
    Fighter* f = &s->f[s->fx_side];
    int x, y, w, h;
    fighter_pos(s, f, focus, &x, &y, &w, &h);
    if(!f->med.blk && !f->nat.blk) {
        w = 24;
        h = 30;
    }
    int dir = s->fx_side == 1 ? 1 :
                                -1; // the user stands on the other side: effects travel along dir
    if(s->fx_miss) x -= dir * (w * 3 / 4 + 6); // a miss lands beside the target, toward the user
    fx_draw(&s->fxp, lvl, s->fx_kind, s->fx_t, x, y, x + w, y + h, dir, s->fx_big);
}

// The projectile of an attack (contract 10.4): from the middle of the user to the middle of the target (beside it on a miss).
static void draw_projectile(S* s, LvlBuf lvl) {
    if(s->an_kind != AN_ATTACK || !(s->an_style & MST_PROJ) || s->an_t < AT_PROJ0) return;
    int ax, ay, aw, ah, bx, by, bw, bh;
    fighter_pos(s, &s->f[s->an_side], false, &ax, &ay, &aw, &ah);
    fighter_pos(s, &s->f[s->an_side ^ 1], false, &bx, &by, &bw, &bh);
    int dir = s->an_side == 0 ? 1 : -1;
    if((s->an_mode & 3) == AM_MISS) bx -= dir * (bw * 3 / 4 + 6);
    fx_proj(
        &s->fxp,
        lvl,
        s->an_style & MST_KIND,
        (uint16_t)(s->an_t - AT_PROJ0),
        AT_PROJ_MS,
        ax + aw / 2,
        ay + ah / 2,
        bx + bw / 2,
        by + bh / 2);
}

static void hero_blit(Blit* b) {
    hero_view(3, 0, b); // facing NE: the back view
}

static void draw_trainers(S* s, LvlBuf lvl) {
    if(s->tr_vis[0] && s->rival.blk) {
        int w = s->rival.b.w, h = s->rival.b.h;
        int x = SCREEN_W - w - RIVAL_RIGHT + s->tr_dx[0], y = RIVAL_BOT - h + 1;
        plat_draw(lvl, &s->xplat, x + w / 2, y + h - 3);
        blit_masked(lvl, &s->rival.b, x, y);
    }
    if(s->tr_vis[1]) {
        Blit hb;
        hero_blit(&hb);
        int x = HERO_LEFT + s->tr_dx[1], y = HERO_BOT - hb.h + 1;
        plat_draw(lvl, &s->xplat, x + hb.w / 2, y + hb.h - 3);
        blit_masked(lvl, &hb, x, y);
    }
}

// The transition works IN PLACE on `lvl`, which still holds the world's last frame (battle_open.c drew it before the
// sprite bank was freed; nothing else writes `lvl` until this scene draws): the flash toggles the inversion when its
// phase changes, the wipe only ever adds black (cumulative: the same pixels as redrawing from a copy). No 2 KB copy
// has to sit in the heap while the plugin loads.
static void draw_trans(SceneCtx* c, S* s) {
    LvlBuf lvl = c->frame->lvl;
    const int fm =
        s->legend ? LG_FLASH_MS :
                    TRANS_FLASH_MS; // a legendary: 3 quicker flashes, then black at once (no wipe)
    int flash_end = 6 * fm;
    uint8_t want = s->trans_t < flash_end ? (uint8_t)((s->trans_t / fm) & 1) : 0;
    if(want != s->inv) {
        inv_all(lvl);
        s->inv = want;
    }
    if(s->trans_t < flash_end) return;
    if(s->legend) {
        lvl_fill(lvl, 3);
        return;
    }
    // wipe: 8 bands of 8 rows, odd bands from the right; 8 steps
    int k = (s->trans_t - flash_end) / TRANS_WIPE_MS + 1;
    if(k > 8) k = 8;
    int cover = SCREEN_W * k / 8;
    for(int band = 0; band < 8; band++)
        for(int y = band * 8; y < band * 8 + 8; y++) {
            if(band & 1)
                lvl_span(lvl, y, SCREEN_W - cover, SCREEN_W, 3);
            else
                lvl_span(lvl, y, 0, cover, 3);
        }
}

// ---- the Eclipse Form's animation (contract 67), all code: a black disc closes over the creature, the sky goes dark and a white corona swells around the disc; behind that black the pictures change, a white flash, and the same
// plays backwards, the disc opening on the new picture. Painted straight into `lvl` after the focus shot (the picture, the platform) was drawn.
static void ecl_disc(
    LvlBuf lvl,
    int cx,
    int cy,
    int r,
    uint8_t
        v) { // a filled disc of level v (the half width of each row is the integer square root of r^2 - dy^2, found bit by bit)
    for(int y = cy - r < 0 ? 0 : cy - r; y <= cy + r && y < SCREEN_H; y++) {
        const int q = r * r - (y - cy) * (y - cy);
        int o = 0;
        for(int b = 64; b; b >>= 1)
            if((o + b) * (o + b) <= q) o += b;
        lvl_span(lvl, y, cx - o, cx + o + 1, v);
    }
}
static void draw_eclipse(
    const S* s,
    LvlBuf
        lvl) { // the second part (an_fx 1) is the first one played backwards: the corona shrinks, the sky lightens, the disc opens on the new picture
    const Fighter* f = &s->f[s->an_side];
    const int w = f->nat.b.w, h = f->nat.b.h, t = s->an_fx ? ECL_MS - s->an_t : s->an_t;
    const int
        cx = SCREEN_W / 2,
        cy = FOC_BOT + 1 - h / 2, r = ((w + h) * 36 / 100 + 2) * (t < 600 ? t : 600) / 600,
        g = t > 600 ?
                (t - 600) / 40 :
                0; // (the disc covers the picture's corners when it is full; the corona swells after it)
    if(t >= 480) lvl_fill(lvl, t >= 570 ? 3 : 2); // the sky goes dark
    ecl_disc(lvl, cx, cy, r + g + 3, 0); // the corona (a white ring), then the black disc
    ecl_disc(lvl, cx, cy, r, 3);
}

static void draw_world(SceneCtx* c, void* data) {
    S* s = data;
    if(s->mode == M_TRANS) {
        draw_trans(c, s);
        return;
    }
    LvlBuf lvl = c->frame->lvl;
    const bool lg_dark = s->an_kind == AN_LEGEND &&
                         s->an_t < LG_BLACK_MS; // the legendary intro's black beat
    lvl_fill(lvl, s->view == V_BLACK || lg_dark ? 3 : 0);
    if(s->mode == M_PARTY || lg_dark)
        return; // the party list is a plain white screen (all in the overlay)
    switch(s->view) {
    case V_NORMAL:
        for(int side = 1; side >= 0; side--) {
            Fighter* f = &s->f[side];
            if(!f->species) continue;
            plat_draw(lvl, &f->plat, f->cx + s->shx, f->cy + s->shy);
        }
        // breathing: a sleeping / idle creature lifts 1 px every other half period (only when nothing animates it)
        for(int side = 1; side >= 0; side--) {
            Fighter* f = &s->f[side];
            if(!f->species) continue;
            int8_t save = f->oy;
            if(s->an_kind == 0 && s->breath && f->oy == 0 && f->visible) f->oy = -1;
            draw_fighter(s, lvl, side, false);
            f->oy = save;
        }
        if(s->cry_on)
            fx_crystal(lvl, s->cry_x + s->shx, s->cry_y + s->shy, s->cry_tilt, s->cry_dim);
        draw_projectile(s, lvl);
        if(s->fx_kind && s->view == V_NORMAL) draw_effect(s, lvl, false);
        break;
    case V_FOCUS:
        plat_draw(lvl, &s->xplat, SCREEN_W / 2, FOC_BOT - 2);
        draw_fighter(s, lvl, s->fside, true);
        draw_effect(s, lvl, true);
        if(s->an_kind == AN_ECLIPSE) draw_eclipse(s, lvl);
        break;
    case V_INTRO:
        draw_trainers(s, lvl);
        break;
    default:
        break;
    }
    fx_frame_end(&s->fxp); // nothing of the effect drawn this frame: its frames leave the heap
}

// ------------------------------------------------------------------------------------------------ drawing: overlay
static void draw_box(int x, int y, int w, int h) {
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
}

static void draw_msgbox(S* s) {
    const Msg* m = &s->m;
    int bh = m->one_line ? box1_h() : box2_h();
    int by = SCREEN_H - bh;
    draw_box(0, by, TB_W, bh);
    int ty = by + TB_PAD_Y;
    int s0 = imin(m->shown, m->n0);
    text_draw_line(m->buf, m->l0, TB_TEXT_X, ty, s0);
    if(m->l1 >= 0 && m->shown > m->n0) {
        if(m->one_line)
            text_draw_line(m->buf, m->l1, TB_TEXT_X + m->w0 + sep_px(), ty, m->shown - m->n0);
        else
            text_draw_line(m->buf, m->l1, TB_TEXT_X, ty + pitch(), m->shown - m->n0);
    }
    if(!m->typing && m->wait_ok && m->arrow &&
       !(s->cur.a & MSF_ASK)) // (no 'more' arrow under a YES / NO)
        font_draw_glyph(
            GLYPH_MORE, TB_W - TB_MORE_RIGHT - font_cell_w(), by + bh - TB_MORE_MARGIN, 1);
}

// ---- plates
#define num_str(o, v) ((void)hud_num((o), (v))) // hud_gfx.c

// Compact plates (contract 10.2). A plate is a 9 px strip "Nv5 PS [HP bar 28x6] 19/19" (es; every other language
// "Lv5 HP [bar] 19/19"), so it can sit on the screen's edge without covering a creature's face. The creature names are not
// on the plates: with 42-46 px creatures nothing wide enough is free (the messages name them).
// Enemy: top-left corner, white fill, black line on its right and bottom edges. Player: right edge, directly above the
// text / menu box, black line on its top and left edges.
#define PLATE_H 9

// Contract 67: the Eclipse Form's mark, 7 x 7 and code-drawn (language and font independent, like the variocolor star): a corona ring around a dark disc. `sel`: highlighted (white on black).
static void ecl_icon(int x, int y, bool sel) {
    static const uint8_t ICON[7] = {0x38, 0x44, 0xBA, 0xBA, 0xBA, 0x44, 0x38};
    if(sel) ov_fill(x - 1, y - 1, ECL_ICON_W + 2, ECL_ICON_W + 2, 1);
    ov_bitmap(x, y, ECL_ICON_W, ECL_ICON_W, ICON, 1, !sel);
}

// One plate: `player` right-aligned with its bottom row on row `ytop` (the top edge of what it stands on), else the enemy's at (0, 0).
static void plate_draw(const Fighter* f, bool player, int ytop) {
    bool es = text_language() ==
              LANG_IDX_ES; // the tiny font is ASCII: ja / zh use "Lv" / "HP" too
    const char* lab = es ? "PS" : "HP";
    char lv[12], nums[16];
    hud_level(lv, f->level);
    num_str(nums, f->hp);
    int n = (int)strlen(nums);
    nums[n] = '/';
    num_str(nums + n + 1, f->max_hp);
    const char* tag = status_tag(f->status);
    int lw = tiny_width(lv) + 3, aw = tiny_width(lab) + 2; // level text + gap, label + gap
    int w = (player ? 3 : 2) + lw + aw + BAR_W + 3 + tiny_width(nums) + (player ? 1 : 3);
    if(tag) w += 2 + tiny_width(tag);
    if(f->form) w += 2 + ECL_ICON_W; // the Eclipse Form's mark
    int x = player ? SCREEN_W - w : 0, y = player ? ytop - PLATE_H + 1 : 0;
    ov_fill(x, y, w, PLATE_H, 0);
    if(!player) {
        ov_fill(x, y + PLATE_H - 1, w, 1, 1);
        ov_fill(x + w - 1, y, 1, PLATE_H, 1);
    } else {
        ov_fill(x, y, w, 1, 1);
        ov_fill(x, y, 1, PLATE_H, 1);
    }
    int ix = x + (player ? 3 : 2), by = y + 2;
    tiny_draw(lv, ix, by);
    tiny_draw(lab, ix + lw, by);
    int bx = ix + lw + aw;
    hpbar_draw_h(bx, by, BAR_W, 6, f->hp, f->max_hp);
    bx +=
        BAR_W + 3; // 3 px of air: a full bar's black end swallowed a leading 1 at 2 (contract 78)
    tiny_draw(nums, bx, by);
    bx += tiny_width(nums);
    if(tag) {
        tiny_draw(tag, bx + 2, by);
        bx += 2 + tiny_width(tag);
    }
    if(f->form) ecl_icon(bx + 2, y + 1, false);
}

// y of the top edge of whatever sits at the bottom of the screen right now (the player's plate stands on it)
static __attribute__((noinline)) int bag_rows_shown(const S* s) {
    return s->bg_n - s->bg_top < BAG_ROWS ? s->bg_n - s->bg_top : BAG_ROWS;
}
static __attribute__((noinline)) int bag_box_h(const S* s) {
    return bag_rows_shown(s) * pitch() + 2 * TB_PAD_Y;
}
static int ui_top(const S* s) {
    if(s->mode == M_MENU) return SCREEN_H - box2_h();
    if(s->mode == M_BAG) return SCREEN_H - bag_box_h(s);
    if(s->mode == M_FIGHT)
        return SCREEN_H - (s->mv_rows * pitch() + 2 * TB_PAD_Y) - (pitch() + 2 * TB_PAD_Y - 1) + 1;
    if(msg_on(s)) return SCREEN_H - (s->m.one_line ? box1_h() : box2_h());
    return SCREEN_H - box1_h();
}

static void draw_plates(S* s) {
    Fighter* e = &s->f[1];
    Fighter* p = &s->f[0];
    if(e->species && (e->visible || s->an_kind == AN_FAINT)) {
        plate_draw(e, false, 0);
    }
    if(p->species && (p->visible || s->an_kind == AN_FAINT) &&
       !(s->mode == M_FIGHT &&
         s->mv_vert)) { // (no player's plate over the one-column move list: contract 72)
        plate_draw(p, true, ui_top(s));
    }
}

static void menu_geom(S* s, int* bx, int* by, int* bw, int* colx, int* cw) {
    (void)s;
    int wl = imax(
        text_line_px(s->lbl[0], 0),
        text_line_px(s->lbl[2], 0)); // (token aware: the Reserva's first entry is "Crystal x{v2}")
    int wr = imax(text_line_px(s->lbl[1], 0), text_line_px(s->lbl[3], 0));
    *cw = font_cell_w() + 2;
    *bw = 3 + *cw + wl + 7 + *cw + wr + 4;
    *bx = SCREEN_W - *bw;
    *by = SCREEN_H - box2_h();
    colx[0] = *bx + 3;
    colx[1] = *bx + 3 + *cw + wl + 7;
}

static void draw_menu(S* s) {
    int bx, by, bw, colx[2], cw;
    menu_geom(s, &bx, &by, &bw, colx, &cw);
    draw_box(bx, by, bw, box2_h());
    for(int k = 0; k < 4; k++) {
        int x = colx[k & 1], y = by + TB_PAD_Y + (k >> 1) * pitch();
        text_draw_line(s->lbl[k], 0, x + cw, y, 99);
        if(s->menu_sel == k) font_draw_glyph(GLYPH_CURSOR, x, y, 1);
    }
}

static const uint8_t ARW_UP[3] = {0x20, 0x70, 0xF8},
                     ARW_DN[3] = {0xF8, 0x70, 0x20}; // 5 x 3 scroll arrows

#ifndef NUCLEO_DEVICE // host tests (tools/host_battle `?fightgeom`, the movescan scenario): where draw_fight put everything on its last call (contract 72)
FightGeom g_sb_fg;
#define FG(f, v) (g_sb_fg.f = (v))
#define FG_NAME(k, x_, y_, w_)  \
    (g_sb_fg.nx[k] = (x_),      \
     g_sb_fg.ny[k] = (y_),      \
     g_sb_fg.nw[k] = (w_),      \
     g_sb_fg.drawn |= 1 << (k), \
     g_sb_fg.sel_drawn |= s->mv_sel == (k))
#else
#define FG(f, v)               ((void)0)
#define FG_NAME(k, x_, y_, w_) ((void)0)
#endif

static void draw_fight(S* s) {
    FG(drawn, 0);
    FG(sel_drawn, 0);
    const int cw = font_cell_w() + 2, p = pitch(), rows = s->mv_rows, bh = rows * p + 2 * TB_PAD_Y,
              wl = s->mv_wl, wr = s->mv_wr, vert = s->mv_vert, top = s->mv_top;
    // two columns: the cursor columns, the names and a gap of 6 px (2 when 6 do not fit); one column: as wide as the longest name (the box does not change width while the cursor scrolls)
    const int gap = 3 + cw + wl + 6 + cw + wr + 4 <= SCREEN_W ? 6 : 2;
    const int bw = vert ? 3 + cw + imax(wl, wr) + 4 : 3 + cw + wl + gap + cw + wr + 4;
    const int bx = SCREEN_W - bw, by = SCREEN_H - bh,
              c1 = cw + wl + gap; // c1: the right column's offset in the two-column box
    draw_box(bx, by, bw, bh);
    for(int k = 0; k < 4; k++) {
        const int r =
            vert ?
                k - top :
                k >> 1; // the row of slot k (a one-column list shows `rows` of them from mv_top)
        if(r < 0 || r >= rows) continue;
        const int x = bx + 3 + (vert ? 0 : (k & 1) * c1), y = by + TB_PAD_Y + r * p;
        if(s->mvn[k][0]) {
            text_draw_line(s->mvn[k], 0, x + cw, y, 99);
            FG_NAME(k, x + cw, y, font_text_width(s->mvn[k]));
#if BF_DISABLE
            if(!s->lrn_mv && !S_CPY(s) && BF_DIS_OFF(s->b, 0, k))
                ov_fill(
                    x + cw,
                    y + font_baseline() - 4,
                    font_text_width(s->mvn[k]),
                    1,
                    1); // a Disabled move is struck through
#endif
        } else
            ov_fill(x + cw, y + font_baseline() - 4, 6, 1, 1); // empty slot
        if(s->mv_sel == k && !s->ecl_sel) font_draw_glyph(GLYPH_CURSOR, x, y, 1);
    }
    if(vert) { // scroll arrows like the bag box's
        if(top > 0) ov_bitmap(bx - 7, by + 1, 5, 3, ARW_UP, 1, 1);
        if(top + rows < s->mv_n) ov_bitmap(bx - 7, SCREEN_H - 4, 5, 3, ARW_DN, 1, 1);
    }
    FG(vert, vert);
    FG(bx, bx);
    FG(by, by);
    FG(bw, bw);
    FG(bh, bh);
    FG(cw, cw);
    // type + PP of the selected move, above the box on the right
    uint8_t mid = fmv(s, s->mv_sel);
    const MoveDef* mv = mid ? bc_move_def(s->b, mid) : NULL;
    char pp[16] = "PP";
    int n = 2;
    pp[n++] = ' ';
    const uint8_t pb =
        s->lrn_mv ?
            s->b->party[0]->mon[s->b->side[0].active].pp[s->mv_sel] :
        S_CPY(s) ?
            0 :
            *bc_ppp(
                s->b,
                0,
                s->mv_sel); // contract 77: the PP byte holds the PP Plus count in its top bits
    num_str(
        pp + n,
        S_CPY(s) ? (mv ? mv->pp : 0) :
                   pb & BC_PP_LEFT); // (Echoing's list shows the foe's moves with their full PP)
    n = (int)strlen(pp);
    pp[n++] = '/';
    num_str(
        pp + n,
        bc_pp_max(
            !mv                   ? 0 :
            S_CPY(s) || s->lrn_mv ? mv->pp :
                                    bc_pp_base(s->b, 0, s->mv_sel),
            pb)); // contract 79: the maximum comes from the slot's own move and Transform's 5 (bc_pp_base): a Echoing copy keeps Echoing's maximum, a Transformed slot is 5 / 5
#ifndef NUCLEO_DEVICE
    snprintf(g_sb_ppstr, sizeof(g_sb_ppstr), "%s", pp); // (host tests read the text: ?ppstr=)
#endif
    const int ew =
        ecl_ready(s) ?
            ECL_ICON_W + 4 :
            0; // contract 67: the Eclipse chip, left of the type (highlighted while it has the cursor)
    int tw = font_text_width(s->tname), pw = tiny_width(pp);
    int iw = 3 + ew + tw + 5 + pw + 3, ih = pitch() + 2 * TB_PAD_Y - 1;
    int ix = SCREEN_W - iw, iy = by - ih + 1;
    draw_box(ix, iy, iw, ih);
    FG(ix, ix);
    FG(iy, iy);
    FG(iw, iw);
    FG(ih, ih);
    if(ew) ecl_icon(ix + 4, iy + (ih - ECL_ICON_W) / 2, s->ecl_sel);
    font_draw_text(s->tname, ix + 3 + ew, iy + TB_PAD_Y - 1);
    tiny_draw(pp, ix + 3 + ew + tw + 5, iy + TB_PAD_Y - 1 + font_baseline() - 5);
}

static void draw_bag(S* s) {
    const int rows = bag_rows_shown(s), cw = font_cell_w() + 2;
    int wn = 0;
    for(int i = 0; i < rows; i++)
        wn = imax(wn, font_text_width(s->itn[i]));
    const int bw = 3 + cw + wn + 6 + tiny_width("x99") + 4, bh = bag_box_h(s);
    const int bx = SCREEN_W - bw, by = SCREEN_H - bh;
    draw_box(bx, by, bw, bh);
    for(int i = 0; i < rows; i++) {
        uint8_t it = 0, n = 0;
        bag_get((uint8_t)(s->bg_top + i), &it, &n);
        const int y = by + TB_PAD_Y + i * pitch();
        if(s->bg_top + i == s->bg_sel) font_draw_glyph(GLYPH_CURSOR, bx + 3, y, 1);
        text_draw_line(s->itn[i], 0, bx + 3 + cw, y, 99);
        char t[8] = "x";
        hud_num(t + 1, n);
        tiny_draw(t, bx + bw - 4 - tiny_width(t), y + font_baseline() - 5);
    }
    if(s->bg_top > 0) ov_bitmap(bx - 7, by + 1, 5, 3, ARW_UP, 1, 1);
    if(s->bg_top + BAG_ROWS < s->bg_n) ov_bitmap(bx - 7, SCREEN_H - 4, 5, 3, ARW_DN, 1, 1);
}

// Two-option box on the right edge with its bottom at row `bottom`: the party list's Cambiar / Cancelar, and the YES / NO of the forget-a-move question (labels l_switch / l_cancel, cursor pm_opt).
static void draw_opts(const S* s, int bottom) {
    const int p = pitch(), cw = font_cell_w(),
              w = cw + 9 + imax(font_text_width(s->l_switch), font_text_width(s->l_cancel)) + 4;
    const int h = 2 * p + 4, x = SCREEN_W - w, y = bottom - h;
    draw_box(x, y, w, h);
    const tchar* lab[2] = {s->l_switch, s->l_cancel};
    for(int i = 0; i < 2; i++) {
        if(s->pm_opt == i) font_draw_glyph(GLYPH_CURSOR, x + 3, y + 2 + i * p, 1);
        font_draw_text(lab[i], x + 3 + cw + 2, y + 2 + i * p);
    }
}

// party list: name, "L5", HP bar + numbers, status tag per creature; 3 rows, the cursor is the > glyph.
static void draw_party(S* s) {
    const int p = pitch(), rh = p + 8, vis = pm_rows(), n = s->b->party[0]->count;
    const int nx = 2 + font_cell_w() + 2;
    for(int v = 0; v < vis && s->pm_top + v < n; v++) {
        const int k = s->pm_top + v;
        const Mon* m = &s->b->party[0]->mon[k];
        const int y = 1 + v * rh;
        if(k == s->pm_sel) font_draw_glyph(GLYPH_CURSOR, 2, y, 1);
        font_draw_text(s->pname[k], nx, y);
        char t[12];
        hud_level(t, m->level);
        tiny_draw(t, 118 - tiny_width(t), y + font_baseline() - 5);
        const int by = y + p + 1;
        hpbar_draw(nx, by, 44, m->hp, m->max_hp);
        char nums[16];
        int q = hud_num(nums, m->hp);
        nums[q++] = '/';
        hud_num(nums + q, m->max_hp);
        tiny_draw(nums, nx + 47, by);
        const char* tag = hud_status_tag(m->status, m->hp);
        if(tag) tiny_draw(tag, 118 - tiny_width(tag), by);
    }
    if(s->pm_top > 0) ov_bitmap(122, 1, 5, 3, ARW_UP, 1, 1);
    if(s->pm_top + vis < n) ov_bitmap(122, SCREEN_H - 4, 5, 3, ARW_DN, 1, 1);
    if(s->pm_box) draw_opts(s, SCREEN_H); // Cambiar / Cancelar
}

static void draw_overlay(SceneCtx* c, void* data) {
    S* s = data;
    if(s->mode == M_PARTY) {
        draw_party(s);
    } else if(s->mode != M_TRANS && s->mode != M_EXIT) {
        if(s->mode == M_MENU)
            draw_menu(s);
        else if(s->mode == M_BAG)
            draw_bag(s);
        else if(s->mode == M_FIGHT)
            draw_fight(s);
        else if(msg_on(s))
            draw_msgbox(s);
        if(s->view == V_NORMAL && s->an_kind != AN_LEGEND)
            draw_plates(
                s); // after the box: the player's plate (9th row) stands open on its top edge (contract 10.2)
        if(ask_on(s) && !s->m.typing)
            draw_opts(
                s,
                SCREEN_H - (s->m.one_line ? box1_h() : box2_h()) +
                    1); // over the player's plate, as
    }
    scene_debug_overlay_draw(
        c); // heap / stack numbers while the debug overlay is on (Left + Right + OK in the world)
}

// ------------------------------------------------------------------------------------------------ scene ops
static void free_all(S* s) {
    for(int k = 0; k < 2; k++)
        fighter_free_pics(&s->f[k]);
    monpic_free(&s->rival);
    plat_free(&s->xplat);
    fx_release(&s->fxp);
}

static void exit_start(SceneCtx* c, S* s) {
    s->mode = M_EXIT;
    s->exit_step = 0;
    s->exit_t = 0;
    s->cur_active = false;
    scene_dirty(c, D_COMPOSE);
}

static void finish(SceneCtx* c, S* s) {
    uint8_t res = BATTLE_LOST;
    if(s->b->result == BR_WIN)
        res = BATTLE_WON;
    else if(s->b->result == BR_RAN)
        res = BATTLE_RAN;
    else if(s->b->result == BR_CAUGHT)
        res = BATTLE_CAUGHT;
    if(s->result_out) *s->result_out = res;
    scene_pop(c); // leave() frees everything
    battle_scene_closed(
        c); // the app: screen white, then (plugin unloaded) the bank back and the fade-in
}

static void b_leave(SceneCtx* c, void* d) {
    S* s = d;
    free_all(s);
    free(s->b);
    free(s->own);
    text_clear_vars();
    c->fade = 0;
    c->hide_overlay = false;
    g_s = NULL;
    free(s);
    // The map's sprite bank is NOT restored here: this code is still part of the loaded plugin (26 KB) and the bank is
    // 7 - 13 KB; finish() hands over to battle_scene_closed(), which restores it after the plugin is unloaded.
}

static void b_enter(SceneCtx* c, void* d) {
    S* s = d;
    c->fade = 0;
    s->mode = M_TRANS;
    s->trans_t = 0;
    s->view = V_BLACK;
    scene_dirty(c, D_WORLD);
    scene_wake(c, 1);
}

static void b_input(SceneCtx* c, void* d, const InputEv* ev) {
    S* s = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT) {
            if(s->mode == M_RUN) {
                if(ask_on(s) && !s->m.typing) { // Back = NO
                    s->pm_opt = 1;
                    learn_answer(c, s);
                }
            } else if(s->mode == M_FIGHT && s->lrn_mv)
                learn_ask_go(c, s, 0);
#if BF_MOVE_COPY
            else if(s->mode == M_FIGHT && s->cpy) { // Back from Echoing's list: the creature's own moves
                s->cpy = 0;
                fight_open(c, s);
            }
#endif
            else if(s->mode == M_FIGHT || s->mode == M_BAG)
                menu_enter(c, s);
            else if(s->mode == M_PARTY) {
                if(s->pm_box) {
                    s->pm_box = false;
                    scene_dirty(c, D_COMPOSE);
                } else if(s->pp == PP_POTION)
                    bag_open(c, s);
                else if(s->pp == PP_SWITCH)
                    menu_enter(c, s);
                else if(s->pp == PP_SHIFT)
                    learn_go(c, s); // contract 75: Back = keep the creature in play
            }
        }
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    switch(s->mode) {
    case M_TRANS:
        if(s->legend && ev->key == KEY_OK &&
           s->trans_t < 6 * LG_FLASH_MS) { // OK skips the flashes of a legendary intro
            s->trans_t = 6 * LG_FLASH_MS;
            scene_dirty(c, D_WORLD);
            scene_wake(c, 1);
        }
        break;
    case M_RUN:
        if(s->an_kind == AN_LEGEND && ev->key == KEY_OK &&
           ev->type == EV_PRESS) { // ... and OK skips the rest of it (the silhouette, the fill-in)
            s->an_t = s->an_dur;
            scene_wake(c, 1);
        } else if(msg_on(s)) {
            Msg* m = &s->m;
            if((s->cur.a & MSF_ASK) && (ev->key == KEY_UP || ev->key == KEY_DOWN)) {
                s->pm_opt ^= 1;
                scene_dirty(c, D_COMPOSE);
            } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
                if(m->typing) {
                    m->shown = m->total;
                    m->typing = false;
                    m->hold = 0;
                    m->blink = 0;
                    m->arrow = true;
                } else if(s->cur.a & MSF_ASK) {
                    learn_answer(c, s);
                    break;
                } else {
                    m->ok_pressed = true;
                }
                scene_dirty(c, D_COMPOSE);
                scene_wake(c, 1);
            }
        }
        break;
    case M_MENU: {
        int k = s->menu_sel;
        if(ev->key == KEY_LEFT)
            k &= ~1;
        else if(ev->key == KEY_RIGHT)
            k |= 1;
        else if(ev->key == KEY_UP)
            k &= ~2;
        else if(ev->key == KEY_DOWN)
            k |= 2;
        else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
            if(s->reserve) { // Crystal / Rock / Bait / Run (the Reserve battle has no moves, no party and no bag)
                static const uint8_t SK[4] = {ACT_ITEM, ACT_ROCK, ACT_BAIT, ACT_RUN};
                if(k == 0 && !bag_take(IT_CRISTAL_RESERVA))
                    say(c,
                        s,
                        TXT_GL_C5B_SF_NONE,
                        0,
                        AFTER_MENU); // (the crystal is taken first: none left = no throw)
                else {
                    s->b->act[0] = (BcAction){SK[k], IT_CRISTAL_RESERVA, 0};
                    start_turn(c, s);
                }
                break;
            }
            if(k == 0) {
#if BF_MOVE_COPY
                s->cpy = 0;
#endif
                fight_open(c, s);
            } else if(k == 1) {
                if(bag_kinds() && !s->b->no_reward)
                    bag_open(c, s); // (an exhibition battle: no items, link rules)
                else
                    say(c, s, TXT_GL_BT_BAG_EMPTY, 0, AFTER_MENU);
            } else if(k == 2)
                party_open(c, s, PP_SWITCH);
            else if(s->trainer)
                say(c, s, TXT_GL_BT_CANT_RUN, 0, AFTER_MENU);
            else
                do_choose(c, s, (BcAction){ACT_RUN, 0, 0});
            break;
        }
        if(k != s->menu_sel) {
            s->menu_sel = (uint8_t)k;
            scene_dirty(c, D_COMPOSE);
        }
        break;
    }
    case M_FIGHT: {
        int k = s->mv_sel;
        int nk = k;
        if(s->ecl_sel) { // contract 67: the cursor is on the Eclipse chip
            if(ev->key == KEY_OK && ev->type == EV_PRESS)
                eclipse_go(c, s);
            else if(ev->key == KEY_DOWN) {
                s->ecl_sel = 0;
                scene_dirty(c, D_COMPOSE);
            }
            break;
        }
        if(ev->key == KEY_UP && (s->mv_vert ? k == 0 : !(k & 2)) &&
           ecl_ready(s)) { // (the top row of either layout)
            s->ecl_sel = 1;
            scene_dirty(c, D_COMPOSE);
            break;
        }
        if(s->mv_vert) { // contract 72, one column: Up / Down walk the moves in order, Left / Right do nothing
            nk = k + (ev->key == KEY_DOWN) - (ev->key == KEY_UP);
            if(nk > 3 || nk < 0) nk = k;
        } else if(ev->key == KEY_LEFT)
            nk = k & ~1;
        else if(ev->key == KEY_RIGHT)
            nk = k | 1;
        else if(ev->key == KEY_UP)
            nk = k & ~2;
        else if(ev->key == KEY_DOWN)
            nk = k | 2;
        if(ev->key == KEY_OK && ev->type == EV_PRESS) {
            if(s->lrn_mv) learn_forget(c, s, (uint8_t)k);
#if BF_MOVE_COPY
            else if(s->cpy) { // Echoing's list: this move of the foe goes with the action (BcAction.arg2)
                s->cpy = 0;
                do_choose(c, s, (BcAction){ACT_MOVE, s->cpy_slot, (uint8_t)k});
            }
#endif
#if BF_MOVE_COPY
            else if(
                bc_move_def(s->b, fmv(s, k))->effect == EF_MOVE_CALL &&
                bc_move_def(s->b, fmv(s, k))->arg0 == 2 && !s->ghost &&
                bc_usable(
                    s->b,
                    0,
                    k)) { // Echoing: pick one of the foe's moves first (not when the core would refuse the move: no PP left, Disabled)
                s->cpy = 1;
                s->cpy_slot = (uint8_t)k;
                s->mv_sel = 0;
                fight_open(c, s);
            }
#endif
            else
                do_choose(c, s, (BcAction){ACT_MOVE, (uint8_t)k, 0});
            break;
        }
        if(nk != k && fmv(s, nk)) {
            s->mv_sel = (uint8_t)nk;
            s->mv_top =
                (uint8_t)(nk < s->mv_top ?
                              nk :
                          nk >= s->mv_top + FIGHT_ROWS ?
                              nk - FIGHT_ROWS + 1 :
                              s->mv_top); // (the one-column list's window; the two-column list ignores it)
            fight_info_load(s);
            scene_dirty(c, D_COMPOSE);
        }
        break;
    }
    case M_BAG:
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            int n = s->bg_n;
            s->bg_sel = (uint8_t)((s->bg_sel + (ev->key == KEY_DOWN ? 1 : n - 1)) % n);
            if(s->bg_sel < s->bg_top) s->bg_top = s->bg_sel;
            if(s->bg_sel >= s->bg_top + BAG_ROWS) s->bg_top = (uint8_t)(s->bg_sel - BAG_ROWS + 1);
            bag_names(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
            bag_pick(c, s);
        }
        break;
    case M_PARTY:
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            if(s->pm_box) {
                s->pm_opt ^= 1;
                scene_dirty(c, D_COMPOSE);
            } else {
                pm_move(c, s, ev->key == KEY_DOWN ? 1 : -1);
            }
        } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
            if(s->pp == PP_SWITCH && !s->pm_box) {
                s->pm_box = true;
                s->pm_opt = 0;
                scene_dirty(c, D_COMPOSE);
            } else if(s->pm_box && s->pm_opt == 1) {
                s->pm_box = false;
                scene_dirty(c, D_COMPOSE);
            } else {
                s->pm_box = false;
                pm_pick(c, s);
            }
        }
        break;
    default:
        break;
    }
}

static void b_update(SceneCtx* c, void* d, uint32_t dt) {
    S* s = d;
    scene_debug_overlay_update(c);
    if(dt > 100) dt = 100;
    s->clk += dt;
    // HP drain (background): one bar step every DRAIN_MS
    bool draining = false;
    for(int k = 0; k < 2; k++) {
        Fighter* f = &s->f[k];
        if(f->hp == f->hp_to) continue;
        uint32_t dd = dt;
        if(f->drain_delay) { // the hit has not landed yet
            if(dd >= f->drain_delay) {
                dd -= f->drain_delay;
                f->drain_delay = 0;
            } else {
                f->drain_delay = (uint16_t)(f->drain_delay - dd);
                dd = 0;
            }
        }
        f->drain_acc = (uint16_t)(f->drain_acc + dd);
        int step = f->max_hp / BAR_W;
        if(step < 1) step = 1;
        if(!f->drain_ms) f->drain_ms = 34;
        while(f->drain_acc >= f->drain_ms && f->hp != f->hp_to) {
            f->drain_acc = (uint16_t)(f->drain_acc - f->drain_ms);
            if(f->hp > f->hp_to)
                f->hp = (int16_t)(f->hp - step < f->hp_to ? f->hp_to : f->hp - step);
            else
                f->hp = (int16_t)(f->hp + step > f->hp_to ? f->hp_to : f->hp + step);
        }
        scene_dirty(c, D_COMPOSE);
        if(f->hp != f->hp_to) draining = true;
    }
    uint32_t wake = SCENE_NO_WAKE;
    switch(s->mode) {
    case M_TRANS: {
        s->trans_t = (uint16_t)(s->trans_t + dt);
        int total = s->legend ? 6 * LG_FLASH_MS : 6 * TRANS_FLASH_MS + 8 * TRANS_WIPE_MS;
        if(s->trans_t >= total + (s->legend ? 40 : 120)) {
            // from here the screen is black; the world is not drawn until the battle is over (its bank was freed by
            // battle_open.c before the plugin loaded and comes back after the plugin is gone, battle_scene_closed())
            // into the intro: trainers (trainer battles) or straight into the field (wild)
            s->mode = M_RUN;
            s->view = V_BLACK;
            s->cur_active = false;
            s->qi = s->qn = 0;
            if(s->trainer && s->have_rival) {
                q_cut(s, V_INTRO, 0);
                s->tr_vis[0] = s->tr_vis[1] = true;
                s->tr_dx[0] = 70;
                s->tr_dx[1] = -70;
                q_anim(s, AN_TR_IN, 0, 0);
                q_m0(s, TXT_GL_BT_CHALLENGE, MSF_OK);
            } else {
                q_cut(s, V_NORMAL, 0);
                s->intro_done = true;
            }
            scene_dirty(c, D_WORLD);
            wake = 1;
        } else {
            scene_dirty(c, D_WORLD);
            wake = 20;
        }
        break;
    }
    case M_RUN:
        run_ops(c, s, dt);
        if(s->mode == M_RUN) {
            wake = 16;
            if(msg_on(s) && !s->m.typing) wake = 40;
        }
        break;
    case M_EXIT: {
        s->exit_t = (uint16_t)(s->exit_t + dt);
        if(s->exit_t >= FADE_STEP_MS) {
            s->exit_t = 0;
            s->exit_step++;
            if(s->exit_step > FADE_STEPS) {
                finish(c, s);
                return;
            }
            c->fade = s->exit_step;
            scene_dirty(c, D_COMPOSE);
        }
        wake = FADE_STEP_MS;
        break;
    }
    default:
        break;
    }
    // breathing in the quiet states
    if(s->mode == M_MENU || s->mode == M_FIGHT || s->mode == M_BAG || msg_on(s)) {
        uint8_t ph = (uint8_t)((s->clk / BREATH_MS) & 1);
        if(ph != s->breath && s->view == V_NORMAL) {
            s->breath = ph;
            scene_dirty(c, D_WORLD);
        }
        uint32_t next = BREATH_MS - (s->clk % BREATH_MS);
        if(next < wake) wake = next;
    }
    if(draining && wake > 20) wake = 20;
    if(wake != SCENE_NO_WAKE) scene_wake(c, wake);
}

const SceneOps scene_battle_ops = {
    .enter = b_enter,
    .leave = b_leave,
    .input = b_input,
    .update = b_update,
    .draw_world = draw_world,
    .draw_overlay = draw_overlay,
    .pauses_below = true,
};

// ------------------------------------------------------------------------------------------------ opening
static bool
    g_legend_next; // scene_battle_wild -> open_common: the wild level had bit 7 (contract 52)
static bool open_common(
    SceneCtx* c,
    Party* foe,
    bool trainer,
    uint8_t trainer_id,
    uint8_t* result_out,
    const LinkFoe* lf) {
    const bool legend =
        g_legend_next &&
        !trainer; // (read and cleared first: a refused open must not leave it set for the next battle)
    g_legend_next = false;
    if(c->n >= MAX_SCENES) return false;
    const BcData* data = party_data();
    if(!data || !party_player()->count) return false;
    if(!heap_can_alloc((uint32_t)(sizeof(Battle) + sizeof(S) + 8192))) return false;
    S* s = calloc(1, sizeof(S));
    Battle* b = malloc(sizeof(Battle));
    if(!s || !b) {
        free(s);
        free(b);
        return false;
    }
    uint32_t seed = bc_rng_next(party_rng());
    Party* mine =
        lf ?
            &((LinkFoe*)lf)->mine :
            party_player(); // IR LINK exhibition (contract 45): our team is a healed copy inside the block (no experience, no prize, no item, no Registro entry)
    if(!bc_init(b, data, mine, foe, trainer, trainer_id, seed)) {
        free(s);
        free(b);
        return false;
    }
    b->no_reward = lf != NULL;
    b->share = bag_count(IT_REPARTO_TOTAL) != 0; // contract 74
#if BF_OBEY
    b->cap = 255;
#endif
#if BF_BADGE || BF_OBEY
    if(!lf &&
       c->world) { // contract 73: the badges boost Attack / Defense / Speed / Special (Boulder, Thunder, Soul, Volcano: x 9/8) and decide up to what level a creature from another trainer obeys (Cascade 30, Rainbow 50, Marsh 70, Earth: all)
        const uint8_t bg = c->world->flags[FLAG_BADGE_FIRST >> 3];
#if BF_BADGE
        b->side[0].bdg = (uint8_t)((bg & 1) | (bg >> 1 & 2) | (bg >> 2 & 4) | (bg >> 3 & 8));
#endif
#if BF_OBEY
        b->cap = bg & 0x80 ? 255 : bg & 0x20 ? 70 : bg & 0x08 ? 50 : bg & 2 ? 30 : 10;
#endif
    }
#endif
    s->own = (LinkFoe*)lf; // (the scene owns the block from here: b_leave frees it)
    s->fname = lf ? lf->name : NULL;
    s->b = b;
    s->result_out = result_out;
    s->trainer = trainer;
    s->legend = legend;
    s->ghost = !trainer && c->world && (c->world->map.flags & MAPF_GHOSTS) && !bag_count(IT_LENTE);
    s->reserve = !trainer && !s->ghost &&
                 party_state()->rv_steps != 0; // inside the Reserva Salvaje (contract 43)
    if(s->reserve) {
        b->reserve = true;
        b->sf_rate = bc_species(data, foe->mon[0].species)->catch_rate;
    }
    if(s->ghost || s->reserve)
        b->side[0].run_attempts =
            255; // the run formula always escapes from that many attempts on: "Run always works"
    s->tview = V_NORMAL;
    load_events(s);
    // the platform first: plat_build's working buffer (1.4 KB) is not alive together with the trainer's picture
    if(trainer && !plat_build(
                      &s->xplat,
                      TR_PLAT_RX,
                      TR_PLAT_RY,
                      2)) { /* the intro shows the trainers without a platform */
    }
    if(trainer) s->have_rival = load_trainer_pic(s);
    g_s = s;
    if(!scene_push(c, &scene_battle_ops, s)) {
        free_all(s);
        free(b);
        free(s);
        g_s = NULL;
        return false;
    }
    return true;
}

bool scene_battle_trainer(SceneCtx* c, uint8_t trainer, uint8_t* result_out) {
    const BcData* data = party_data();
    if(!data || trainer == 0 || trainer >= data->n_trainers) return false;
    Party foe;
    bc_party_clear(&foe);
    LinkFoe* lf =
        g_link_foe && g_link_foe->trainer == trainer ?
            g_link_foe :
            NULL; // IR LINK (contract 45): the friend's team, made for exactly this trainer id
    if(lf) {
        foe = lf->party;
        g_link_foe =
            NULL; // the battle takes the block over (it frees it when the scene ends), or it is freed here when the scene cannot open
        const bool ok = open_common(c, &foe, true, trainer, result_out, lf);
        if(!ok) free(lf);
        return ok;
    }
    if(!bc_trainer_party(data, trainer, party_starter(), &foe, party_rng())) return false;
#ifndef NUCLEO_DEVICE
    if(g_sb_shiny_foe) // host tests only (contract 72): the trainer's creatures are variocolors (the worst case of the op queue: a sparkle on the trainer's first send-out)
        for(int k = 0; k < foe.count; k++) {
            Mon* fm = &foe.mon[k];
            fm->dv[1] = fm->dv[2] = fm->dv[3] = 10;
            bc_mon_recalc(data, fm);
            fm->hp = fm->max_hp;
        }
#endif
    return open_common(c, &foe, true, trainer, result_out, NULL);
}

bool scene_battle_wild(SceneCtx* c, uint8_t species, uint8_t level, uint8_t* result_out) {
    const BcData* data = party_data();
    if(!data || species == 0 || species >= data->n_species) return false;
    Party foe;
    bc_party_clear(&foe);
    g_legend_next =
        level &
        128; // bit 7 of the wild level = a legendary encounter (the `legend=` idiom, contract 52)
    bc_mon_create(
        data,
        &foe.mon[0],
        aff_wild(species),
        level & 127,
        party_rng()); // (contract 66: a Lunar player meets the counterpart of the table's Solar form)
#if defined(NUCLEO_DEBUG) || \
    !defined(                \
        NUCLEO_DEVICE) // contract 65: the DEV / host switch that forces a variocolor wild creature (story flag 819, FLAG_DEV_SHINY; a release build never reads it)
    if(c->world && (c->world->flags[FLAG_DEV_SHINY >> 3] >> (FLAG_DEV_SHINY & 7) & 1)) {
        Mon* fm = &foe.mon[0];
        fm->dv[1] = fm->dv[2] = fm->dv[3] = 10;
        bc_mon_recalc(data, fm);
        fm->hp = fm->max_hp;
    }
#endif
    foe.count = 1;
    return open_common(c, &foe, false, 0, result_out, NULL);
}

// ------------------------------------------------------------------------------------------------ test hooks
const Battle*
    scene_battle_core(void) { // also the plugin's "a battle is on" probe (battle_plugin.c)
    return g_s ? g_s->b : NULL;
}
#ifndef NUCLEO_DEVICE // host harness only (tools/host_battle): not worth ~0.5 KB of the plugin's RAM
int scene_battle_shiny(
    int side) { // contract 65: the fighter of `side` is drawn as a variocolor (-1: no battle)
    return g_s ? g_s->f[side & 1].shiny : -1;
}
int scene_battle_pic_ok(
    int side) { // contract 65: the medium picture of `side` is the species' own picture, with its tones swapped exactly when the fighter is a variocolor (-1: nothing to compare)
    if(!g_s || !g_s->f[side & 1].med.blk) return -1;
    const Fighter* f = &g_s->f[side & 1];
    MonPic p;
    if(!fighter_pic(g_s, &p, f->species, f->form, side == 1 ? BP_FRONT_MED : BP_BACK_MED))
        return -1; // (the Eclipse Form's own picture while it is on, contract 67)
    if(f->shiny) monpic_shiny(&p);
    const int ok = !memcmp(p.blk, f->med.blk, (size_t)(p.b.bpr + p.b.mbpr) * p.b.h);
    monpic_free(&p);
    return ok;
}
int scene_battle_cpy(void) { // contract 77: Echoing is choosing
    return g_s ? S_CPY(g_s) : -1;
}
int scene_battle_drawn(
    int side) { // contract 85: the fighter of `side` is on screen (not hidden by a blink, a faint or Dig / Fly)
    return g_s ? g_s->f[side & 1].visible && g_s->f[side & 1].blink != 2 && !g_s->f[side & 1].dug :
                 -1;
}
int scene_battle_fsp(int side) { // contract 77: the species the fighter is drawn as
    return g_s ? g_s->f[side & 1].species : -1;
}
char g_sb_ppstr[24];
int scene_battle_ecl(
    int what) { // contract 67 (host tests): 0 the Eclipse chip is offered, 1 the cursor is on it, 2 the form was taken in this battle, 3 / 4 the form of the player's / the enemy's creature in play (the core), 5 / 6 the picture form of each fighter; -1 no battle
    if(!g_s) return -1;
    switch(what) {
    case 0:
        return ecl_ready(g_s) && g_s->mode == M_FIGHT;
    case 1:
        return g_s->ecl_sel;
    case 2:
        return g_s->b->ecl_used;
    case 3:
        return g_s->b->side[0].ecl;
    case 4:
        return g_s->b->side[1].ecl;
    case 5:
        return g_s->f[0].form;
    default:
        return g_s->f[1].form;
    }
}
BattleUiMode scene_battle_mode(void) {
    if(!g_s) return BUI_EXIT;
    switch(g_s->mode) {
    case M_TRANS:
        return BUI_TRANSITION;
    case M_RUN:
        return BUI_RUNNING;
    case M_MENU:
        return BUI_MENU;
    case M_FIGHT:
        return BUI_FIGHT;
    case M_BAG:
        return BUI_BAG;
    case M_PARTY:
        return BUI_PARTY;
    default:
        return BUI_EXIT;
    }
}
static uint32_t blk_bytes(const uint8_t* blk, const Blit* b) {
    return blk ? (uint32_t)(b->bpr + b->mbpr) * b->h : 0;
}
void scene_battle_heap(uint32_t* bytes, uint32_t* blocks) {
    uint32_t n = 0, k = 0;
    if(g_s) {
        n += sizeof(S) + sizeof(Battle);
        k += 2;
        for(int i = 0; i < 2; i++) {
            const Fighter* f = &g_s->f[i];
            const struct {
                const uint8_t* blk;
                const Blit* b;
            } L[3] = {{f->med.blk, &f->med.b}, {f->nat.blk, &f->nat.b}, {f->plat.blk, &f->plat.b}};
            for(int j = 0; j < 3; j++)
                if(L[j].blk) {
                    n += blk_bytes(L[j].blk, L[j].b);
                    k++;
                }
        }
        if(g_s->rival.blk) n += blk_bytes(g_s->rival.blk, &g_s->rival.b), k++;
        if(g_s->xplat.blk) n += blk_bytes(g_s->xplat.blk, &g_s->xplat.b), k++;
    }
    *bytes = n;
    *blocks = k;
}
int scene_battle_ask(
    void) { // 0 = no question; 1 forget a move? (cursor on YES), 2 the same on NO, 3 stop learning? (YES), 4 the same on NO, 5 / 6 change creatures? (contract 75: YES / NO)
    return g_s && ask_on(g_s) && !g_s->m.typing ?
               ((g_s->cur.a & MSF_SHIFT) ? 5 : 1 + 2 * !!(g_s->cur.a & MSF_STOP)) + g_s->pm_opt :
               0;
}
int scene_battle_hp(
    int side,
    int* hp,
    int* hp_to) { // contract 75: the HP the plate of `side` shows now and the value it drains to (-1 no battle)
    if(!g_s) return -1;
    *hp = g_s->f[side & 1].hp;
    *hp_to = g_s->f[side & 1].hp_to;
    return g_s->f[side & 1].visible;
}
int scene_battle_typed(void) { // a message (or question) is on screen and fully typed
    return g_s && msg_on(g_s) && !g_s->m.typing;
}
int scene_battle_learning(void) { // the move waiting to be learned (0 none)
    return g_s ? g_s->lrn_mv : 0;
}
int scene_battle_uses(void) {
    return g_s ? g_s->uses : 0;
}
int scene_battle_anim(int* t, int* dur) {
    if(!g_s || !g_s->an_kind) return 0;
    *t = g_s->an_t;
    *dur = g_s->an_dur;
    return g_s->an_kind;
}
int scene_battle_view(int* side) {
    if(!g_s) return 0;
    if(side) *side = g_s->fside;
    return (int)g_s->view;
}
void scene_battle_dump(void) { // one line of the scene's runner state (debugging a stuck battle)
    if(!g_s) return;
    printf(
        "[dump] mode %d view %d cur_active %d cur.op %d qi/qn %d/%d ev %d/%d phase %d an_kind %d msg typing %d wait_ok %d after %d\n",
        g_s->mode,
        g_s->view,
        g_s->cur_active,
        g_s->cur.op,
        g_s->qi,
        g_s->qn,
        g_s->ev_i,
        g_s->ev_n,
        g_s->b->phase,
        g_s->an_kind,
        g_s->m.typing,
        g_s->m.wait_ok,
        g_s->after);
    printf(
        "[dump] pp %d pm_sel %d box %d opt %d party %d active %d item %d\n",
        g_s->pp,
        g_s->pm_sel,
        g_s->pm_box,
        g_s->pm_opt,
        g_s->b->party[0]->count,
        g_s->b->side[0].active,
        g_s->item);
    for(int k = 0; k < g_s->b->party[0]->count; k++)
        printf(
            "  party[%d] sp %d hp %d/%d\n",
            k,
            g_s->b->party[0]->mon[k].species,
            g_s->b->party[0]->mon[k].hp,
            g_s->b->party[0]->mon[k].max_hp);
}
const tchar* scene_battle_label(int k) {
    return g_s ? g_s->lbl[k & 3] : NULL;
}
uint32_t scene_battle_state_size(void) {
    return (uint32_t)sizeof(S);
}
#endif
