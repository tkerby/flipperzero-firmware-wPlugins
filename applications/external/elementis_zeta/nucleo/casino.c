// The casino plugin's rules that are not a screen (contract section 37): the Coin Purse, the coins, the clerk's sale and the entry point `casino_run` the native calls.
#include "casino.h"
#include "party.h"
#include "bag.h"
#include "text.h"
#include "gen/reg_gen.h"
#include "gen/maps_gen.h"
#include "world.h"
#include "legend.h"
#include "scene_reg.h" // contract 76: the Registro (scene_reg.c, in this plugin)
#ifndef NUCLEO_DEVICE
#include "rtc_host.h"
DateTime g_host_dt = {.hour = 12, .minute = 0, .second = 0, .day = 1, .month = 1, .year = 2026, .weekday = 1}; // the host harness sets the clock here (rtc_host.h)
void furi_hal_rtc_get_datetime(DateTime* dt) { *dt = g_host_dt; }
#endif

bool casino_has_case(void) {
    return bag_count(IT_ESTUCHE) > 0;
}

uint16_t casino_coins(void) {
    return party_state()->coins;
}

uint16_t casino_coins_add(uint16_t n) {
    PartyState* ps = party_state();
    if(!casino_has_case() || ps->coins >= COINS_MAX) return 0;
    if(n > COINS_MAX - ps->coins) n = (uint16_t)(COINS_MAX - ps->coins);
    ps->coins = (uint16_t)(ps->coins + n);
    return n;
}

bool casino_coins_take(uint16_t n) {
    PartyState* ps = party_state();
    if(ps->coins < n) return false;
    ps->coins = (uint16_t)(ps->coins - n);
    return true;
}

// `coins_buy`: clerk, 50 coins for $1000. The case is checked first, then the money, then the room (the whole pack has to fit under the 9,999 cap); nothing is charged on a refusal.
static uint8_t buy(void) {
    if(!casino_has_case()) return CB_NO_CASE;
    if(money_get() < COINS_PRICE) return CB_NO_MONEY;
    if((unsigned)casino_coins() + COINS_PACK > COINS_MAX) return CB_FULL;
    if(!money_take(COINS_PRICE)) return CB_NO_MONEY;
    casino_coins_add(COINS_PACK);
    return CB_OK;
}

// ---- Reserva Salvaje (contract 43). The session is PartyState.rv_steps (steps left, 0 = not inside) + rv_map / rv_ti / rv_tj / rv_face (the way home); the crystals are the bag item.
static bool g_rv_end; // the end scene is on the stack (keeps the plugin loaded until it has warped the hero home)

static bool rv_clear(void) { // ends the session; true when one was on. The crystals left are taken back.
    PartyState* p = party_state();
    const bool was = p->rv_steps != 0; // (a gate on map 0 is a valid way home: the map byte says nothing)
    p->rv_steps = 0;
    p->rv_map = p->rv_ti = p->rv_tj = p->rv_face = 0;
    while(bag_take(IT_CRISTAL_RESERVA)) {}
    return was;
}

static uint8_t rv_start(SceneCtx* c) {
    PartyState* p = party_state();
    if(p->rv_steps) return RS_INSIDE;
    if(money_get() < RV_PRICE) return RS_NO_MONEY;
    while(bag_take(IT_CRISTAL_RESERVA)) {} // (none are left over from an older visit)
    if(bag_add(IT_CRISTAL_RESERVA, RV_BALLS) != RV_BALLS) { // nothing is charged when the bag cannot hold them
        while(bag_take(IT_CRISTAL_RESERVA)) {}
        return RS_BAG_FULL;
    }
    money_take(RV_PRICE);
    const World* w = c->world;
    p->rv_map = w->map.id;
    p->rv_ti = w->hero.ti;
    p->rv_tj = w->hero.tj;
    p->rv_face = w->hero.facing & 7;
    p->rv_steps = RV_STEPS;
    return RS_OK;
}

// The end scene: it sits under the "time's up" dialog; when the dialog closes it is the top scene and its update ends the session and fades the hero to the gate.
static void rv_end_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)d;
    (void)dt;
    const PartyState* p = party_state();
    const uint8_t m = p->rv_map, i = p->rv_ti, j = p->rv_tj, f = p->rv_face;
    rv_clear();
    g_rv_end = false;
    scene_pop(c);
    scene_wake(c, 1);
    scene_fade_warp(c, m, i, j, f);
}
static const SceneOps rv_end_ops = {.update = rv_end_update, .pauses_below = true};

