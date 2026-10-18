// The LINK scene (contract section 45), the scene of the LINK plugin: the Club de Enlace over the infrared port. It owns the session (link_proto.c) and the game (link_game.c: the
// trade or the team exchange of the exhibition battle) and draws them. NOTHING is a dialog while the link is alive: a dialog pauses the scenes below it, and a paused scene stops
// polling the IR port (the friend would time out), so the YES / NO question is drawn here and the dialogs only report the result once the session is closed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link_host.h"
#include "link_phy.h"
#include "party.h"
#include "bag.h"
#include "world.h"
#include "legend.h"
#include "assets.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "hud_gfx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "gen/langs_gen.h"

#define SL_CAP 16
#define SL_VIS_MAX 6
#define SL_LIST_X 3
#define SL_RIGHT_R 118

enum { SLW_NONE = 0, SLW_END };

typedef struct {
    LinkSess sess;
    LinkGame game;
    uint8_t* out;
    uint8_t arg, cur, top, vis, sel, ph, wait, anim, prog;
    bool prepped;
    uint32_t clk;
    tchar t_search[24], t_aim[40], t_pick[24], t_wait[32], t_ot[10], t_swap[24], t_yes[10], t_no[10], t_trading[24], t_teams[32];
    tchar nm[BC_MAX_PARTY][SL_CAP]; // the names of our party (the pick list)
    tchar mine[SL_CAP], theirs[SL_CAP], ot[12];
} LK;

static LK* g_lk;

bool link_active(void) {
    return g_lk != NULL;
}

// ---------------------------------------------------------------------------------------------------------------- names
// Unicode -> codes of the active language (ASCII is itself, the rest comes from the pairs of lang/<xx>/kb.bin: u16 code + u16 Unicode from byte 20, n at byte 12; a character
// without a pair is '?'). The same typecast as box.c's nick_codes: a plugin cannot call another plugin's code.
static void sl_ucodes(tchar* out, const uint16_t* u, int cap) {
    uint8_t* kb = NULL;
    uint16_t n = 0;
    int i = 0;
    for(; u[i] && i < cap - 1; i++)
        if(u[i] > 0x7E && !kb) {
            char path[28];
            uint8_t h[20];
            snprintf(path, sizeof(path), "lang/%s/kb.bin", LANG_DIR[text_language()]);
            if(read_at(path, 0, h, sizeof(h)) && !memcmp(h, "NKBD", 4) && (n = (uint16_t)(h[12] | (h[13] << 8))) <= 512 && heap_can_alloc(4u * n) &&
               (kb = malloc(4u * n + 1)) && !read_at(path, 20, kb, 4u * n)) {
                free(kb);
                kb = NULL;
            }
            if(!kb) n = 0;
        }
    for(i = 0; u[i] && i < cap - 1; i++) {
        tchar c = u[i] < 0x7F ? (tchar)u[i] : (tchar)'?';
        for(uint16_t k = 0; u[i] > 0x7E && kb && k < n; k++)
            if((uint16_t)(kb[4 * k + 2] | (kb[4 * k + 3] << 8)) == u[i]) c = (tchar)(kb[4 * k] | (kb[4 * k + 1] << 8));
        out[i] = c;
    }
    out[i] = 0;
    free(kb);
}

static void sl_sp_name(tchar* out, uint8_t sp, int cap) {
    int len = sp ? text_load(reg_sp_txt(sp), out, cap) : -1;
    if(len < 0) out[0] = 0;
}

static void sl_ld(tchar* d, uint16_t id, int cap) {
    if(text_load(id, d, cap) < 0) d[0] = 0;
}

// The nickname codes cache of the real party follows a trade (the party screen, the battle and the PC read it)
static void sl_slot_changed(void* ctx, uint8_t slot) {
    (void)ctx;
    sl_ucodes(party_nick_cache()[slot], party_state()->nick_u[slot], PARTY_NICK_CAP);
}

// ---------------------------------------------------------------------------------------------------------------- the host transport
#ifndef NUCLEO_DEVICE
const LinkChan* link_host_chan;
const LinkChan* link_chan_open(void) {
    return link_host_chan;
}
void link_chan_close(void) {}
#endif

// ---------------------------------------------------------------------------------------------------------------- drawing
static int sl_pitch(void) {
    return font_pitch();
}
static int sl_list_y0(void) {
    return sl_pitch() + 5;
}

