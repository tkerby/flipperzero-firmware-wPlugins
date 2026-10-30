// The SLOT MACHINE (contract section 37), a scene of the CASINO plugin. Three reels, six code-drawn symbols (no art files), bets of 1 - 3 coins (1: the middle row; 2: the three rows;
// 3: the three rows + both diagonals, five lines), the reels spin and each one stops with a press of OK (left to right), Back leaves (not while the reels turn). The rules
// (strip, lines, pay table) are plain functions at the top so the tests can check them against the documented odds without the screen. The screen is language-neutral (icons and
// numbers; the only text is the "out of coins" dialog).
//
// Fairness: a reel's stop position is drawn from the shared creature RNG (party_rng) at the moment OK is pressed and is uniform over the strip, whatever the timing (the player cannot
// aim: the reel keeps turning for 140 ms and then lands on the drawn position). So the odds are exactly the table of contract 37.2 (checked exhaustively and by a 10,000-spin sample).
#include <stdlib.h>
#include <string.h>
#include "casino.h"
#include "party.h"
#include "bag.h"
#include "battle_core.h"
#include "sound.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "gen/maps_gen.h"

// ---------------------------------------------------------------------------------------------------- rules
// The strip (top to bottom): Zero 1, Vertice 2, Bolt 3, Crystal 3, Bell 4, Berry 5 = 18; equal symbols are never next to each other.
const uint8_t SL_STRIP[SL_N] = {
    SY_ZERO,
    SY_BERRY,
    SY_BOLT,
    SY_BELL,
    SY_CRYSTAL,
    SY_BERRY,
    SY_VERT,
    SY_BELL,
    SY_BOLT,
    SY_BERRY,
    SY_CRYSTAL,
    SY_BELL,
    SY_VERT,
    SY_BERRY,
    SY_BOLT,
    SY_BELL,
    SY_CRYSTAL,
    SY_BERRY};

int slots_line_pay(uint8_t a, uint8_t b, uint8_t c) {
    if(a == b && b == c)
        return a == SY_ZERO  ? PAY_ZERO :
               a == SY_VERT  ? PAY_VERT :
               a == SY_BERRY ? PAY_BERRY3 :
                               PAY_TRIO;
    return (a == SY_BERRY && b == SY_BERRY) ? PAY_BERRY2 : 0;
}

uint8_t slots_lines_for_bet(int bet) {
    return bet <= 1 ? 0x01 : bet == 2 ? 0x07 : 0x1F;
}

uint8_t slots_cell(const uint8_t pos[3], int reel, int row) {
    return SL_STRIP[(pos[reel] + row) % SL_N];
}

int slots_pay(const uint8_t pos[3], int bet, uint8_t* paying) {
    // line k: the row of each reel (middle, top, bottom, diagonal \, diagonal /)
    static const int8_t ROW[SL_LINES][3] = {{1, 1, 1}, {0, 0, 0}, {2, 2, 2}, {0, 1, 2}, {2, 1, 0}};
    const uint8_t lines = slots_lines_for_bet(bet);
    int total = 0;
    uint8_t mask = 0;
    for(int k = 0; k < SL_LINES; k++) {
        if(!(lines & (1u << k))) continue;
        const int p = slots_line_pay(
            slots_cell(pos, 0, ROW[k][0]),
            slots_cell(pos, 1, ROW[k][1]),
            slots_cell(pos, 2, ROW[k][2]));
        if(p) mask |= (uint8_t)(1u << k);
        total += p;
    }
    if(paying) *paying = mask;
    return total;
}

uint8_t slots_stop_pos(uint32_t* rng) {
    return (uint8_t)bc_rand_range(rng, 0, SL_N - 1);
}

// ---------------------------------------------------------------------------------------------------- the screen
#define SYM_W  12
#define CELL_W 20 // a reel
#define CELL_H 14
#define WIN_X  37 // the reel window: 3 reels + 2 one-pixel gaps = 62 x 42
#define WIN_Y  8
#define WIN_W  (3 * CELL_W + 2)
#define WIN_H  (3 * CELL_H)
#define SYM_MS \
    55 // a spinning reel moves one symbol in this many ms (the three reels differ slightly: x 1.0, 1.2, 1.4)
