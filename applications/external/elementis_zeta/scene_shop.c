// SHOP scene (contract section 17), the menu plugin's third screen. mart: greeting, then the box Comprar / Vender / Salir.
// BUY: the shop's items with their prices (items.bin) -> OK -> quantity (Up / Down +-1 wrapping, Left / Right +-10) -> YES/NO question
// -> money_take + bag_add. SELL: the bag; the price is half, key items / unpriced items are refused. Messages and the question
// are dialogs above the screen (the bag screen's pattern: `wait` says what to do when the child above has closed).
// Money is shown as "$N" by ONE helper (fmt_money, CUR_SYM): the screen and the {v3} variable of gl.shop.deal both use it.
#include <stdlib.h>
#include <string.h>
#include "scene_shop.h"
#include "party.h"
#include "bag.h"
#include "battle_core.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "hud_gfx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "assets.h"

#define CAP        16 // codes of a name / label
#define MONEY_CAP  12 // "$999999" + END
#define VIS_MAX_sh 6 // rows kept loaded (the list shows (64 - 2 - header) / pitch: 4 / 4 / 3)
#define LIST_X_sh  3 // cursor x
#define RIGHT_R    118 // right edge of a price / tiny xN (the scroll arrows live at 120..124)
#define CUR_SYM \
    '$' // the currency sign (the single place that decides how money looks; texts get it through {v3})

enum {
    M_MAIN = 0,
    M_BUY,
    M_SELL,
    M_QTY
};
// what to do when the child above (dialog, YES/NO) has closed
enum {
    W_NONE_sh = 0,
    W_MAIN,
    W_LIST_sh,
    W_ASK_sh,
    W_BYE
};

typedef struct {
    uint8_t mode, from, cur, top, qty, max, vis, wait, ask, item, main_cur, n_stock;
    uint8_t stock[SHOP_MAX_ITEMS];
    tchar lab[VIS_MAX_sh][CAP]; // names of rows top .. top + vis - 1
    uint16_t
        prc[VIS_MAX_sh]; // ... and their prices (read from items.bin when the rows are loaded: the device's item table keeps no price, contract 64)
    tchar l_buy[CAP], l_sell[CAP], l_quit[CAP], l_cancel[CAP], l_item[CAP];
} SS;

static SS* g_sh;

bool scene_shop_active(void) {
    return g_sh != NULL;
}

static const tchar GLYPHS[] = {
    CUR_SYM,
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'x',
    0}; // prefetched (zh streams its glyphs)

// ---------------------------------------------------------------------------------------------------- data
static void ld_sh(tchar* d, uint16_t id) {
    if(id == 0xFFFF || text_load(id, d, CAP) < 0) d[0] = 0;
}

static uint16_t it_txt_sh(uint8_t it) {
    return reg_it_txt(it);
}

// "$1234" as codes (END terminated, out needs MONEY_CAP): the one place that decides how an amount looks.
static int fmt_money(tchar* out, uint32_t v) {
    char t[12];
    int n = hud_num(t, (int)v);
    out[0] = CUR_SYM;
    for(int i = 0; i < n; i++)
        out[1 + i] = (tchar)t[i];
    out[n + 1] = 0;
    return n + 1;
}

static const ItemDef* item_def(uint8_t it) {
    const BcData* bd = party_data();
    return (bd && it < bd->n_items) ? &bd->items[it] : NULL;
}

static uint32_t
    buy_price(uint8_t it) { // the price is the u16 at byte 4 of the item's record (items.bin)
    const ItemDef* d = item_def(it);
    uint8_t b[2];
    return d && d->kind != IK_KEY && read_at("items.bin", 12u + it * 8u + 4, b, 2) ?
               (uint32_t)(b[0] | b[1] << 8) :
               0;
}

static uint32_t sell_price(uint8_t it) { // half; 0 = cannot be sold
    return buy_price(it) / 2;
}

static int rows_sh(const SS* s) { // list rows: items + Cancelar
    return (s->mode == M_SELL || (s->mode == M_QTY && s->from == M_SELL) ? (int)bag_kinds() :
                                                                           (int)s->n_stock) +
           1;
}

