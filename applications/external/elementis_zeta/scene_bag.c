// BAG scene (contract section 15), the menu plugin's second screen. START -> ITEM: a framed white screen with the item
// list (name, tiny "x5", up to 20 kinds, scroll arrows, a last "Cancelar" row) -> OK -> a 3-row box (Usar / Tirar / Cancelar).
// Usar: a healing item or a status cure opens the party list as its target (scene_party_open_use: the HP bar drains, gl.bt.healed /
// the cure message), a catch item or a key item says gl.bag.not_now (a Crystal is thrown in battle only). C2-S1 (contract 29): a Ward Spray
// starts its step counter (script variable VAR_REPEL), an Exit Wisp warps to the last Centro in a cave map (else gl.bag.no_here), a technique disc
// shows its boot text and opens the party list as its target (the teaching flow lives in scene_party.c). C4-B (contract 36): an evolution stone and a vitamin open the
// party list as their target too (scene_party.c stone_on / vit_on), the key item `bici` mounts / dismounts the bicycle (world_bike_toggle, its text, and the menus close after
// a success). Tirar on a key item says
// gl.bag.cant_toss. Tirar: the quantity picker (Up / Down +-1, wraps; Left / Right +-10),
// then a YES/NO question; YES takes the items out of the bag. Messages are dialogs above the screen.
// Texts are keys (gl.bag.*, gl.it.*, gl.party.cancel, gl.bt.*); the counts use the tiny font (hud_gfx.c) like the battle's bag.
#include <stdlib.h>
#include <string.h>
#include "scene_bag.h"
#include "scene_party.h"
#include "party.h"
#include "world.h"
#include "bag.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "hud_gfx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"

#define CAP       16 // codes of a name / label
#define VIS_MAX   8 // rows kept loaded (the screen shows (64 - 4) / pitch: 5 / 6 / 5)
#define LIST_X_bg 3 // cursor x
#define COUNT_R   116 // right edge of the tiny xN (the scroll arrows live at 120..124)

enum {
    B_LIST = 0,
    B_ACT,
    B_QTY
};
enum {
    W_NONE_bg = 0,
    W_LIST,
    W_ASK,
    W_DISC,
    W_QUIT
}; // after the child above closes: back to the list / the YES/NO answer is in `ask` / the disc's boot text is done: open the party list / (C4-B) the bicycle's text is read: close the bag and the pause menu
#define N_ACT 3

typedef struct {
    uint8_t mode, cur, top, act, qty, max, vis, wait, ask, item;
    uint8_t mods,
        nmods; // C3-A (contract 32): bit k = bracelet module k owned (World flag MOD_CORTADOR + k), and how many: listed after the items as key-item rows
    int16_t ox, oy, ow, oh; // Usar / Tirar / Cancelar box
    tchar lab[VIS_MAX][CAP]; // names of rows top .. top + vis - 1
    tchar l_use[CAP], l_toss[CAP], l_cancel[CAP];
} BS;

static BS* g_bg;

bool scene_bag_active(void) {
    return g_bg != NULL;
}

static void ld_bg(tchar* d, uint16_t id) {
    if(id == 0xFFFF || text_load(id, d, CAP) < 0) d[0] = 0;
}

#define MODROW \
    0xE0 // item_at() answers MODROW + k for the row of bracelet module k (never a real item id: items.toml has far fewer than 224)
_Static_assert(
    TXT_GL_C3A_MOD_PLANEADOR - TXT_GL_C3A_MOD_CORTADOR == MOD_COUNT - 1,
    "the module names are consecutive texts");

static uint16_t it_txt(uint8_t it) {
    if(it >= MODROW) return (uint16_t)(TXT_GL_C3A_MOD_CORTADOR + it - MODROW);
    return reg_it_txt(it);
}

static const tchar DIGITS_bg[] =
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0}; // quantity picker (zh streams its glyphs)

static int rows(void) { // item kinds + owned modules + Cancelar
    return bag_kinds() + g_bg->nmods + 1;
}