#define STOPPING_MS \
    140 // OK pressed: the reel keeps turning this long, then lands on the position that was drawn
#define BOUNCE_MS   70 // the landing kick
#define SPIN_SND_MS 210
#define FLASH_MS    130
#define WIN_MS      1200
#define JACKPOT_MS  3200

enum {
    M_IDLE = 0,
    M_SPIN,
    M_WIN,
    M_END
};
enum {
    R_LAND = 0,
    R_SPIN,
    R_STOPPING,
    R_BOUNCE
};
enum {
    W_NONE = 0,
    W_CLOSE
};

typedef struct {
    uint8_t mode, bet, wait, lines;
    uint8_t pos[3], target[3], rstate[3];
    uint16_t win;
    uint32_t clock, t, rt[3], snd_t;
} SL;

static SL* g;

bool scene_slots_active(void) {
    return g != NULL;
}

// 12 x 12 symbols, 2 bytes per row (the left pixel in bit 7), drawn black on transparent.
static const uint8_t SYM[SY_COUNT][24] = {
    {// bolt
     0x07, 0xC0, 0x0F, 0x80, 0x1F, 0x00, 0x3E, 0x00, 0x7F, 0xE0, 0x7F, 0xC0,
     0x03, 0x80, 0x07, 0x00, 0x06, 0x00, 0x0C, 0x00, 0x08, 0x00, 0x10, 0x00},
    {// crystal
     0x1F, 0x80, 0x20, 0x40, 0x50, 0xA0, 0xFF, 0xF0, 0x49, 0x20, 0x26, 0x40,
     0x16, 0x80, 0x0F, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {// zero
     0x7F, 0xE0, 0x80, 0x10, 0xBF, 0xD0, 0x80, 0xD0, 0x81, 0x90, 0x83, 0x10,
     0x86, 0x10, 0x8C, 0x10, 0xBF, 0xD0, 0x80, 0x10, 0x7F, 0xE0, 0x00, 0x00},
    {// berry
     0x02, 0x00, 0x06, 0xC0, 0x0D, 0x80, 0x1F, 0x80, 0x3F, 0xC0, 0x7F, 0xE0,
     0x6F, 0xE0, 0x6F, 0xE0, 0x7F, 0xE0, 0x3F, 0xC0, 0x1F, 0x80, 0x00, 0x00},
    {// bell
     0x06, 0x00, 0x0F, 0x00, 0x1F, 0x80, 0x1F, 0x80, 0x3F, 0xC0, 0x3F, 0xC0,
     0x7F, 0xE0, 0x7F, 0xE0, 0xFF, 0xF0, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00},
    {// vert
     0xFF, 0xF0, 0x80, 0x10, 0xB0, 0xD0, 0x99, 0x90, 0x8F, 0x10, 0x86, 0x10,
     0x40, 0x20, 0x20, 0x40, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static const uint8_t COIN[8] = {0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C};
static const uint8_t STAR[7] = {0x10, 0x10, 0x7C, 0x38, 0x6C, 0x44, 0x00};
static const uint8_t UPA[3] = {0x20, 0x70, 0xF8}, DNA[3] = {0xF8, 0x70, 0x20};
static const uint8_t OKB[18] = {
    0x3E,
    0x00,
    0x41,
    0x00,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x80,
    0x41,
    0x00,
    0x3E,
    0x00}; // the OK button's ring, 9 x 9

static const tchar DIGITS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0};

static int put_num(tchar* o, uint32_t v) {
    char t[11];
    int n = 0, len = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while(v);
    while(n)
        o[len++] = (tchar)t[--n];
    o[len] = 0;
    return len;
}

static void sfx(uint16_t id) {
    sound_play(id);
}

// ---- input
static void bet_clamp(SL* s) {
    const uint16_t coins = casino_coins();
    if(s->bet > coins) s->bet = (uint8_t)coins;
    if(s->bet < 1) s->bet = 1;
    if(s->bet > 3) s->bet = 3;
}

static void say_out(SceneCtx* c, SL* s) {
    s->mode = M_END;
    s->wait = W_CLOSE;
    if(!scene_dialog_open(c, TXT_GL_C4C_SLOT_OUT, -1)) { // no dialog: just leave
        s->wait = W_NONE;
        scene_pop(c);
        scene_wake(c, 1);
    }
}

static void resume(SceneCtx* c, SL* s) {
    s->wait = W_NONE;
    scene_pop(c);
    scene_wake(c, 1);
}

static void start_spin(SceneCtx* c, SL* s) {
    bet_clamp(s);
    if(casino_coins() == 0 || !casino_coins_take(s->bet)) {
        say_out(c, s);
        return;
    }
    s->mode = M_SPIN;
    s->win = 0;
    s->lines = 0;
    s->t = 0;
    s->snd_t = s->clock;
    for(int r = 0; r < 3; r++) {
        s->rstate[r] = R_SPIN;
        s->rt[r] = 0;
    }
    sfx(SND_FX_SLOT_SPIN);
    scene_dirty(c, D_COMPOSE);
}

static void stop_next(SceneCtx* c, SL* s) {
    for(int r = 0; r < 3; r++)
        if(s->rstate[r] == R_SPIN) {
            s->rstate[r] = R_STOPPING;
            s->rt[r] = 0;
            s->target[r] = slots_stop_pos(party_rng());
            break;
        }
    scene_dirty(c, D_COMPOSE);
}

static void s_input(SceneCtx* c, void* d, const InputEv* ev) {
    SL* s = d;
    if(s->wait) {
        resume(c, s);
        return;
    }
    if(scene_top_ops(c) != &scene_slots_ops) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT && s->mode != M_SPIN) {
            scene_pop(c);
            scene_wake(c, 1);
        }
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    if(s->mode == M_END) return;
    if(ev->key == KEY_OK && press) {
        if(s->mode == M_IDLE)
            start_spin(c, s);
        else if(s->mode == M_SPIN)
            stop_next(c, s);
        else if(s->mode == M_WIN) { // skip the flashing
            s->mode = M_IDLE;
            scene_dirty(c, D_COMPOSE);
        }
    } else if(s->mode != M_SPIN && (ev->key == KEY_UP || ev->key == KEY_DOWN)) {
        const uint8_t old = s->bet;
        const uint16_t max = casino_coins() < 3 ? casino_coins() : 3;
        if(ev->key == KEY_UP)
            s->bet = (uint8_t)(s->bet >= max ? 1 : s->bet + 1); // wraps, like the quantity box
        else
            s->bet = (uint8_t)(s->bet <= 1 ? max : s->bet - 1);
        if(s->bet < 1) s->bet = 1;
        if(s->bet != old) scene_dirty(c, D_COMPOSE);
    }
}

// ---- time
static void finish_spin(SceneCtx* c, SL* s) {
    uint8_t paying = 0;
    const int win = slots_pay(s->pos, s->bet, &paying);
    s->win = (uint16_t)win;
    s->lines = paying;
    s->t = 0;
    if(win) {
        casino_coins_add((uint16_t)win);
        s->mode = M_WIN;
        sfx(win >= PAY_VERT ? SND_FX_SLOT_JACKPOT : SND_FX_SLOT_WIN);
    } else {
        s->mode = M_IDLE;
    }
    bet_clamp(s);
    if(casino_coins() == 0) say_out(c, s);
    scene_dirty(c, D_COMPOSE);
}

static void s_update(SceneCtx* c, void* d, uint32_t dt) {
    SL* s = d;
    if(s->wait) {
        resume(c, s);
        return;
    }
    s->clock += dt;
    if(s->mode == M_SPIN) {
        bool any_turn = false, all_land = true;
        for(int r = 0; r < 3; r++) {
            s->rt[r] += dt;
            if(s->rstate[r] == R_STOPPING && s->rt[r] >= STOPPING_MS) {
                s->pos[r] = s->target[r];
                s->rstate[r] = R_BOUNCE;
                s->rt[r] = 0;
                sfx(SND_FX_SLOT_STOP);
            } else if(s->rstate[r] == R_BOUNCE && s->rt[r] >= BOUNCE_MS) {
                s->rstate[r] = R_LAND;
            }
            if(s->rstate[r] == R_SPIN || s->rstate[r] == R_STOPPING) any_turn = true;
            if(s->rstate[r] != R_LAND) all_land = false;
        }
        if(any_turn && s->clock - s->snd_t >= SPIN_SND_MS) {
            s->snd_t = s->clock;
            sfx(SND_FX_SLOT_SPIN);
        }
        if(all_land) finish_spin(c, s);
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, 33);
    } else if(s->mode == M_WIN) {
        s->t += dt;
        if(s->t >= (s->win >= PAY_VERT ? JACKPOT_MS : WIN_MS)) s->mode = M_IDLE;
        scene_dirty(c, D_COMPOSE);
        scene_wake(c, 33);
    }
}