// item of list row `row` (0 = the Cancelar row); `count` = how many the bag holds (sell list)
static uint8_t item_at_sh(const SS* s, int row, uint8_t* count) {
    uint8_t it = 0, n = 0;
    const uint8_t list_mode = s->mode == M_QTY ? s->from : s->mode;
    if(list_mode == M_SELL) {
        if(row >= 0 && row < bag_kinds() && bag_get((uint8_t)row, &it, &n)) {
            if(count) *count = n;
            return it;
        }
        return 0;
    }
    if(row >= 0 && row < s->n_stock) return s->stock[row];
    return 0;
}

// Names of the visible rows (the bag may have changed) + the glyphs the screen draws.
static void load_rows_sh(SS* s) {
    const tchar* all[VIS_MAX_sh + 6];
    int n = 0;
    for(int v = 0; v < s->vis; v++) {
        uint8_t it = item_at_sh(s, s->top + v, NULL);
        s->lab[v][0] = 0;
        s->prc[v] = it ? (uint16_t)buy_price(it) : 0;
        if(it) ld_sh(s->lab[v], it_txt_sh(it));
        all[n++] = s->lab[v];
    }
    all[n++] = s->l_cancel;
    all[n++] = s->l_buy;
    all[n++] = s->l_sell;
    all[n++] = s->l_quit;
    all[n++] = s->l_item;
    all[n++] = GLYPHS;
    text_prefetch_many(all, n);
}

static void clamp_sh(SS* s) {
    int n = rows_sh(s);
    if(s->cur >= n) s->cur = (uint8_t)(n - 1);
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
    if(n <= s->vis)
        s->top = 0;
    else if(s->top + s->vis > n)
        s->top = (uint8_t)(n - s->vis);
    load_rows_sh(s);
}

// ---------------------------------------------------------------------------------------------------- scene
static int list_y0(void) { // first list row: below the header (title + money) and its rule
    return font_pitch() + 5;
}

static void s_enter(SceneCtx* c, void* d) {
    SS* s = d;
    ld_sh(s->l_buy, TXT_GL_SHOP_BUY);
    ld_sh(s->l_sell, TXT_GL_SHOP_SELL);
    ld_sh(s->l_quit, TXT_GL_SHOP_QUIT);
    ld_sh(s->l_cancel, TXT_GL_PARTY_CANCEL);
    int p = font_pitch();
    s->vis = (uint8_t)((SCREEN_H - 2 - list_y0()) / p);
    if(s->vis > VIS_MAX_sh) s->vis = VIS_MAX_sh;
    if(s->vis < 1) s->vis = 1;
    s->mode = M_MAIN;
    load_rows_sh(s);
    scene_dirty(c, D_WORLD);
}

static void s_leave(SceneCtx* c, void* d) {
    (void)c;
    if(g_sh == d) g_sh = NULL;
    free(d);
}

static void close_screen_sh(SceneCtx* c) {
    scene_pop(c);
    scene_wake(c, 1);
}

#ifndef NUCLEO_DEVICE
static uint16_t g_last_msg; // host tests: the text id of the last message the screen showed
#endif

static void say_sh(SceneCtx* c, SS* s, uint16_t id, uint8_t after) {
#ifndef NUCLEO_DEVICE
    g_last_msg = id;
#endif
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) { // no dialog: carry on as if it had been read
        s->wait = W_NONE_sh;
        if(after == W_BYE)
            close_screen_sh(c);
        else if(after == W_MAIN)
            s->mode = M_MAIN;
        else if(after == W_LIST_sh)
            s->mode = s->from;
    }
}

static void go_main(SS* s) {
    s->mode = M_MAIN;
}

static void go_list(SS* s, uint8_t list_mode) {
    s->mode = list_mode;
    clamp_sh(s);
}

static void bye(SceneCtx* c, SS* s) {
    say_sh(c, s, TXT_GL_SHOP_BYE, W_BYE);
}

