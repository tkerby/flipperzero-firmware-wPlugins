// The PRIZE COUNTER (contract section 37), a scene of the CASINO plugin: a shop priced in COINS that sells creatures (given at a level through party_give) and discs. The stock is a
// `coins = true` table of assets/data/shops.toml (tools/shopc.py: shops.bin, 5-byte entries); a screen like the mart's BUY list: header (coin icon + coins), rows (name, tiny level
// for a creature, price in coins), Cancelar; OK -> the YES/NO question -> the deal. Back = a goodbye and out. The party-full / bag-full / not-enough-coins cases refuse WITHOUT charging.
#include <stdlib.h>
#include <string.h>
#include "casino.h"
#include "party.h"
#include "bag.h"
#include "battle_core.h"
#include "assets.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "hud_gfx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"

#define SHOPS_FILE "shops.bin"
#define PRIZE_FLAG 0x80
#define CAP 16
#define VIS_MAX 6
#define LIST_X 3
#define RIGHT_R 118

typedef struct {
    uint8_t n, cur, top, vis, wait, ask;
    Prize p[MAX_PRIZES];
    tchar lab[VIS_MAX][CAP];
    tchar l_cancel[CAP];
} PZ;

enum { W_NONE_pz = 0, W_LIST, W_ASK, W_BYE };

static PZ* g_pz;
#ifndef NUCLEO_DEVICE
static uint16_t g_last_msg;
#endif

bool scene_prize_active(void) {
    return g_pz != NULL;
}

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

// Reads the prize record of shop `id` (section 37.4) into out[0..*n). False (nothing written) for a missing / damaged file, a shop id out of range, an ordinary (item) shop or an entry that
// is not valid (kind, id out of range, level, price).
bool prize_load(uint8_t id, Prize* out, uint8_t* n) {
    uint8_t h[8], o[2], rec[1 + 5 * MAX_PRIZES];
    if(!read_at(SHOPS_FILE, 0, h, sizeof(h))) return false;
    if(memcmp(h, "NSHP", 4) != 0 || h[4] != 1 || id == 0 || id >= h[5]) return false;
    const uint16_t size = rd16(h + 6);
    if(size < 8u + 2u * h[5] || file_size(SHOPS_FILE) != (int32_t)size) return false;
    if(!read_at(SHOPS_FILE, 8u + 2u * id, o, 2)) return false;
    const uint16_t off = rd16(o);
    if(off < 8u + 2u * h[5] || off >= size || !read_at(SHOPS_FILE, off, rec, 1)) return false;
    const uint8_t count = rec[0] & (uint8_t)~PRIZE_FLAG;
    if(!(rec[0] & PRIZE_FLAG) || count == 0 || count > MAX_PRIZES || off + 1u + 5u * count > size) return false;
    if(!read_at(SHOPS_FILE, off + 1u, rec + 1, 5u * count)) return false;
    const BcData* bd = party_data();
    for(uint8_t k = 0; k < count; k++) {
        const uint8_t* e = rec + 1 + 5 * k;
        const uint16_t price = rd16(e + 3);
        const bool ok = e[0] == 0 ? (e[1] >= 1 && e[1] < IT_COUNT && e[2] >= 1)
                                  : (e[0] == 1 && bd && e[1] >= 1 && e[1] < bd->n_species && e[2] >= 1 && e[2] <= 100);
        if(!ok || price < 1 || price > COINS_MAX) return false;
        out[k] = (Prize){e[0], e[1], e[2], price};
    }
    *n = count;
    return true;
}

static uint16_t txt_of(const Prize* p) {
    return p->kind ? reg_sp_txt(p->id) : reg_it_txt(p->id);
}

static void ld(tchar* d, uint16_t id) {
    if(text_load(id, d, CAP) < 0) d[0] = 0;
}

static int rows(const PZ* s) {
    return s->n + 1; // + Cancelar
}