// Text with line breaks (0x0A), each line centred, from y down; returns the next y.
static int sl_put_c(const tchar* t, int y) {
    const tchar* p = t;
    tchar ln[40];
    while(*p) {
        int n = 0;
        while(p[n] && p[n] != 0x0A && p[n] != 0x0C && n < 39) {
            ln[n] = p[n];
            n++;
        }
        ln[n] = 0;
        font_draw_text(ln, (SCREEN_W - font_text_width(ln)) / 2, y);
        y += sl_pitch();
        p += n;
        if(*p == 0x0A || *p == 0x0C) p++;
    }
    return y;
}

// Makes the glyphs of what a phase draws resident (SD-backed CJK fonts keep a small cache: only that phase's strings, never all of them at once).
static void sl_prefetch(LK* s, uint8_t ph) {
    const tchar* all[10];
    int n = 0;
    switch(ph) {
    case LP_CONNECT: all[n++] = s->t_search; all[n++] = s->t_aim; break;
    case LP_PICK:
        all[n++] = s->t_pick;
        for(int k = 0; k < s->game.st.ps->party.count && k < BC_MAX_PARTY; k++) all[n++] = s->nm[k];
        break;
    case LP_REVIEW: all[n++] = s->mine; all[n++] = s->theirs; all[n++] = s->ot; all[n++] = s->t_ot; all[n++] = s->t_swap; all[n++] = s->t_yes; all[n++] = s->t_no; break;
    case LP_WAIT_OFFER:
    case LP_WAIT_VOTE: all[n++] = s->t_wait; break;
    case LP_COMMIT: all[n++] = s->t_trading; break;
    case LP_XCHG: all[n++] = s->t_teams; break;
    default: return;
    }
    text_prefetch_many(all, n);
}

static void sl_spinner(int cx, int y, uint8_t a) { // three IR "waves" lighting up one after the other
    for(int i = 0; i < 3; i++) {
        if(a > i) ov_fill(cx - 12 + i * 9, y + 5 - i * 2, 5, 3 + i * 4, 1);
        else ov_frame(cx - 12 + i * 9, y + 5 - i * 2, 5, 3 + i * 4, 1);
    }
}

static void sl_level_tag(int x_right, int y, uint8_t level) {
    char t[12];
    hud_level(t, level);
    tiny_draw(t, x_right - tiny_width(t), y + font_baseline() - 5);
}

static void l_draw(SceneCtx* c, void* d) {
    (void)c;
    LK* s = d;
    const int p = sl_pitch();
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    switch(s->game.phase) {
    case LP_CONNECT: {
        int y = sl_put_c(s->t_search, 6);
        y = sl_put_c(s->t_aim, y + 2);
        sl_spinner(SCREEN_W / 2, SCREEN_H - 14, s->anim);
        break;
    }
    case LP_PICK: {
        font_draw_text(s->t_pick, SL_LIST_X, 2);
        ov_fill(1, p + 3, SCREEN_W - 2, 1, 1);
        const int y0 = sl_list_y0(), n = s->game.st.ps->party.count;
        for(int v = 0; v < s->vis && s->top + v < n; v++) {
            const int k = s->top + v, y = y0 + v * p;
            if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, SL_LIST_X, y, 1);
            font_draw_text(s->nm[k], SL_LIST_X + font_cell_w() + 2, y);
            sl_level_tag(SL_RIGHT_R, y, s->game.st.ps->party.mon[k].level);
        }
        static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
        if(s->top > 0) ov_bitmap(121, y0 + 1, 5, 3, UP, 1, 1);
        if(s->top + s->vis < n) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
        break;
    }
    case LP_REVIEW: {
        static const uint8_t OUT[5] = {0x20, 0x10, 0xFC, 0x10, 0x20}, IN[5] = {0x08, 0x10, 0x7E, 0x10, 0x08}; // -> (we give) and <- (we get), 7 wide
        const LgOffer *mo = &s->game.u.tr.mine, *to = &s->game.u.tr.theirs;
        int y = 3;
        ov_bitmap(SL_LIST_X, y + (p - 5) / 2, 7, 5, OUT, 1, 1);
        font_draw_text(s->mine, SL_LIST_X + 10, y);
        sl_level_tag(SL_RIGHT_R, y, mo->mon.level);
        y += p;
        ov_bitmap(SL_LIST_X, y + (p - 5) / 2, 7, 5, IN, 1, 1);
        font_draw_text(s->theirs, SL_LIST_X + 10, y);
        sl_level_tag(SL_RIGHT_R, y, to->mon.level);
        y += p;
        font_draw_text(s->t_ot, SL_LIST_X, y);
        font_draw_text(s->ot, SL_LIST_X + font_text_width(s->t_ot) + 3, y);
        y += p;
        font_draw_text(s->t_swap, SL_LIST_X, y);
        y += p;
        const int cw = font_cell_w() + 2;
        for(int k = 0; k < 2; k++) {
            const tchar* t = k == 0 ? s->t_yes : s->t_no;
            const int x = SL_LIST_X + 6 + k * 52;
            if(s->sel == k) font_draw_glyph(GLYPH_CURSOR, x, y, 1);
            font_draw_text(t, x + cw, y);
        }
        break;
    }
    case LP_WAIT_OFFER:
    case LP_WAIT_VOTE: {
        sl_put_c(s->t_wait, 14);
        sl_spinner(SCREEN_W / 2, SCREEN_H - 14, s->anim);
        break;
    }
    case LP_COMMIT: {
        sl_put_c(s->t_trading, 20);
        sl_spinner(SCREEN_W / 2, SCREEN_H - 16, s->anim);
        break;
    }
    case LP_XCHG: {
        int y = sl_put_c(s->t_teams, 10);
        const LinkGame* gm = &s->game;
        const int mine_n = gm->st.ps->party.count, theirs_n = gm->total ? gm->total : mine_n;
        const int done = gm->sent + gm->got, all = mine_n + theirs_n;
        const int bw = 100, bx = (SCREEN_W - bw) / 2, by = y + 6;
        ov_frame(bx, by, bw, 8, 1);
        ov_fill(bx + 2, by + 2, (bw - 4) * done / (all ? all : 1), 4, 1);
        break;
    }
    default: break;
    }
}