static void quote_vars(SS* s, uint32_t total) { // the variables of gl.shop.deal
    tchar m[MONEY_CAP];
    text_set_var_id(0, it_txt_sh(s->item));
    text_set_var_num(2, s->qty);
    fmt_money(m, total);
    text_set_var(3, m);
}

// YES to the question: do the deal.
static void finish_deal(SceneCtx* c, SS* s) {
    const uint8_t it = s->item, n = s->qty;
    if(s->from == M_BUY) {
        const uint32_t total = buy_price(it) * n;
        const uint8_t held = bag_count(it);
        const uint8_t room = held ? (uint8_t)(BAG_MAX_COUNT - held) :
                                    (bag_kinds() < BAG_MAX_KINDS ? BAG_MAX_COUNT : 0);
        if(money_get() < total) {
            say_sh(c, s, TXT_GL_SHOP_NO_MONEY, W_LIST_sh);
        } else if(room < n) {
            say_sh(c, s, TXT_GL_SHOP_BAG_FULL, W_LIST_sh);
        } else if(money_take(total) && bag_add(it, n) == n) {
            say_sh(c, s, TXT_GL_SHOP_BOUGHT, W_MAIN);
        } else { // cannot happen (checked above); never lose money for items that did not arrive
            say_sh(c, s, TXT_GL_SHOP_BAG_FULL, W_LIST_sh);
        }
    } else {
        const uint32_t unit = sell_price(it);
        for(uint8_t k = 0; k < n; k++)
            bag_take(it);
        money_add(unit * n);
        say_sh(c, s, TXT_GL_SHOP_SOLD, W_MAIN);
    }
    scene_dirty(c, D_COMPOSE);
}

// The child above (dialog, YES/NO) has closed.
static void resume_sh(SceneCtx* c, SS* s) {
    uint8_t a = s->wait;
    s->wait = W_NONE_sh;
    if(a == W_BYE) {
        close_screen_sh(c);
        return;
    }
    if(a == W_ASK_sh) {
        if(s->ask) {
            finish_deal(c, s);
            return;
        }
        go_list(s, s->from); // NO / Back: the list again, nothing happened
    } else if(a == W_LIST_sh) {
        go_list(s, s->from);
    } else {
        go_main(s); // W_MAIN: after the greeting / a deal
    }
    scene_dirty(c, D_WORLD);
}

static void pick_item(SceneCtx* c, SS* s, uint8_t list_mode) { // OK on an item row
    const uint8_t it = item_at_sh(s, s->cur, &s->max);
    s->item = it;
    s->from = list_mode;
    if(list_mode == M_BUY) {
        const uint32_t price = buy_price(it);
        const uint8_t held = bag_count(it);
        const uint8_t room = held ? (uint8_t)(BAG_MAX_COUNT - held) :
                                    (bag_kinds() < BAG_MAX_KINDS ? BAG_MAX_COUNT : 0);
        if(!price || money_get() < price) {
            say_sh(c, s, TXT_GL_SHOP_NO_MONEY, W_LIST_sh);
            return;
        }
        if(room == 0) {
            say_sh(c, s, TXT_GL_SHOP_BAG_FULL, W_LIST_sh);
            return;
        }
        s->max = room;
    } else if(!sell_price(it)) {
        say_sh(c, s, TXT_GL_SHOP_CANT_SELL, W_LIST_sh);
        return;
    }
    ld_sh(s->l_item, it_txt_sh(it));
    text_prefetch(s->l_item);
    s->mode = M_QTY;
    s->qty = 1;
    scene_dirty(c, D_COMPOSE);
}

static void ask_deal(SceneCtx* c, SS* s) {
    const uint32_t unit = s->from == M_BUY ? buy_price(s->item) : sell_price(s->item);
    quote_vars(s, unit * s->qty);
    s->ask = 0;
    s->wait = W_ASK_sh;
    if(!scene_ask_open(c, TXT_GL_SHOP_DEAL, &s->ask)) {
        s->wait = W_NONE_sh;
        go_list(s, s->from);
        scene_dirty(c, D_COMPOSE);
    }
}