static bool rv_timeup(SceneCtx* c) {
    const PartyState* p = party_state();
    const uint16_t txt = p->rv_steps ? TXT_GL_C5B_RV_BALLS : TXT_GL_C5B_RV_TIME;
    if(c->n + 1 >= MAX_SCENES || !scene_push(c, &rv_end_ops, NULL)) { // (the stack is full: the session ends without the walk home)
        rv_clear();
        return false;
    }
    g_rv_end = true;
    scene_dialog_open(c, txt, -1); // (if the box cannot open, the walk home still happens)
    return true;
}

// Contract 65 (CAS_BOND, casino_host.h): the maintainer's friendship tier for the evaluator, and the Centro's +1 for every creature that was really tended (HP or status; PP alone does not count: every fight uses some).
static uint8_t bond_op(uint8_t arg) {
    Party* p = party_player();
    if(arg == BOND_TIER) {
        if(!p->count) return 255;
        tchar nm[16];
        party_mon_name(0, nm, 16);
        text_set_var(0, nm);
        return bc_friend_tier(bc_friend(&p->mon[0]));
    }
    uint8_t n = 0;
    for(uint8_t k = 0; k < p->count; k++) {
        Mon* m = &p->mon[k];
        if(m->hp < m->max_hp || m->status) bc_friend_add(m, 1), n++;
    }
    return n;
}

// Contract 68 (CAS_LEGEND, casino_host.h): the Flipper's clock. DEV builds and the host harness may override it with three story flags (legend.h: clk_on + a 2-bit preset); a release build reads the RTC only.
static uint8_t legend_op(const World* w, uint8_t arg) {
    uint8_t h;
#if defined(NUCLEO_DEBUG) || !defined(NUCLEO_DEVICE)
    if(lg_flag(w, LG_FLAG_CLK_ON)) {
        h = LG_CLK_PRESET[lg_flag(w, LG_FLAG_CLK_B0) | (lg_flag(w, LG_FLAG_CLK_B1) << 1)];
    } else
#endif
    {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        h = dt.hour;
    }
    (void)w;
    return arg == CLK_HOUR ? h : arg == CLK_NIGHT ? lg_night(h) : lg_cave_window(h);
}

bool casino_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) {
    switch(op) {
    case CAS_LEGEND: *out = legend_op(c->world, arg); return false;
    case CAS_BOND:
        if(arg >= BOND_ACT) { // (contract 69: a world act of menu_open_daycare: b = (act - WACT_ROAM + 1) << 4 | arg)
            const uint8_t a = (uint8_t)((arg >> 4) - 1 + WACT_ROAM);
            return a == WACT_POISON ? poison_act(c) : a >= WACT_DETECT ? f2_feat(c, a) : a == WACT_FOLLOW ? scene_follow_open(c) : roam_feat(a, (uint8_t)(arg & 15)); // (contract 74: the Buscatesoros and the Atlas, scene_f2.c)
        }
        if(arg == BOND_DIPLOMA) return f2_diploma(c); // contract 74
        if(arg == BOND_REG) return scene_reg_open(c);   // contract 76: the pause menu's Registro (scene_reg.c)
        *out = arg == BOND_OT ? f2_ot(out) : bond_op(arg);
        return false;
    case CAS_RV_START: *out = rv_start(c); return false;
    case CAS_RV_END: *out = rv_clear(); return false;
    case CAS_RV_LEFT:
        *out = party_state()->rv_steps != 0;
        text_set_var_num(2, party_state()->rv_steps);
        text_set_var_num(3, bag_count(IT_CRISTAL_RESERVA));
        return false;
    case CAS_RV_TIMEUP: return rv_timeup(c);
    case CAS_BUY: *out = buy(); return false;
    case CAS_GET:
        *out = casino_coins() > 255 ? 255 : (uint8_t)casino_coins();
        text_set_var_num(2, casino_coins());
        return false;
    case CAS_SLOTS:
    case CAS_PRIZES:
        *out = !casino_has_case() ? CS_NO_CASE : (op == CAS_SLOTS && casino_coins() == 0) ? CS_NO_COINS : CS_ERROR;
        if(*out == CS_ERROR && (op == CAS_SLOTS ? scene_slots_open(c) : scene_prize_open(c, arg))) {
            *out = CS_OK;
            return true;
        }
        return false;
    default: *out = CS_ERROR; return false;
    }
}

bool casino_active(void) {
    return g_rv_end || scene_slots_active() || scene_prize_active() || scene_follow_active() || scene_poison_active() || scene_f2_active() || scene_reg_active();
}