static void l_draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

// ---------------------------------------------------------------------------------------------------------------- the screens' data
static void prep_pick(LK* s) {
    const Party* party = &s->game.st.ps->party;
    for(uint8_t k = 0; k < party->count; k++) party_mon_name(k, s->nm[k], SL_CAP);
    s->vis = (uint8_t)((SCREEN_H - 2 - sl_list_y0()) / sl_pitch());
    if(s->vis > SL_VIS_MAX) s->vis = SL_VIS_MAX;
    if(s->vis < 1) s->vis = 1;
    sl_prefetch(s, LP_PICK);
}

static void prep_review(LK* s) {
    const LgOffer *mo = &s->game.u.tr.mine, *to = &s->game.u.tr.theirs;
    party_mon_name(s->game.slot, s->mine, SL_CAP);
    if(to->nick[0]) sl_ucodes(s->theirs, to->nick, SL_CAP);
    else sl_sp_name(s->theirs, to->mon.species, SL_CAP);
    sl_ucodes(s->ot, to->ot_name, 12);
    (void)mo;
    s->sel = 0;
    sl_prefetch(s, LP_REVIEW);
}

static void sl_clamp(LK* s) {
    const int n = s->game.st.ps->party.count;
    if(s->cur >= n) s->cur = (uint8_t)(n - 1);
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
    if(n <= s->vis) s->top = 0;
    else if(s->top + s->vis > n) s->top = (uint8_t)(n - s->vis);
}

// ---------------------------------------------------------------------------------------------------------------- end of the session
static void sl_close(SceneCtx* c) {
    scene_pop(c);
    scene_wake(c, 1);
}