static void s_input(SceneCtx* c, void* d, const InputEv* ev) {
    SS* s = d;
    if(s->wait) resume_sh(c, s);
    if(scene_top_ops(c) != &scene_shop_ops) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT) {
            if(s->mode == M_MAIN)
                bye(c, s);
            else if(s->mode == M_QTY)
                go_list(s, s->from);
            else
                go_main(s);
            scene_dirty(c, D_COMPOSE);
        }
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    const bool up = ev->key == KEY_UP, down = ev->key == KEY_DOWN;
    if(s->mode == M_MAIN) {
        if(up || down) {
            s->main_cur = (uint8_t)((s->main_cur + (down ? 1 : 2)) % 3);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->main_cur == 0) {
                s->cur = s->top = 0;
                go_list(s, M_BUY);
                scene_dirty(c, D_COMPOSE);
            } else if(s->main_cur == 1) {
                if(bag_kinds() == 0) {
                    say_sh(c, s, TXT_GL_BAG_EMPTY, W_MAIN);
                } else {
                    s->cur = s->top = 0;
                    go_list(s, M_SELL);
                    scene_dirty(c, D_COMPOSE);
                }
            } else {
                bye(c, s);
            }
        }
    } else if(s->mode == M_BUY || s->mode == M_SELL) {
        const int n = rows_sh(s);
        if(up || down) {
            s->cur = (uint8_t)((s->cur + (down ? 1 : n - 1)) % n);
            clamp_sh(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(item_at_sh(s, s->cur, NULL) == 0) { // Cancelar
                go_main(s);
                scene_dirty(c, D_COMPOSE);
            } else {
                pick_item(c, s, s->mode);
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
            ask_deal(c, s);
            return;
        }
        if(q != s->qty) {
            s->qty = (uint8_t)q;
            scene_dirty(c, D_COMPOSE);
        }
    }
}

static void s_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    SS* s = d;
    if(s->wait) resume_sh(c, s);
}