static uint8_t item_at(int row, uint8_t* count) { // 0 = the Cancelar row
    uint8_t it = 0, n = 0;
    int r = bag_kinds();
    for(uint8_t k = 0; k < MOD_COUNT && row >= r; k++)
        if((g_bg->mods >> k) & 1 && r++ == row) return (uint8_t)(MODROW + k);
    if(row >= 0 && row < bag_kinds() && bag_get((uint8_t)row, &it, &n)) {
        if(count) *count = n;
        return it;
    }
    return 0;
}

// Names of the visible rows (the bag may have changed) + their glyphs.
static void load_rows(BS* s) {
    const tchar* all[VIS_MAX + 4];
    int n = 0;
    for(int v = 0; v < s->vis; v++) {
        uint8_t it = item_at(s->top + v, NULL);
        s->lab[v][0] = 0;
        if(it) ld_bg(s->lab[v], it_txt(it));
        all[n++] = s->lab[v];
    }
    all[n++] = s->l_cancel;
    all[n++] = s->l_use;
    all[n++] = s->l_toss;
    all[n++] = DIGITS_bg;
    text_prefetch_many(all, n);
}

static void clamp(BS* s) {
    int n = rows();
    if(s->cur >= n) s->cur = (uint8_t)(n - 1);
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
    if(n <= s->vis)
        s->top = 0;
    else if(s->top + s->vis > n)
        s->top = (uint8_t)(n - s->vis);
    load_rows(s);
}

// ---------------------------------------------------------------------------------------------------- scene
static void b_enter(SceneCtx* c, void* d) {
    BS* s = d;
    for(uint8_t k = 0; k < MOD_COUNT; k++)
        if(world_module_has(c->world, (uint8_t)(MOD_CORTADOR + k))) {
            s->mods |= (uint8_t)(1u << k);
            s->nmods++;
        }
    ld_bg(s->l_use, TXT_GL_BAG_USE);
    ld_bg(s->l_toss, TXT_GL_BAG_TOSS);
    ld_bg(s->l_cancel, TXT_GL_PARTY_CANCEL);
    int p = font_pitch();
    s->vis = (int16_t)((SCREEN_H - 4) / p);
    if(s->vis > VIS_MAX) s->vis = VIS_MAX;
    if(s->vis < 1) s->vis = 1;
    int w = font_text_width(s->l_use), w2 = font_text_width(s->l_toss),
        w3 = font_text_width(s->l_cancel);
    if(w2 > w) w = w2;
    if(w3 > w) w = w3;
    s->ow = (int16_t)(font_cell_w() + 9 + w + 4);
    s->oh = (int16_t)(N_ACT * p + 4);
    s->ox = (int16_t)(SCREEN_W - s->ow);
    s->oy = (int16_t)(SCREEN_H - s->oh);
    clamp(s);
    scene_dirty(c, D_WORLD);
}

static void b_leave(SceneCtx* c, void* d) {
    (void)c;
    if(g_bg == d) g_bg = NULL;
    free(d);
}

static void close_screen_bg(SceneCtx* c) {
    scene_pop(c);
    scene_wake(c, 1);
}

static void say_bg(SceneCtx* c, BS* s, uint16_t id, uint8_t after) {
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) s->wait = W_NONE_bg;
}

// The child above (party list, dialog, YES/NO) has closed.
static void resume_bg(SceneCtx* c, BS* s) {
    uint8_t a = s->wait;
    s->wait = W_NONE_bg;
    if(a == W_ASK && s->ask) { // YES: take them out of the bag
        int n = s->qty;
        while(n-- > 0)
            bag_take(s->item);
        text_set_var_id(0, it_txt(s->item));
        text_set_var_num(2, s->qty);
        s->mode = B_LIST;
        clamp(s);
        say_bg(c, s, TXT_GL_BAG_TOSSED, W_LIST);
        scene_dirty(c, D_WORLD);
        return;
    }
    if(a == W_QUIT) {
        while(c->n > 1)
            scene_pop(
                c); // the bag (its state is freed here: `s` is not touched again) and the pause menu
        scene_wake(c, 1);
        return;
    }
    if(a ==
       W_DISC) { // the boot text of a technique disc is read: the party list picks the creature
        s->wait = W_LIST;
        if(scene_party_open_use(c, s->item)) return;
        s->wait = W_NONE_bg;
    }
    s->mode = B_LIST;
    clamp(s);
    scene_dirty(c, D_WORLD);
}