static void sl_finish(SceneCtx* c, LK* s) {
    LinkGame* gm = &s->game;
    uint8_t res = gm->result;
    lk_close(&s->sess);
    link_chan_close();
    s->out[1] = 255;
    if(res == LR_DONE && gm->mode == LMODE_BATTLE) { // the friend's team goes to the app for the battle that the script starts next
        LinkFoe* f = calloc(1, sizeof(LinkFoe));
        if(f) {
            f->party = gm->u.foe.party;
            f->mine = gm->st.ps->party;
            for(uint8_t k = 0; k < f->mine.count; k++) lg_mon_heal(gm->st.d, &f->mine.mon[k]);
            for(int i = 0; i < PARTY_NAME_CAP - 1 && gm->peer_i.name[i]; i++) f->name[i] = gm->peer_i.name[i] >= 0x20 && gm->peer_i.name[i] < 0x7F ? (tchar)gm->peer_i.name[i] : (tchar)'?';
            f->trainer = s->arg;
            g_link_foe = f;
        } else {
            res = LR_ERROR;
        }
    }
    uint16_t msg = 0;
    if(res == LR_DONE) {
        if(gm->mode == LMODE_TRADE) {
            if(gm->evo_slot != 255) {
                s->out[1] = gm->evo_slot;
                c->stone_to = gm->evo_to; // the evolution scene reads its target here (the stone path of contract 36)
            }
            tchar nm[SL_CAP];
            party_mon_name(gm->slot, nm, SL_CAP);
            text_set_var(0, nm);
            msg = TXT_GL_LK_DONE;
            // Contract 68: the FIRST successful trade gives the seventh Origin Fragment (flag frag_7 is the truth, the bag item is its count; a full bag leaves it for the next trade)
            if(!lg_flag(c->world, LG_FLAG_FRAG1 + 6) && bag_add(IT_ORIGIN_SHARD, 1)) {
                lg_flag_set(c->world, LG_FLAG_FRAG1 + 6);
                msg = TXT_GL_D1_LK_FRAG;
            }
        }
    } else if(res == LR_CANCEL) {
        msg = gm->why == LW_PEER_NO ? TXT_GL_LK_DECLINED : TXT_GL_LK_CANCEL;
    } else if(res == LR_NOPARTNER) {
        msg = TXT_GL_LK_NOPARTNER;
    } else {
        msg = gm->why == LW_VERSION ? TXT_GL_LK_VERSION : gm->why == LW_BAD_DATA ? TXT_GL_LK_BAD : TXT_GL_LK_ERROR;
    }
    s->out[0] = res;
    s->wait = SLW_END;
    if(!msg || !scene_dialog_open(c, msg, -1)) {
        s->wait = SLW_NONE;
        sl_close(c);
    }
}

// ---------------------------------------------------------------------------------------------------------------- scene callbacks
static void l_update(SceneCtx* c, void* d, uint32_t dt) {
    LK* s = d;
    if(s->wait) { // the result dialog has been read
        s->wait = SLW_NONE;
        sl_close(c);
        return;
    }
    s->clk += dt;
    lk_poll(&s->sess);
    lg_step(&s->game);
    const uint8_t ph = s->game.phase;
    if(ph == LP_END) {
        sl_finish(c, s);
        return;
    }
    bool redraw = false;
    if(ph != s->ph) {
        s->ph = ph;
        if(ph == LP_PICK) prep_pick(s);
        else if(ph == LP_REVIEW) prep_review(s);
        else sl_prefetch(s, ph);
        redraw = true;
    }
    const uint8_t a = (uint8_t)((s->clk / 250u) & 3u);
    if(a != s->anim && (ph == LP_CONNECT || ph == LP_WAIT_OFFER || ph == LP_WAIT_VOTE || ph == LP_COMMIT)) {
        s->anim = a;
        redraw = true;
    }
    if(ph == LP_XCHG && s->prog != (uint8_t)(s->game.sent + s->game.got)) {
        s->prog = (uint8_t)(s->game.sent + s->game.got);
        redraw = true;
    }
    if(redraw) scene_dirty(c, D_COMPOSE);
    scene_wake(c, 20);
}

static void l_input(SceneCtx* c, void* d, const InputEv* ev) {
    LK* s = d;
    if(s->wait) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c); // the usual: Back held leaves the app
        else if(ev->type == EV_SHORT) lg_abort(&s->game); // before the YES vote: leave (the friend is told); afterwards ignored
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    const uint8_t ph = s->game.phase;
    if(ph == LP_PICK) {
        const int n = s->game.st.ps->party.count;
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            s->cur = (uint8_t)((s->cur + (ev->key == KEY_DOWN ? 1 : n - 1)) % n);
            sl_clamp(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            lg_pick(&s->game, s->cur);
        }
    } else if(ph == LP_REVIEW) {
        if(ev->key == KEY_LEFT || ev->key == KEY_RIGHT || ev->key == KEY_UP || ev->key == KEY_DOWN) {
            s->sel ^= 1;
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            lg_vote(&s->game, s->sel == 0);
        }
    }
}

static void l_leave(SceneCtx* c, void* d) {
    (void)c;
    LK* s = d;
    if(g_lk == s) {
        g_lk = NULL;
        if(s->game.phase != LP_END || lk_state(&s->sess) < LS_CLOSED) {
            lk_close(&s->sess);
            link_chan_close(); // (Back held, or the app exits: the infrared port is released)
        }
    }
    free(s);
}