// ---------------------------------------------------------------------------------------------------- drawing
static void draw_world_sh(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static void draw_box_sh(int x, int y, int w, int h) {
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
}

// An item name next to a price on the same row (contract 72): drawn in at most `maxw` px; a longer one is cut and ends with the ellipsis glyph, so a name never runs under its price.
#ifndef NUCLEO_DEVICE
ShopGeom
    g_sh_geom; // host tests (contract 72): where the last frame put each name (x, drawn width) and its price (x)
#define SHG(f, v) (g_sh_geom.f = (v))
#else
#define SHG(f, v) ((void)(v)) // (v is always a plain variable: no code)
#endif
static int draw_name_fit(const tchar* s, int x, int y, int room) { // returns the width drawn
    const int x0 = x, ew = font_adv(GLYPH_ELLIPSIS);
    for(; *s; s++) {
        const int a = font_adv(*s);
        if(a + (s[1] ? ew : 0) > room)
            break; // does not fit (a glyph that is not the last one leaves the ellipsis its room)
        font_draw_glyph(*s, x, y, 1);
        x += a;
        room -= a;
    }
    if(*s) {
        font_draw_glyph(GLYPH_ELLIPSIS, x, y, 1);
        x += ew;
    }
    return x - x0;
}

// money, right aligned, text row at y (right edge x = r)
static void draw_money(int r, int y) {
    tchar m[MONEY_CAP];
    fmt_money(m, money_get());
    font_draw_text(m, r - font_text_width(m), y);
}

static void draw_header(const tchar* title) {
    const int p = font_pitch();
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    font_draw_text(title, LIST_X_sh, 2);
    draw_money(SCREEN_W - 3, 2);
    ov_fill(1, p + 3, SCREEN_W - 2, 1, 1);
}

static void draw_list_sh(SS* s, bool buy) {
    const int p = font_pitch(), n = rows_sh(s), nx = LIST_X_sh + font_cell_w() + 2, y0 = list_y0();
    draw_header(buy ? s->l_buy : s->l_sell);
#ifndef NUCLEO_DEVICE
    g_sh_geom.n = 0;
#endif
    for(int v = 0; v < s->vis && s->top + v < n; v++) {
        const int k = s->top + v, y = y0 + v * p;
        uint8_t cnt = 0;
        const uint8_t it = item_at_sh(s, k, &cnt);
        if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X_sh, y, 1);
        tchar m[MONEY_CAP];
        int px = RIGHT_R;
        if(it && buy) {
            fmt_money(m, s->prc[v]);
            px -= font_text_width(
                m); // the name gets the room left of its price (the glyphs' own blank column is the gap)
        }
        const int dw = draw_name_fit(it ? s->lab[v] : s->l_cancel, nx, y, px - nx);
        (void)dw;
        if(it && buy) {
            font_draw_text(m, px, y);
#ifndef NUCLEO_DEVICE
            if(g_sh_geom.n < 8)
                g_sh_geom.nx[g_sh_geom.n] = nx, g_sh_geom.nw[g_sh_geom.n] = dw,
                g_sh_geom.px[g_sh_geom.n] = px,
                g_sh_geom.full[g_sh_geom.n] = font_text_width(s->lab[v]), g_sh_geom.n++;
#endif
        } else if(it) {
            char t[8] = "x";
            hud_num(t + 1, cnt);
            tiny_draw(t, RIGHT_R - tiny_width(t), y + font_baseline() - 5);
        }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(121, y0 + 1, 5, 3, UP, 1, 1);
    if(s->top + s->vis < n) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
}

// The quantity picker: the item and its unit price, then "x05" with arrows and the total.
static void draw_qty(SS* s) {
    const int p = font_pitch(), adv = font_adv('0'), y0 = list_y0();
    const bool buy = s->from == M_BUY;
    const uint32_t unit = buy ? buy_price(s->item) : sell_price(s->item);
    tchar m[MONEY_CAP];
    draw_header(buy ? s->l_buy : s->l_sell);
    fmt_money(m, unit);
    const int qpx = RIGHT_R - font_text_width(m),
              qn = draw_name_fit(s->l_item, LIST_X_sh, y0, qpx - LIST_X_sh);
    font_draw_text(m, qpx, y0);
    SHG(qn, qn);
    SHG(qpx, qpx);
    const int y1 = y0 + p + 2;
    tiny_draw("x", LIST_X_sh, y1 + font_baseline() - 5);
    const int dx = LIST_X_sh + 3 + 2;
    font_draw_glyph((tchar)('0' + s->qty / 10), dx, y1, 1);
    font_draw_glyph((tchar)('0' + s->qty % 10), dx + adv, y1, 1);
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    ov_bitmap(dx + 2 * adv + 3, y1 + 1, 5, 3, UP, 1, 1);
    ov_bitmap(dx + 2 * adv + 3, y1 + p - 5, 5, 3, DN, 1, 1);
    fmt_money(m, unit * s->qty);
    font_draw_text(m, RIGHT_R - font_text_width(m), y1);
}

static void draw_main(SS* s) {
    const int p = font_pitch();
    tchar m[MONEY_CAP];
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    fmt_money(m, money_get());
    const int mw = font_text_width(m) + 8, mh = p + 4;
    draw_box_sh(SCREEN_W - mw, 0, mw, mh);
    font_draw_text(m, SCREEN_W - mw + 4, 2);
    if(!s->wait) {
        int w = font_text_width(s->l_buy), w2 = font_text_width(s->l_sell),
            w3 = font_text_width(s->l_quit);
        if(w2 > w) w = w2;
        if(w3 > w) w = w3;
        const int bw = font_cell_w() + 9 + w + 4, bh = 3 * p + 4;
        draw_box_sh(0, 0, bw, bh);
        const tchar* lab[3] = {s->l_buy, s->l_sell, s->l_quit};
        for(int i = 0; i < 3; i++) {
            int y = 2 + i * p;
            if(s->main_cur == i) font_draw_glyph(GLYPH_CURSOR, 3, y, 1);
            font_draw_text(lab[i], 3 + font_cell_w() + 2, y);
        }
    }
}

static void draw_overlay_sh(SceneCtx* c, void* d) {
    (void)c;
    SS* s = d;
    if(s->mode == M_MAIN)
        draw_main(s);
    else if(s->mode == M_QTY)
        draw_qty(s);
    else
        draw_list_sh(s, s->mode == M_BUY);
}

const SceneOps scene_shop_ops = {
    .enter = s_enter,
    .leave = s_leave,
    .input = s_input,
    .update = s_update,
    .draw_world = draw_world_sh,
    .draw_overlay = draw_overlay_sh,
    .pauses_below = true,
};

bool scene_shop_open(SceneCtx* c, const uint8_t* items, uint8_t n) {
    if(g_sh || c->n + 3 > MAX_SCENES || !items || n == 0 || n > SHOP_MAX_ITEMS)
        return false; // room for the dialog + YES/NO above us
    SS* s = calloc(1, sizeof(SS));
    if(!s) return false;
    memcpy(s->stock, items, n);
    s->n_stock = n;
    g_sh = s;
    if(!scene_push(c, &scene_shop_ops, s)) {
        g_sh = NULL;
        free(s);
        return false;
    }
    say_sh(c, s, TXT_GL_SHOP_HELLO, W_MAIN);
    return true;
}

// ---- the stock from shops.bin (section 17.1, tools/shopc.py). It lived in the resident app (natives_shop.c) until the C4 memory recovery (contract 38.3): only this screen needs it.
#define SHOPS_FILE    "shops.bin"
#define SHOPS_VERSION 1
static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool shop_load(uint8_t id, uint8_t* items, uint8_t cap, uint8_t* n) {
    uint8_t h[8], o[2], rec[1 + SHOP_MAX_ITEMS];
    if(cap < SHOP_MAX_ITEMS || !read_at(SHOPS_FILE, 0, h, sizeof(h))) return false;
    if(memcmp(h, "NSHP", 4) != 0 || h[4] != SHOPS_VERSION || id == 0 || id >= h[5]) return false;
    const uint16_t size = rd16(h + 6);
    if(size < 8u + 2u * h[5] || file_size(SHOPS_FILE) != (int32_t)size) return false;
    if(!read_at(SHOPS_FILE, 8u + 2u * id, o, 2)) return false;
    const uint16_t off = rd16(o);
    if(off < 8u + 2u * h[5] || off >= size) return false;
    if(!read_at(SHOPS_FILE, off, rec, 1)) return false;
    const uint8_t count = rec[0];
    if(count == 0 || count > SHOP_MAX_ITEMS || off + 1u + count > size) return false;
    if(!read_at(SHOPS_FILE, off + 1u, rec + 1, count)) return false;
    for(uint8_t i = 0; i < count; i++)
        if(rec[1 + i] == 0 || rec[1 + i] >= IT_COUNT) return false;
    memcpy(items, rec + 1, count);
    *n = count;
    return true;
}

// The menu plugin's entry for `shop ID` (MenuPlugin.open_shop): reads the stock of shop `id` and opens the screen. False = no such shop / the screen cannot open.
bool scene_shop_open_id(SceneCtx* c, uint8_t id) {
    uint8_t items[SHOP_MAX_ITEMS], n = 0;
    return shop_load(id, items, sizeof(items), &n) && scene_shop_open(c, items, n);
}

#ifndef NUCLEO_DEVICE
int scene_shop_mode(void) {
    return g_sh ? g_sh->mode : -1;
}
int scene_shop_cur(void) {
    return g_sh ? g_sh->cur : -1;
}
int scene_shop_top(void) {
    return g_sh ? g_sh->top : -1;
}
int scene_shop_qty(void) {
    return g_sh ? g_sh->qty : -1;
}
int scene_shop_main_cur(void) {
    return g_sh ? g_sh->main_cur : -1;
}
int scene_shop_last_msg(void) {
    return g_last_msg;
}
void scene_shop_heap(uint32_t* bytes, uint32_t* blocks) {
    *bytes = *blocks = 0;
    if(!g_sh) return;
    *bytes = sizeof(SS);
    *blocks = 1;
}
#endif