static bool is_key(uint8_t it) {
    const BcData* bd = party_data();
    return it >= MODROW || (bd && it < bd->n_items && bd->items[it].kind == IK_KEY);
}

static void do_use(SceneCtx* c, BS* s) {
    const BcData* bd = party_data();
    if(s->item >=
       MODROW) { // a module is used in the world, facing its obstacle; the Planeador (C6, contract 46) flies from here: world_fly() arms the list of towns (PC plugin) for the next world update
        if(s->item == MODROW + MOD_PLANEADOR - MOD_CORTADOR) {
            const uint8_t r =
                party_state()->rv_steps ?
                    FLY_NOT_HERE :
                    world_fly(); // (not inside a Reserva Salvaje session: no Fly in the Reserva Salvaje)
            if(r == FLY_OK) {
                s->wait =
                    W_QUIT; // the bag and the pause menu close; the list of towns opens on top of the world
                if(scene_top_ops(c) == &scene_bag_ops) resume_bg(c, s);
                return;
            }
            say_bg(c, s, r == FLY_NO_BADGE ? TXT_GL_C6_FLY_BADGE : TXT_GL_BAG_NO_HERE, W_LIST);
            return;
        }
        say_bg(c, s, TXT_GL_C3A_MOD_HINT, W_LIST);
        return;
    }
    uint8_t it = s->item, kind = bd && it < bd->n_items ? bd->items[it].kind : 0;
    if((kind == IK_HEAL || kind == IK_CURE || kind == IK_STONE || kind == IK_VITAMIN ||
        (kind >= IK_REVIVE && kind != IK_BATTLE)) &&
       party_player()->count >
           0) { // C4-B: an evolution stone and a vitamin also pick a creature; contract 64: Life Spark / Vital Draught / Growth Seed / Vigor Drop (kinds 10 ..)
        s->wait = W_LIST;
        if(scene_party_open_use(c, it)) return;
        s->wait = W_NONE_bg;
    } else if(kind == IK_DISC && party_player()->count > 0) {
        text_set_var_id(1, reg_mv_txt(bd->items[it].arg));
        say_bg(c, s, TXT_GL_DISC_BOOT, W_DISC);
        return;
    } else if(kind == IK_REPEL) { // the counter restarts (using another one while it runs refreshes it)
        if(bag_take(it)) {
            c->world->vars[VAR_REPEL] = bd->items[it].arg;
            text_set_var_id(0, it_txt(it));
            say_bg(c, s, TXT_GL_BAG_USED, W_LIST);
            return;
        }
    } else if(
        it ==
        IT_BICI) { // C4-B (contract 35.4 / 36): Usar on the bicycle = mount / dismount (world_bike_toggle); its text (on / off / refused) is shown, and after a success the bag and the pause menu close
        const bool ok =
            world_bike_toggle(); // (A's world_bike_msg() would be a second table entry: the message follows from the flag the toggle flipped)
        const bool on = world_module_has(c->world, FLAG_BIKE); // (inline: reads the flag bit)
        say_bg(
            c,
            s,
            ok ? (on ? TXT_GL_C4A_BIKE_ON : TXT_GL_C4A_BIKE_OFF) : TXT_GL_C4A_BIKE_NO,
            ok ? W_QUIT : W_LIST);
        return;
    } else if(
        it >= IT_CANA_JUNCO &&
        it <=
            IT_CANA_ALEACION) { // C5-B (contract 43): a fishing rod. Usar = world_fish(0 / 1 / 2) (world.h; it must ARM the fishing, not open the bag's scenes)
        if(world_fish((uint8_t)(it - IT_CANA_JUNCO))) {
            s->wait =
                W_QUIT; // the bag and the pause menu close once whatever world_fish pushed (nothing, or its dialogs) is done
            if(scene_top_ops(c) == &scene_bag_ops) resume_bg(c, s);
            return;
        }
        say_bg(c, s, TXT_GL_BAG_NOT_NOW, W_LIST); // not facing water
        return;
    } else if(
        it == IT_BUSCATESOROS ||
        it ==
            IT_ATLAS) { // F2 (contract 74): the hidden-item detector and the region map: the casino plugin does them at the next world update (World.act, like a rod); the bag and the pause menu close first
        c->world->act = it == IT_ATLAS ? WACT_REGMAP : WACT_DETECT;
        s->wait = W_QUIT;
        if(scene_top_ops(c) == &scene_bag_ops) resume_bg(c, s);
        return;
    } else if(
        it ==
        IT_FLAUTA) { // contract 81: the flute played outside a battle wakes every sleeping creature of the party (its use on a sleeping giant is the map's own script)
        Party* p = party_player();
        int woke = 0;
        for(int k = 0; k < p->count; k++) {
            woke |= p->mon[k].status & BS_SLP_MASK;
            p->mon[k].status &= (uint8_t)~BS_SLP_MASK;
        }
        say_bg(c, s, woke ? TXT_GL_MISC_FLUTE_WOKE : TXT_GL_MISC_FLUTE_NONE, W_LIST);
        return;
    } else if(
        kind == IK_ROPE &&
        it !=
            IT_PELUCHE_FUGA) { // (the Peluche Fuga is an escape item of the battle's bag: here it says "not now")
        if(!(c->world->map.flags & MAPF_CAVE)) {
            say_bg(c, s, TXT_GL_BAG_NO_HERE, W_LIST);
            return;
        }
        if(bag_take(it)) {
            scene_escape(
                c); // the bag (its state is freed here: nothing of `s` is touched again) and the pause menu close, the fade takes him to the last Centro
            return;
        }
    }
    say_bg(
        c,
        s,
        it == IT_PIEDRA_ECLIPSE ? TXT_GL_B3_ECL_STONE : TXT_GL_BAG_NOT_NOW,
        W_LIST); // a Crystal is thrown in battle only; a key item is never used (the Eclipse Stone says how it is used, contract 67)
}