static void l_enter(SceneCtx* c, void* d) {
    LK* s = d;
    sl_ld(s->t_search, TXT_GL_LK_SEARCH, 24);
    sl_ld(s->t_aim, TXT_GL_LK_AIM, 40);
    sl_ld(s->t_pick, TXT_GL_LK_PICK, 24);
    sl_ld(s->t_wait, TXT_GL_LK_WAIT, 32);
    sl_ld(s->t_ot, TXT_GL_LK_OT, 10);
    sl_ld(s->t_swap, TXT_GL_LK_SWAP, 24);
    sl_ld(s->t_yes, TXT_GL_CHOICE_YES, 10);
    sl_ld(s->t_no, TXT_GL_CHOICE_NO, 10);
    sl_ld(s->t_trading, TXT_GL_LK_TRADING, 24);
    sl_ld(s->t_teams, TXT_GL_LK_TEAMS, 32);
    sl_prefetch(s, LP_CONNECT);
    scene_dirty(c, D_WORLD);
    scene_wake(c, 1);
}

static const SceneOps scene_link_ops = {
    .enter = l_enter,
    .leave = l_leave,
    .input = l_input,
    .update = l_update,
    .draw_world = l_draw_world,
    .draw_overlay = l_draw,
    .pauses_below = true,
};

static bool scene_link_open(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) {
    const BcData* d = party_data();
    PartyState* ps = party_state();
    if(g_lk || !d || !ps->party.count || c->n + 2 > MAX_SCENES) return false; // room for the result dialog
    if(!heap_can_alloc((uint32_t)sizeof(LK) + 1024u)) return false;
    const LinkChan* ch = link_chan_open();
    if(!ch) return false;
    LK* s = calloc(1, sizeof(LK));
    if(!s) {
        link_chan_close();
        return false;
    }
    s->out = out;
    s->arg = arg;
    s->ph = 0xFF;
    lk_init(&s->sess, ch);
    const LgStore st = {.d = d, .ps = ps, .slot_changed = sl_slot_changed, .ctx = NULL};
    lg_init(&s->game, &s->sess, &st, op == LK_OP_BATTLE ? LMODE_BATTLE : LMODE_TRADE);
    g_lk = s;
    if(!scene_push(c, &scene_link_ops, s)) {
        g_lk = NULL;
        link_chan_close();
        free(s);
        return false;
    }
    return true;
}

// The self-test (dev): the codec round trip with the receiver module's distortion, and on the device the real transmit time of a frame. Reports through a dialog.
#ifdef NUCLEO_DEVICE
bool link_ir_selftest(uint32_t* bytes_per_s);
#endif
static bool sl_selftest(SceneCtx* c, uint8_t* out) {
    uint32_t air_us = 0, bps = 0;
    const bool ok = phy_selftest(150, &air_us);
    bps = air_us ? 64u * 1000000u / air_us : 0;
#ifdef NUCLEO_DEVICE
    link_ir_selftest(&bps); // (the measured rate of a frame that really went out of the LED; the codec figure stays when the port is busy)
#endif
    out[0] = ok;
    out[1] = bps > 255 ? 255 : (uint8_t)bps;
    text_set_var_num(2, (int)bps);
    return scene_dialog_open(c, TXT_GL_LK_SELFTEST, -1);
}

bool link_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) {
    free(g_link_foe); // a team left over from an earlier exchange that was never fought
    g_link_foe = NULL;
    if(!c->world || out + 1 >= c->world->vars + SCRIPT_VARS) return false; // VAR + 1 must exist (mapc keeps VAR <= 14)
    out[0] = LR_OPEN_FAIL;
    out[1] = 255;
    if(op == LK_OP_SELFTEST) return sl_selftest(c, out);
    if(op != LK_OP_TRADE && op != LK_OP_BATTLE) return false;
    return scene_link_open(c, op, arg, out);
}

#ifndef NUCLEO_DEVICE
bool scene_link_probe(LinkProbe* p) {
    p->open = g_lk != NULL;
    p->game = g_lk ? &g_lk->game : NULL;
    p->sess = g_lk ? &g_lk->sess : NULL;
    p->phase = g_lk ? g_lk->game.phase : 0xFF;
    p->cur = g_lk ? g_lk->cur : 0;
    p->sel = g_lk ? g_lk->sel : 0;
    return g_lk != NULL;
}
#endif