static void load_rows(PZ* s) {
    const tchar* all[VIS_MAX + 1];
    int n = 0;
    for(int v = 0; v < s->vis; v++) {
        s->lab[v][0] = 0;
        if(s->top + v < s->n) ld(s->lab[v], txt_of(&s->p[s->top + v]));
        all[n++] = s->lab[v];
    }
    all[n++] = s->l_cancel;
    text_prefetch_many(all, n);
}

static void clamp(PZ* s) {
    const int n = rows(s);
    if(s->cur >= n) s->cur = (uint8_t)(n - 1);
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
    if(n <= s->vis) s->top = 0;
    else if(s->top + s->vis > n) s->top = (uint8_t)(n - s->vis);
    load_rows(s);
}

static int list_y0(void) {
    return font_pitch() + 5;
}

static void say(SceneCtx* c, PZ* s, uint16_t id, uint8_t after) {
#ifndef NUCLEO_DEVICE
    g_last_msg = id;
#endif
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) { // no dialog: carry on as if it had been read
        s->wait = W_NONE_pz;
        if(after == W_BYE) {
            scene_pop(c);
            scene_wake(c, 1);
        }
    }
}

static void deal(SceneCtx* c, PZ* s) { // YES: check everything again, then charge and hand over
    const Prize* p = &s->p[s->cur];
    if(casino_coins() < p->price) {
        say(c, s, TXT_GL_C4C_PRIZE_NO_COINS, W_LIST);
    } else if(p->kind && party_player()->count >= BC_MAX_PARTY) {
        say(c, s, TXT_GL_C4C_PRIZE_FULL, W_LIST);
    } else if(!p->kind && !(bag_count(p->id) ? bag_count(p->id) < BAG_MAX_COUNT : bag_kinds() < BAG_MAX_KINDS)) {
        say(c, s, TXT_GL_SHOP_BAG_FULL, W_LIST);
    } else if(!casino_coins_take(p->price)) {
        say(c, s, TXT_GL_C4C_PRIZE_NO_COINS, W_LIST);
    } else {
        if(p->kind) party_give(p->id, p->level);
        else bag_add(p->id, p->level);
        say(c, s, TXT_GL_C4C_PRIZE_BOUGHT, W_LIST);
    }
    scene_dirty(c, D_COMPOSE);
}

static void resume_pz(SceneCtx* c, PZ* s) {
    const uint8_t a = s->wait;
    s->wait = W_NONE_pz;
    if(a == W_BYE) {
        scene_pop(c);
        scene_wake(c, 1);
        return;
    }
    if(a == W_ASK && s->ask) {
        deal(c, s);
        return;
    }
    clamp(s);
    scene_dirty(c, D_WORLD);
}

static void p_input(SceneCtx* c, void* d, const InputEv* ev) {
    PZ* s = d;
    if(s->wait) resume_pz(c, s);
    if(scene_top_ops(c) != &scene_prize_ops) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        else if(ev->type == EV_SHORT) say(c, s, TXT_GL_C4C_PRIZE_BYE, W_BYE);
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    const int n = rows(s);
    if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
        s->cur = (uint8_t)((s->cur + (ev->key == KEY_DOWN ? 1 : n - 1)) % n);
        clamp(s);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && press) {
        if(s->cur >= s->n) { // Cancelar
            say(c, s, TXT_GL_C4C_PRIZE_BYE, W_BYE);
            return;
        }
        const Prize* p = &s->p[s->cur];
        if(casino_coins() < p->price) {
            say(c, s, TXT_GL_C4C_PRIZE_NO_COINS, W_LIST);
            return;
        }
        text_set_var_id(0, txt_of(p));
        text_set_var_num(2, p->price);
        s->ask = 0;
        s->wait = W_ASK;
        if(!scene_ask_open(c, TXT_GL_C4C_PRIZE_ASK, &s->ask)) s->wait = W_NONE_pz;
    }
}

static void p_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    PZ* s = d;
    if(s->wait) resume_pz(c, s);
}