static void b_input(SceneCtx* c, void* d, const InputEv* ev) {
    BS* s = d;
    if(s->wait) resume_bg(c, s);
    if(scene_top_ops(c) != &scene_bag_ops) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT) {
            if(s->mode == B_LIST)
                close_screen_bg(c);
            else {
                s->mode = B_LIST;
                scene_dirty(c, D_COMPOSE);
            }
        }
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    const bool up = ev->key == KEY_UP, down = ev->key == KEY_DOWN;
    if(s->mode == B_LIST) {
        const int n = rows();
        if(up || down) {
            s->cur = (uint8_t)((s->cur + (down ? 1 : n - 1)) % n);
            clamp(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            s->item = item_at(s->cur, &s->max);
            if(!s->item)
                close_screen_bg(c); // Cancelar
            else {
                s->mode = B_ACT;
                s->act = 0;
                scene_dirty(c, D_COMPOSE);
            }
        }
    } else if(s->mode == B_ACT) {
        if(up || down) {
            s->act = (uint8_t)((s->act + (down ? 1 : N_ACT - 1)) % N_ACT);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->act == 0)
                do_use(c, s);
            else if(s->act == 1 && is_key(s->item)) {
                s->mode = B_LIST;
                say_bg(c, s, TXT_GL_BAG_CANT_TOSS, W_LIST); // a key item is never tossed
                scene_dirty(c, D_COMPOSE);
            } else if(s->act == 1) {
                s->mode = B_QTY;
                s->qty = 1;
                scene_dirty(c, D_COMPOSE);
            } else {
                s->mode = B_LIST;
                scene_dirty(c, D_COMPOSE);
            }
        }
    } else { // quantity
        const int max = s->max;
        int q = s->qty;
        if(up || down)
            q = up ? (q >= max ? 1 : q + 1) : (q <= 1 ? max : q - 1); // wraps
        else if(ev->key == KEY_RIGHT)
            q = q + 10 > max ? max : q + 10;
        else if(ev->key == KEY_LEFT)
            q = q - 10 < 1 ? 1 : q - 10;
        else if(ev->key == KEY_OK && press) {
            text_set_var_id(0, it_txt(s->item));
            text_set_var_num(2, s->qty);
            s->ask = 0;
            s->wait = W_ASK;
            if(!scene_ask_open(c, TXT_GL_BAG_TOSS_ASK, &s->ask)) {
                s->wait = W_NONE_bg;
                s->mode = B_LIST;
                scene_dirty(c, D_COMPOSE);
            }
            return;
        }
        if(q != s->qty) {
            s->qty = (uint8_t)q;
            scene_dirty(c, D_COMPOSE);
        }
    }
}