// ---- drawing
static void sym_rows(
    int sym,
    int x,
    int y,
    int top,
    int bot) { // the rows of symbol `sym` at (x, y) that fall inside [top, bot)
    for(int r = 0; r < SYM_W; r++) {
        const int yy = y + r;
        if(yy >= top && yy < bot) ov_bitmap(x, yy, SYM_W, 1, SYM[sym] + r * 2, 2, 1);
    }
}

static void draw_reel(SL* s, int r) {
    const int x = WIN_X + r * (CELL_W + 1);
    int top_idx = s->pos[r], off = 0;
    if(s->rstate[r] == R_SPIN || s->rstate[r] == R_STOPPING) {
        const uint32_t sp =
            (s->clock + (uint32_t)r * 130u) * (uint32_t)(CELL_H * (10 + 2 * r)) / (SYM_MS * 10u);
        top_idx = (int)((uint32_t)(SL_N * 1000) - sp / CELL_H) % SL_N;
        off = (int)(sp % CELL_H);
    } else if(s->rstate[r] == R_BOUNCE) {
        off = s->rt[r] < BOUNCE_MS / 2 ? 3 : 1;
    }
    for(int j = -1; j < 3; j++) {
        const int idx = (top_idx + j + SL_N * 2) % SL_N;
        sym_rows(
            SL_STRIP[idx],
            x + (CELL_W - SYM_W) / 2,
            WIN_Y + j * CELL_H + off + (CELL_H - SYM_W) / 2,
            WIN_Y,
            WIN_Y + WIN_H);
    }
}