static void p_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static void draw_coin(int x, int y) {
    static const uint8_t COIN[8] = {0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C};
    ov_bitmap(x, y, 8, 8, COIN, 1, 1);
}

static void draw_price(uint16_t v, int y) { // right-aligned at RIGHT_R
    char t[8];
    tchar m[8];
    const int n = hud_num(t, v);
    for(int i = 0; i <= n; i++) m[i] = (tchar)t[i];
    font_draw_text(m, RIGHT_R - font_text_width(m), y);
}

static void p_draw(SceneCtx* c, void* d) {
    (void)c;
    PZ* s = d;
    const int p = font_pitch(), nx = LIST_X + font_cell_w() + 2, y0 = list_y0();
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    draw_coin(LIST_X, 2 + (p - 8) / 2);
    {
        char t[8];
        tchar m[8];
        const int n = hud_num(t, casino_coins());
        for(int i = 0; i <= n; i++) m[i] = (tchar)t[i];
        font_draw_text(m, SCREEN_W - 3 - font_text_width(m), 2);
    }
    ov_fill(1, p + 3, SCREEN_W - 2, 1, 1);
    for(int v = 0; v < s->vis && s->top + v < rows(s); v++) {
        const int k = s->top + v, y = y0 + v * p;
        if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X, y, 1);
        if(k >= s->n) {
            font_draw_text(s->l_cancel, nx, y);
            continue;
        }
        font_draw_text(s->lab[v], nx, y);
        if(s->p[k].kind) { // a creature: its level, tiny, between the name and the price
            char t[12];
            hud_level(t, s->p[k].level);
            tiny_draw(t, RIGHT_R - 28 - tiny_width(t), y + font_baseline() - 5);
        }
        draw_price(s->p[k].price, y);
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(121, y0 + 1, 5, 3, UP, 1, 1);
    if(s->top + s->vis < rows(s)) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
}

static void p_enter(SceneCtx* c, void* d) {
    PZ* s = d;
    ld(s->l_cancel, TXT_GL_PARTY_CANCEL);
    s->vis = (uint8_t)((SCREEN_H - 2 - list_y0()) / font_pitch());
    if(s->vis > VIS_MAX) s->vis = VIS_MAX;
    if(s->vis < 1) s->vis = 1;
    clamp(s);
    scene_dirty(c, D_WORLD);
}

static void p_leave(SceneCtx* c, void* d) {
    (void)c;
    if(g_pz == d) g_pz = NULL;
    free(d);
}

const SceneOps scene_prize_ops = {
    .enter = p_enter,
    .leave = p_leave,
    .input = p_input,
    .update = p_update,
    .draw_world = p_draw_world,
    .draw_overlay = p_draw,
    .pauses_below = true,
};

bool scene_prize_open(SceneCtx* c, uint8_t shop_id) {
    if(g_pz || c->n + 3 > MAX_SCENES) return false; // room for the dialog + YES/NO above us
    PZ* s = calloc(1, sizeof(PZ));
    if(!s) return false;
    if(!prize_load(shop_id, s->p, &s->n)) {
        free(s);
        return false;
    }
    g_pz = s;
    if(!scene_push(c, &scene_prize_ops, s)) {
        g_pz = NULL;
        free(s);
        return false;
    }
    say(c, s, TXT_GL_C4C_PRIZE_HELLO, W_LIST);
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_prize_cur(void) { return g_pz ? g_pz->cur : -1; }
int scene_prize_count(void) { return g_pz ? g_pz->n : -1; }
int scene_prize_entry(int k, uint8_t* kind, uint8_t* id, uint8_t* level, uint16_t* price) {
    if(!g_pz || k < 0 || k >= g_pz->n) return 0;
    *kind = g_pz->p[k].kind;
    *id = g_pz->p[k].id;
    *level = g_pz->p[k].level;
    *price = g_pz->p[k].price;
    return 1;
}
int scene_prize_last_msg(void) { return g_last_msg; }
#endif