static void b_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    BS* s = d;
    if(s->wait) resume_bg(c, s);
}

// ---------------------------------------------------------------------------------------------------- drawing
static void draw_world_bg(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static void draw_box(int x, int y, int w, int h) {
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
}

static void draw_list_bg(BS* s) {
    const int p = font_pitch(), n = rows(), nx = LIST_X_bg + font_cell_w() + 2;
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    for(int v = 0; v < s->vis && s->top + v < n; v++) {
        const int k = s->top + v, y = 2 + v * p;
        uint8_t cnt = 0;
        const uint8_t it = item_at(k, &cnt);
        if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X_bg, y, 1);
        font_draw_text(it ? s->lab[v] : s->l_cancel, nx, y);
        if(it && it < MODROW) {
            char t[8] = "x";
            hud_num(t + 1, cnt);
            tiny_draw(t, COUNT_R - tiny_width(t), y + font_baseline() - 5);
        }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(121, 3, 5, 3, UP, 1, 1);
    if(s->top + s->vis < n) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
}

static void draw_act_box(BS* s) {
    const int p = font_pitch();
    draw_box(s->ox, s->oy, s->ow, s->oh);
    const tchar* lab[N_ACT] = {s->l_use, s->l_toss, s->l_cancel};
    for(int i = 0; i < N_ACT; i++) {
        int y = s->oy + 2 + i * p;
        if(s->act == i) font_draw_glyph(GLYPH_CURSOR, s->ox + 3, y, 1);
        font_draw_text(lab[i], s->ox + 3 + font_cell_w() + 2, y);
    }
}

// "x05" with a pair of small arrows: how many to toss.
static void draw_qty_box(BS* s) {
    const int p = font_pitch(), adv = font_adv('0');
    const int w = 4 + 3 + 2 + 2 * adv + 4 + 5 + 3, h = p + 4;
    const int x = SCREEN_W - w, y = SCREEN_H - h;
    draw_box(x, y, w, h);
    tiny_draw("x", x + 4, y + 2 + font_baseline() - 5);
    int dx = x + 4 + 3 + 2;
    font_draw_glyph((tchar)('0' + s->qty / 10), dx, y + 2, 1);
    font_draw_glyph((tchar)('0' + s->qty % 10), dx + adv, y + 2, 1);
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    ov_bitmap(x + w - 9, y + 3, 5, 3, UP, 1, 1);
    ov_bitmap(x + w - 9, y + h - 6, 5, 3, DN, 1, 1);
}

static void draw_overlay_bg(SceneCtx* c, void* d) {
    (void)c;
    BS* s = d;
    draw_list_bg(s);
    if(s->mode == B_ACT)
        draw_act_box(s);
    else if(s->mode == B_QTY)
        draw_qty_box(s);
}

const SceneOps scene_bag_ops = {
    .enter = b_enter,
    .leave = b_leave,
    .input = b_input,
    .update = b_update,
    .draw_world = draw_world_bg,
    .draw_overlay = draw_overlay_bg,
    .pauses_below = true,
};

bool scene_bag_open(SceneCtx* c) {
    if(g_bg || c->n >= MAX_SCENES) return false;
    BS* s = calloc(1, sizeof(BS));
    if(!s) return false;
    g_bg = s;
    if(!scene_push(c, &scene_bag_ops, s)) {
        g_bg = NULL;
        free(s);
        return false;
    }
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_bag_mode(void) {
    return g_bg ? g_bg->mode : -1;
}
int scene_bag_cur(void) {
    return g_bg ? g_bg->cur : -1;
}
int scene_bag_top(void) {
    return g_bg ? g_bg->top : -1;
}
int scene_bag_qty(void) {
    return g_bg ? g_bg->qty : -1;
}
int scene_bag_rows(void) {
    return g_bg ? rows() : -1;
}
int scene_bag_row_item(int row) {
    return g_bg ? item_at(row, NULL) : -1;
}
void scene_bag_heap(uint32_t* bytes, uint32_t* blocks) {
    *bytes = *blocks = 0;
    if(!g_bg) return;
    *bytes = sizeof(BS);
    *blocks = 1;
}
#endif