static void draw_line(
    int k) { // a paying line, flashing: a frame around a row / a diagonal across the window
    if(k < 3) {
        static const int8_t ROWY[3] = {1, 0, 2};
        ov_frame(WIN_X - 1, WIN_Y + ROWY[k] * CELL_H, WIN_W + 2, CELL_H, 1);
        return;
    }
    const int rows = WIN_H, x0 = WIN_X, w = WIN_W;
    for(int i = 0; i < rows; i++) {
        const int x = x0 + i * w / rows, y = k == 3 ? WIN_Y + i : WIN_Y + rows - 1 - i;
        ov_fill(x, y, 2, 1, 1);
    }
}

static void draw_num(uint32_t v, int right, int y) {
    tchar t[8];
    put_num(t, v);
    font_draw_text(t, right - font_text_width(t), y);
}

static void s_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 1);
}

static void s_draw(SceneCtx* c, void* d) {
    (void)c;
    SL* s = d;
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    // the reel window
    ov_fill(WIN_X - 3, WIN_Y - 3, WIN_W + 6, WIN_H + 6, 1);
    ov_fill(WIN_X, WIN_Y, WIN_W, WIN_H, 0);
    ov_fill(WIN_X + CELL_W, WIN_Y, 1, WIN_H, 1);
    ov_fill(WIN_X + 2 * CELL_W + 1, WIN_Y, 1, WIN_H, 1);
    for(int r = 0; r < 3; r++)
        draw_reel(s, r);
    if(s->mode == M_WIN && ((s->t / FLASH_MS) & 1))
        for(int k = 0; k < SL_LINES; k++)
            if(s->lines & (1u << k)) draw_line(k);
    // the lines in play: an arrow at the left of each row (the diagonals: a slash at the right edge), as many as the bet pays
    static const int8_t ROWY[3] = {1, 0, 2}; // middle, top, bottom
    const uint8_t lines = slots_lines_for_bet(s->bet);
    for(int k = 0; k < 3; k++)
        if(lines & (1u << k)) {
            const int y = WIN_Y + ROWY[k] * CELL_H + CELL_H / 2 - 2;
            ov_fill(WIN_X - 9, y, 4, 5, 0);
            for(int i = 0; i < 3; i++)
                ov_fill(WIN_X - 9 + i, y + i, 1, 5 - 2 * i, 1);
        }
    if(lines & 0x18) { // the two diagonals: a slash and a backslash at the right of the window
        ov_fill(WIN_X + WIN_W + 4, WIN_Y + 18, 6, 6, 0);
        ov_fill(WIN_X + WIN_W + 4, WIN_Y + 26, 6, 6, 0);
        for(int i = 0; i < 6; i++) {
            ov_fill(WIN_X + WIN_W + 4 + i, WIN_Y + 18 + i, 1, 1, 1);
            ov_fill(WIN_X + WIN_W + 4 + i, WIN_Y + 31 - i, 1, 1, 1);
        }
    }
    // coins (top left) and the bet (coins stacked below)
    ov_bitmap(2, 3, 8, 8, COIN, 1, 1);
    draw_num(casino_coins(), 27, 13);
    for(int i = 0; i < s->bet; i++)
        ov_bitmap(3, 36 - i * 9 + 18, 8, 8, COIN, 1, 1);
    if(s->mode != M_SPIN) {
        ov_bitmap(17, 33, 5, 3, UPA, 1, 1);
        ov_bitmap(17, 45, 5, 3, DNA, 1, 1);
    }
    // the last win (top right) and the key hint (bottom right): OK = spin / stop
    ov_bitmap(112, 3, 7, 7, STAR, 1, 1);
    if(s->win) draw_num(s->win, 126, 13);
    ov_fill(112, 44, 9, 9, 0);
    ov_bitmap(112, 44, 9, 9, OKB, 2, 1);
    if(s->mode == M_SPIN)
        ov_fill(115, 47, 3, 3, 1); // stop: a square
    else
        for(int i = 0; i < 3; i++)
            ov_fill(115 + i, 46 + i, 1, 5 - 2 * i, 1); // spin: a triangle
}

static void s_enter(SceneCtx* c, void* d) {
    SL* s = d;
    text_prefetch(DIGITS);
    bet_clamp(s);
    for(int r = 0; r < 3; r++)
        s->pos[r] = (uint8_t)(r * 6 % SL_N);
    scene_dirty(c, D_WORLD);
}

static void s_leave(SceneCtx* c, void* d) {
    (void)c;
    if(g == d) g = NULL;
    free(d);
}

const SceneOps scene_slots_ops = {
    .enter = s_enter,
    .leave = s_leave,
    .input = s_input,
    .update = s_update,
    .draw_world = s_draw_world,
    .draw_overlay = s_draw,
    .pauses_below = true,
};

bool scene_slots_open(SceneCtx* c) {
    if(g || c->n + 2 > MAX_SCENES || casino_coins() == 0) return false;
    SL* s = calloc(1, sizeof(SL));
    if(!s) return false;
    s->bet = 3;
    g = s;
    if(!scene_push(c, &scene_slots_ops, s)) {
        g = NULL;
        free(s);
        return false;
    }
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_slots_mode(void) {
    return g ? g->mode : -1;
}
int scene_slots_bet(void) {
    return g ? g->bet : -1;
}
int scene_slots_pos(int r) {
    return g ? g->pos[r] : -1;
}
int scene_slots_rstate(int r) {
    return g ? g->rstate[r] : -1;
}
int scene_slots_win(void) {
    return g ? g->win : -1;
}
int scene_slots_lines(void) {
    return g ? g->lines : -1;
}
#endif
