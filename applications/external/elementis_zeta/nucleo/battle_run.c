// Battle turn engine (formulas, stat stages, damage, status, AI, bc_init / bc_choose / bc_step): the second half of the
// battle core. Split from battle_core.c (data loading, creature creation, RNG) because on the Flipper this half is
// only needed while a battle is on, so it is built into the battle plugin (application.fam); the app keeps the
// data half for party.c. Both are linked together on the host. Pure C (no furi.h).
#include "battle_core.h"
#include "battle_features.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================================
// Formulas
// ============================================================================================
static const uint8_t RATIO[13][2] = {{25, 100}, {28, 100}, {33, 100}, {40, 100}, {50, 100}, {66, 100}, {1, 1},
                                     {15, 10},  {2, 1},    {25, 10},  {3, 1},    {35, 10},  {4, 1}};

void bc_stage_ratio(int stage, uint8_t* num, uint8_t* den) {
    if(stage < -6) stage = -6;
    if(stage > 6) stage = 6;
    *num = RATIO[stage + 6][0];
    *den = RATIO[stage + 6][1];
}

uint16_t bc_stage_apply(uint16_t v, int stage) {
    uint8_t n, dn;
    bc_stage_ratio(stage, &n, &dn);
    uint32_t r = (uint32_t)v * n / dn;
    if(r < 1) r = 1;
    if(r > BC_STAT_CAP) r = BC_STAT_CAP;
    return (uint16_t)r;
}

#if BF_TRANSFORM
#define RAW_STAT(m, sd, st) ((sd)->tf ? (sd)->tst[st] : (m)->stat[st] + ((sd)->ecl ? bc_ecl_bonus((sd)->ecl, (st), (m)->level) : 0))
#else
#define RAW_STAT(m, sd, st) ((m)->stat[st] + ((sd)->ecl ? bc_ecl_bonus((sd)->ecl, (st), (m)->level) : 0))
#endif // the stat before the stages: the Mon's (Transform: the copied one), plus the Eclipse Form's points (contract 67)

uint16_t bc_eff_stat(const Mon* m, const Side* sd, int stat) {
    uint16_t raw = (uint16_t)RAW_STAT(m, sd, stat);
#if BF_BADGE
    if((sd->bdg >> stat) & 1) raw = (uint16_t)(raw + raw / 8); // contract 73: a badge's boost, x 9/8 (bc_stage_apply caps at 999)
#endif
    uint16_t v = bc_stage_apply(raw, sd->stage[stat]);
    if(stat == STI_SPD && (m->status & BS_PAR)) v = v / 4 ? v / 4 : 1;
    if(stat == STI_ATK && (m->status & BS_BRN)) v = v / 2 ? v / 2 : 1;
    return v;
}

// Contract 83 (the packed chart, battle_core.h `chart`): the next row at or after *pi that names the attacking type `att` and one of the two defending types, as its multiplier in tenths (0 / 5 / 20) + 1; 0 = no more rows (and no chart loaded:
// every matchup is neutral). *pi moves past the row. One copy of the decode for the damage, the type test and the AI.
static __attribute__((noinline)) uint8_t chart_next(const BcData* d, uint8_t* pi, uint8_t att, uint8_t t1, uint8_t t2) {
    for(uint8_t i = *pi; i < (d->chart ? d->n_chart : 0); i++) {
        const uint8_t r = d->chart[i], m = (d->chart[d->n_chart + (i >> 2)] >> ((i & 3) * 2)) & 3;
        if((r & 15) != att || ((r >> 4) != t1 && (r >> 4) != t2)) continue;
        *pi = (uint8_t)(i + 1);
        return (uint8_t)(5 * m * m + 1);
    }
    return 0;
}

uint8_t bc_type_eff(const BcData* d, uint8_t mt, uint8_t t1, uint8_t t2) {
    uint32_t eff = 10;
    uint8_t i = 0, m;
    while((m = chart_next(d, &i, mt, t1, t2))) eff = eff * (m - 1) / 10;
    return (uint8_t)(eff > 255 ? 255 : eff);
}

uint16_t bc_damage(const BcData* d, const BcDmgIn* in, uint8_t* eff_out) {
    uint32_t lvl = in->crit ? (uint32_t)in->level * 2 : in->level;
    uint32_t A = in->a, D = in->d;
    if(A > 255 || D > 255) { // FIX: both clamped to >= 1 afterwards
        A /= 4;
        D /= 4;
    }
    if(A < 1) A = 1;
    if(D < 1) D = 1;
    uint32_t x = ((lvl * 2) / 5 + 2) * in->power;
    x = x * A / D / 50;
    if(x > 997) x = 997;
    x += 2;
    uint32_t eff = 10;
    if(in->move_type != TY_NONE) {
        if(in->move_type == in->atk_t1 || in->move_type == in->atk_t2) x += x / 2; // STAB
        uint8_t i = 0, m;
        while((m = chart_next(d, &i, in->move_type, in->def_t1, in->def_t2))) {
            x = x * (m - 1) / 10;
            eff = eff * (m - 1) / 10;
        }
        if(x > 1) x = x * in->rnd / 255;
        if(eff > 0 && x < 1) x = 1; // FIX: a non-immune move always does at least 1
    } else if(x < 1) {
        x = 1;
    }
    if(eff_out) *eff_out = (uint8_t)(eff > 255 ? 255 : eff);
    if(eff == 0) x = 0;
    return (uint16_t)(x > 0xFFFF ? 0xFFFF : x);
}

uint8_t bc_crit_threshold(uint8_t base_speed, bool high_crit, bool focus) {
    uint32_t b = base_speed / 2;
    uint32_t t = high_crit ? b * 8 : b;
    if(t > 255) t = 255;
    if(focus) { // FIX: Steady Gaze multiplies by 4 (divided by 4)
        t *= 4;
        if(t > 255) t = 255;
    }
    return (uint8_t)t;
}

static uint32_t ratio_mul(uint32_t v, int stage) {
    uint8_t n, dn;
    bc_stage_ratio(stage, &n, &dn);
    v = v * n / dn;
    if(v < 1) v = 1;
    if(v > 255) v = 255;
    return v;
}

uint16_t bc_hit_threshold(uint8_t acc, int acc_stage, int eva_stage, bool never_miss) {
    if(never_miss) return 256;
    uint32_t a = ratio_mul(acc, acc_stage);
    a = ratio_mul(a, -eva_stage);
    if(a >= 255) return 256; // FIX: a final accuracy of 255 always hits (no 1/256 miss)
    return (uint16_t)a;
}

uint16_t bc_run_threshold(uint16_t pspd, uint16_t espd, uint8_t attempts) {
    if(pspd >= espd) return 256;
    uint32_t div = (espd / 4) & 0xFF;
    if(div == 0) return 256;
    uint32_t f = (uint32_t)pspd * 32 / div;
    if(f > 255) return 256;
    f += 30u * attempts;
    if(f > 255) return 256;
    return (uint16_t)f;
}

// capture, contract 14.2. Ball classes: crystal = ball (first draw 0..255, HP
// divisor 12), great (0..200, 8), ultra (0..150, 12), master (always).
uint8_t bc_ball_range(uint8_t ball) {
    static const uint8_t RANGE[3] = {255, 200, 150};
    return ball <= BALL_ULTRA ? RANGE[ball] : 0;
}

bool bc_catch(uint8_t ball, uint8_t rate, uint16_t max_hp, uint16_t hp, uint8_t st, uint8_t n, uint8_t m,
              uint8_t* shakes) {
    if(shakes) *shakes = 3;
    if(ball > BALL_ULTRA) return true; // master
    static const uint8_t DIV[3] = {12, 8, 12};
    uint32_t bonus = (st & (BS_SLP_MASK | BS_FRZ)) ? 25 : (st ? 12 : 0); // sleep / freeze 25, poison / burn / paralysis 12
    if(n < bonus) return true; // the first draw minus the bonus went below zero = caught outright
    n = (uint8_t)(n - bonus);
    uint32_t q = hp / 4;
    if(q < 1) q = 1; // FIX: divided by HP/4 even when it was 0 (HP < 4)
    uint32_t f = (uint32_t)max_hp * 255 / DIV[ball] / q;
    if(f > 255) f = 255;
    if(n <= rate && m <= f) return true;
    // wobble class: Z = X * f / 255 + 5 (sleep / freeze) or 10 (poison / burn / paralysis), X = rate * 100 / range
    uint32_t x = (uint32_t)rate * 100 / bc_ball_range(ball);
    if(x > 255) x = 255;
    uint32_t z = x * f / 255 + ((st & (BS_SLP_MASK | BS_FRZ)) ? 5 : (st ? 10 : 0));
    if(shakes) *shakes = z < 10 ? 0 : z < 30 ? 1 : z < 70 ? 2 : 3;
    return false;
}

// ============================================================================================
// Battle
// ============================================================================================
static const MoveDef STRUGGLE_DEF = {TY_NORMAL, 50, 255, 0, EF_RECOIL, 2, 0, 0, 0, 0}; // (recoil 1/2 of the damage: contract 64 made it the EF_RECOIL effect, divisor 2)

const MoveDef* bc_move_def(const Battle* b, uint8_t id) {
    if(id == MV_STRUGGLE) return &STRUGGLE_DEF;
    return bc_move(b->data, id); // 0 / unknown: the all-zero record
}

static __attribute__((noinline)) Mon* act_mon(const Battle* b, int s) {
    return &b->party[s]->mon[b->side[s].active];
}

uint8_t bc_ecl_can(const Battle* b) {
    const Mon* m = &b->party[0]->mon[b->side[0].active];
    const uint8_t f = bc_ecl_form(m->species);
    return f && m->hp && bc_mon_is_ot(m) && !b->ecl_used && !b->side[0].ecl && !BF_TF_ON(b) && !b->reserve && b->phase == BP_CHOOSE ? f : 0;
}

uint8_t bc_ecl_go(Battle* b) {
    const uint8_t f = bc_ecl_can(b);
    if(f) {
        Side* sd = &b->side[0];
        sd->ecl = f;
        b->ecl_used = 1;
        sd->fl |= FL_TY; // the form's types (ty_of)
        sd->ty[0] = bc_ecl_ty(f, 0);
        sd->ty[1] = bc_ecl_ty(f, 1);
    }
    return f;
}

// The types of the creature of side `s` (contract 73: Typecast, Transform and the Eclipse Form override the species').
static uint8_t ty_of(const Battle* b, int s, int n) {
    const Side* sd = &b->side[s];
    if(sd->fl & FL_TY) return sd->ty[n];
    const SpeciesDef* sp = bc_species(b->data, act_mon(b, s)->species);
    return n ? sp->type2 : sp->type1;
}

static void ev(Battle* b, uint8_t type, uint8_t side, uint8_t a, uint8_t bb, uint16_t v, uint16_t w) {
    // contract 79: the last 4 slots are for what the scene cannot do without: a send-out, the prize, the forced switch and the end (EV_SENDOUT, EV_MONEY, EV_NEED_SWITCH, EV_END); everything else is dropped first
    if(b->n_ev >= BC_MAX_EVENTS || (b->n_ev >= BC_MAX_EVENTS - 4 && type != EV_SENDOUT && (uint8_t)(type - EV_MONEY) > 2)) return;
    BcEvent* e = &b->ev[b->n_ev++];
    e->type = type;
    e->side = side;
    e->a = a;
    e->b = bb;
    e->v = v;
    e->w = w;
}

static __attribute__((noinline)) void ev3(Battle* b, uint8_t type, uint8_t side, uint8_t a) { // an event with only a side and one byte (outlined: ev() takes seven arguments, three of them on the stack)
    ev(b, type, side, a, 0, 0, 0);
}

static __attribute__((noinline)) uint8_t rnd8(Battle* b) { // the battle's random byte / range (outlined: an API call through the app's table costs 8 bytes at every site)
    return bc_rand8(&b->rng);
}
static __attribute__((noinline)) uint32_t rndr(Battle* b, uint32_t lo, uint32_t hi) {
    return bc_rand_range(&b->rng, lo, hi);
}

static void fail(Battle* b, int s) { // "But it failed!"
    ev3(b, EV_FAILED, (uint8_t)s, 0);
}

static void side_reset(Side* sd, uint8_t active) {
    const uint8_t attempts = sd->run_attempts, bdg = sd->bdg;
    memset(sd, 0, sizeof(*sd));
    sd->active = active;
    sd->run_attempts = attempts;
    sd->bdg = bdg;
}

static void do_sendout(Battle* b, int s, uint8_t idx) {
    side_reset(&b->side[s], idx);
    if(s) { // contract 82: a new foe starts with only the creature in play as a participant and a fresh AI (uses, turn count)
        b->part = (uint8_t)(1u << b->side[0].active);
        b->ai_st = 0; // (the trainer AI's uses and turn count start again)
    } else
        b->part |= (uint8_t)(1u << idx); // (the creature that comes in has taken part)
    ev3(b, EV_SENDOUT, (uint8_t)s, idx);
}

static __attribute__((noinline)) bool mv_ok(const Battle* b, int s, int k) { // move slot k of side s holds a move with PP left (outlined: two API-free but large expressions at four sites)
    return bc_mv(b, s, k) && (*bc_ppp(b, s, k) & BC_PP_LEFT);
}

static bool has_pp(const Battle* b, int s) {
    for(int i = 0; i < 4; i++)
        if(mv_ok(b, s, i)) return true;
    return false;
}

static int next_alive(const Party* p, int from_excl, int skip) {
    for(int i = 0; i < p->count; i++)
        if(i != skip && i > from_excl && p->mon[i].hp) return i;
    return -1;
}

static __attribute__((noinline)) int any_alive_other(const Party* p, int skip) {
    for(int i = 0; i < p->count; i++)
        if(i != skip && p->mon[i].hp) return i;
    return -1;
}

bool bc_init(Battle* b, const BcData* d, Party* player, const Party* enemy, bool trainer, uint8_t trainer_id,
             uint32_t seed) {
    memset(b, 0, sizeof(*b));
    b->data = d;
    b->party[0] = player;
    b->enemy = *enemy;
    b->party[1] = &b->enemy;
    b->is_trainer = trainer;
    b->trainer_id = trainer_id;
    b->ai_mod = trainer ? bc_trainer(d, trainer_id)->ai_mod : 0;
    bc_rng_seed(&b->rng, seed);
    int pe = next_alive(&b->enemy, -1, -1), pp = next_alive(player, -1, -1);
    if(pe < 0 || pp < 0) {
        b->phase = BP_OVER;
        return false;
    }
    do_sendout(b, 1, (uint8_t)pe);
    do_sendout(b, 0, (uint8_t)pp);
    b->part = (uint8_t)(1u << pp);
    b->phase = BP_CHOOSE;
    return true;
}

// contract 79: move slot k of side s can be chosen: it holds a move with PP left and it is not the Disabled one (bc_choose, and the scene before it opens Echoing's list)
bool bc_usable(const Battle* b, int s, int k) {
    return mv_ok(b, s, k) && !BF_DIS_OFF(b, s, k);
}

// contract 79: the base PP the maximum of slot k comes from: the creature's OWN move in it (a Echoing copy keeps Echoing's maximum), 5 for a Transformed creature (its slots hold 5 PP)
uint8_t bc_pp_base(const Battle* b, int s, int k) {
    const Side* sd = &b->side[s];
    return BF_TRANSFORM && sd->tf ? 5 : bc_move_def(b, b->party[s]->mon[sd->active].moves[k])->pp;
}

bool bc_forced_go(Battle* b) { // contract 79: the player's creature is locked into a multi-turn move or must recharge: its action is set (ACT_MOVE 0xFF, the core's choice) and true is returned; else false
    if(!BC_FORCED(b, 0)) return false;
    b->act[0] = (BcAction){ACT_MOVE, 0xFF, 0};
    return true;
}

bool bc_choose(Battle* b, BcAction a) {
    if(b->phase == BP_OVER) return false;
    if(b->phase == BP_SWITCH) {
        if(a.kind != ACT_SWITCH || a.arg >= b->party[0]->count || !b->party[0]->mon[a.arg].hp ||
           a.arg == b->side[0].active)
            return false;
        b->act[0] = a;
        return true;
    }
    const bool forced = BC_FORCED(b, 0); // no menu while Rage, Rampage, Hold Fast, a binding move or Fly / Dig's second turn goes on, nor on the recharge turn: only the forced move (like Struggle: 0xFF)
    if(forced && a.kind != ACT_MOVE) return false;
    switch(a.kind) {
    case ACT_MOVE:
        if(forced || !has_pp(b, 0)) {
            b->act[0].kind = ACT_MOVE;
            b->act[0].arg = 0xFF; // struggle
            return true;
        }
        if(a.arg >= 4 || !bc_usable(b, 0, a.arg)) return false; // (a Disabled move is refused)
        b->act[0] = a;
        return true;
    case ACT_RUN:
        if(b->is_trainer) return false; // no running from trainers
        b->act[0] = a;
        return true;
    case ACT_SWITCH:
        if(a.arg >= b->party[0]->count || !b->party[0]->mon[a.arg].hp || a.arg == b->side[0].active) return false;
        b->act[0] = a;
        return true;
    case ACT_ITEM: {
        if(!a.arg || a.arg >= b->data->n_items) return false;
        uint8_t kind = b->data->items[a.arg].kind;
        if(kind == IK_CATCH) {
            if(b->is_trainer) return false; // wasted the ball and the turn; here nothing is lost (the UI says so)
        } else if(kind == IK_HEAL || kind == IK_CURE || kind == IK_REVIVE || kind == IK_RESTORE) { // (contract 64: Life Spark / Life Blaze want a FAINTED creature, Vital Draught a hurt or afflicted one)
            if(a.arg2 >= b->party[0]->count) return false;
            const Mon* t = &b->party[0]->mon[a.arg2];
            const bool alive = t->hp != 0;
            if(kind == IK_REVIVE ? alive : (!alive || (kind == IK_HEAL ? t->hp >= t->max_hp : kind == IK_CURE ? !bc_has_status(t, b->data->items[a.arg].arg) : t->hp >= t->max_hp && !t->status)))
                return false;
#if BF_ITEMS
        } else if(kind != IK_BATTLE && !(BF_FLUTE && a.arg == IT_FLAUTA)) { // (contract 82: the Flute is a key item that works in battle)
#else
        } else {
#endif // (contract 73: the boost items, Mist Veil and Crit Focus need no target)
            return false; // a key item (and anything unknown) is never used in battle
        }
        b->act[0] = a;
        return true;
    }
    default: return false;
    }
}

uint16_t bc_compute_damage(const Battle* b, int s, const MoveDef* mv, bool crit, uint8_t rnd, uint8_t* eff_out) {
    int t = s ^ 1;
    const Mon* am = act_mon(b, s);
    const Mon* dm = act_mon(b, t);
    const Side* as = &b->side[s];
    const Side* ds = &b->side[t];
    bool phys = bc_phys(b->data, mv->type);
    int ai = phys ? STI_ATK : STI_SPC, di = phys ? STI_DEF : STI_SPC;
    BcDmgIn in;
    in.level = am->level;
    in.power = mv->power;
    in.move_type = mv->type;
    in.atk_t1 = ty_of(b, s, 0);
    in.atk_t2 = ty_of(b, s, 1);
    in.def_t1 = ty_of(b, t, 0);
    in.def_t2 = ty_of(b, t, 1);
    in.crit = crit;
    in.rnd = rnd;
    if(crit) { // unmodified stats, screens ignored
        in.a = (uint16_t)RAW_STAT(am, as, ai);
        in.d = (uint16_t)RAW_STAT(dm, ds, di);
    } else {
        in.a = bc_eff_stat(am, as, ai);
        in.d = bc_eff_stat(dm, ds, di);
        if(ds->vol & (phys ? VF_REFLECT : VF_LSCREEN)) in.d = (uint16_t)(in.d * 2);
    }
    if(mv->effect == EF_SUICIDE) in.d >>= 1; // Blow Up / Explosion halve the target's Defense (bc_damage clamps it to >= 1)
    return bc_damage(b->data, &in, eff_out);
}

static void apply_damage(Mon* m, uint16_t dmg) {
    m->hp = dmg >= m->hp ? 0 : (uint16_t)(m->hp - dmg);
}

static void stage_change(Battle* b, int s, int stat, int delta) {
    Side* sd = &b->side[s];
    int cur = sd->stage[stat], nw = cur + delta;
    if(nw > 6) nw = 6;
    if(nw < -6) nw = -6;
    if(delta < 0 && (sd->vol & VF_MIST)) nw = cur; // Mist (contract 64): a drop does nothing (in only the foe lowers a stat)
    if(nw == cur) {
        ev(b, EV_STAT, (uint8_t)s, (uint8_t)stat, 0, 0, (uint16_t)(int16_t)cur);
        return;
    }
    sd->stage[stat] = (int8_t)nw;
    ev(b, EV_STAT, (uint8_t)s, (uint8_t)stat, 0, (uint16_t)(int16_t)(nw - cur), (uint16_t)(int16_t)nw);
}

// ---- contract 64: the events of the new moves
static void ev_hp(Battle* b, int s, uint8_t kind, uint16_t amt) { // EV_RECOIL: the HP of `s` changed (RC_*), `amt` = how much, w = HP after
    ev(b, EV_RECOIL, (uint8_t)s, kind, 0, amt, act_mon(b, s)->hp);
}
static void ev_msg(Battle* b, int s, uint8_t idx, uint8_t num) {
    ev(b, EV_MSG, (uint8_t)s, idx, num, 0, 0);
}
static uint16_t share(uint16_t v, uint8_t div) { // v / div, at least 1 (a corrupt divisor 0 counts as 1)
    uint16_t r = div ? v / div : v;
    return r ? r : 1;
}
static void heal(Battle* b, int s, uint16_t amt, uint8_t kind) { // Recover / drain / Sap Seed / Rest: adds HP up to the maximum
    Mon* m = act_mon(b, s);
    if(amt > (uint16_t)(m->max_hp - m->hp)) amt = (uint16_t)(m->max_hp - m->hp);
    m->hp = (uint16_t)(m->hp + amt);
    ev_hp(b, s, kind, amt);
}

// returns true if the status was inflicted (`toxic`: Toxic, a poison that grows every turn, contract 73)
static bool inflict(Battle* b, int t, uint8_t kind, uint8_t toxic, bool secondary) {
    Mon* tm = act_mon(b, t);
    Side* ts = &b->side[t];
    const uint8_t t1 = ty_of(b, t, 0), t2 = ty_of(b, t, 1);
    bool fail = false;
    if(kind == SK_CONFUSION) {
        fail = (ts->vol & VF_CONF) != 0;
    } else {
        fail = tm->status != 0;
        if(kind == SK_POISON && (t1 == TY_POISON || t2 == TY_POISON)) fail = true;
        if(kind == SK_BURN && (t1 == TY_FIRE || t2 == TY_FIRE)) fail = true;
        if(kind == SK_FREEZE && (t1 == TY_ICE || t2 == TY_ICE)) fail = true;
    }
    if(fail) {
        if(!secondary) ev(b, EV_STATUS, (uint8_t)t, kind, 1, 0, 0);
        return false;
    }
    switch(kind) {
    case SK_CONFUSION:
        ts->vol |= VF_CONF;
        ts->conf_turns = (uint8_t)rndr(b, 2, 5);
        break;
    case SK_SLEEP: tm->status = (uint8_t)rndr(b, 1, 7); break;
    case SK_POISON:
        tm->status = BS_PSN;
#if BF_TOXIC
        ts->tox = 0;
        ts->fl = (uint8_t)((ts->fl & ~FL_TOX) | (toxic ? FL_TOX : 0));
#endif
        break;
    case SK_BURN: tm->status = BS_BRN; break;
    case SK_FREEZE: tm->status = BS_FRZ; break;
    case SK_PARALYSIS: tm->status = BS_PAR; break;
    }
#if BF_RECH
    if(kind == SK_SLEEP || kind == SK_FREEZE) ts->fl &= (uint8_t)~FL_RECH; // a sleeping / frozen target no longer has to recharge
#endif
    ev3(b, EV_STATUS, (uint8_t)t, kind);
    return true;
}

static void run_move(Battle* b, int s, uint8_t mid, uint8_t slot);

#if BF_MIND_WAVE
static uint16_t mind_wave(Battle* b, int s, uint8_t lv) { // contract 82: a random number below 1.5 x the user's level (a byte is re-rolled until it is one); the player's is at least 1, the foe's may be 0. Level 1 has no number: 1
    const unsigned lim = lv + lv / 2u;
    return lim > 1 ? (uint16_t)rndr(b, s == 0, lim - 1) : 1;
}
#else
#define mind_wave(b, s, lv) 1u
#endif

#if BF_CRASH
static void crash(Battle* b, int s) { // Jump Kick / High Kick missed or had no effect: the user takes exactly 1 HP (the damage is damage / 8, which is 0 on a miss, so at least 1)
    apply_damage(act_mon(b, s), 1);
    ev_hp(b, s, RC_CRASH, 1);
}
#endif

static inline bool fx_in(uint32_t mask, uint8_t fx) { // a set of effects as a bit mask (a damaged record may hold any byte)
    return fx < 32 && ((mask >> fx) & 1);
}

#define TGT_FX ((1u << EF_STAT_TGT) | (1u << EF_STATUS_TGT) | (1u << EF_LEECH)) // effects aimed at the target: a decoy stops them (contract 64)

static void apply_effect(Battle* b, int s, int t, const MoveDef* mv, bool secondary, uint8_t slot) { // slot: the move slot in use (Echoing's), 0xFE when the move was called by another
    if(mv->chance && rnd8(b) >= mv->chance) return;
    const uint8_t fx = mv->effect;
    if(fx_in(TGT_FX, fx) && b->side[t].sub) { // a decoy stops it
        if(!secondary) fail(b, s);
        return;
    }
    switch(fx) {
    case EF_STAT_TGT: stage_change(b, t, mv->arg0, mv->arg1); break;
    case EF_STAT_SELF: stage_change(b, s, mv->arg0, mv->arg1); break;
    case EF_STATUS_TGT: inflict(b, t, mv->arg0, (uint8_t)mv->arg1, secondary); break; // (arg1 != 0: Toxic)
    case EF_FOCUS:
        if(b->side[s].vol & VF_FOCUS) fail(b, s);
        else {
            b->side[s].vol |= VF_FOCUS;
            ev3(b, EV_FOCUS, (uint8_t)s, 0);
        }
        break;
    case EF_SCREEN: {
        uint8_t f = mv->arg0 == SCR_REFLECT ? VF_REFLECT : VF_LSCREEN;
        if(b->side[s].vol & f) fail(b, s);
        else {
            b->side[s].vol |= f;
            ev3(b, EV_SCREEN, (uint8_t)s, mv->arg0);
        }
        break;
    }
    case EF_FLINCH:
        if(!b->side[t].acted) {
            b->side[t].vol |= VF_FLINCH;
#if BF_RECH
            b->side[t].fl &= (uint8_t)~FL_RECH; // it flinches instead of recharging
#endif
        }
        break;
    case EF_HEAL: { // Recover / Hearty Broth: half the maximum HP; Rest: all of it, the status goes and the creature sleeps 2 turns
        Mon* m = act_mon(b, s);
        if(m->hp >= m->max_hp) fail(b, s);
        else if(mv->arg0) {
            m->status = 0;
            heal(b, s, m->max_hp, RC_NAP);
            m->status = 2;
            ev3(b, EV_STATUS, (uint8_t)s, SK_SLEEP);
        } else
            heal(b, s, m->max_hp / 2 ? m->max_hp / 2 : 1, RC_REGAIN);
        break;
    }
    case EF_FLEE: // Teleport / Roar / Whirlwind: a wild battle ends (the scene says so); against a trainer it fails
        if(b->is_trainer) fail(b, s);
        else {
            ev_msg(b, s, MSG_FLED, 0);
            b->result = BR_RAN;
        }
        break;
    case EF_STAT_RESET:
        for(Side* sd = b->side; sd < b->side + 2; sd++) {
            memset(sd->stage, 0, sizeof(sd->stage));
            sd->vol = 0;
            sd->fl &= (uint8_t)~FL_TOX; // the toxic poison turns into a plain one ...
#if BF_DISABLE
            sd->dis = 0; // ... and Disable ends
#endif
        }
        ev_msg(b, s, MSG_STAT_RESET, 0);
        break;
    case EF_MIST:
        if(b->side[s].vol & VF_MIST) fail(b, s);
        else {
            b->side[s].vol |= VF_MIST;
            ev_msg(b, s, MSG_MIST, 0);
        }
        break;
    case EF_LEECH: {
        if((b->side[t].vol & VF_SEED) || ty_of(b, t, 0) == TY_GRASS || ty_of(b, t, 1) == TY_GRASS) fail(b, s);
        else {
            b->side[t].vol |= VF_SEED;
            ev_msg(b, t, MSG_SEEDED, 0);
        }
        break;
    }
    case EF_SUBSTITUTE: {
        Mon* m = act_mon(b, s);
        uint16_t c = m->max_hp / 4 ? m->max_hp / 4 : 1;
        if(b->side[s].sub || m->hp <= c) fail(b, s);
        else {
            apply_damage(m, c);
            b->side[s].sub = (uint8_t)(c > 255 ? 255 : c);
            ev_hp(b, s, RC_SUB, c);
        }
        break;
    }
    case EF_MOVE_CALL: { // Wildcard: a random move; Parrot: the target's last move; Echoing (arg0 2): the move copied into its own slot. The called move is used on the spot, no PP.
#if BF_MOVE_COPY
        if(mv->arg0 == 2) { // Echoing (contract 73): the player picked one of the target's moves in the menu (BcAction.arg2), the foe takes one at random; it replaces Echoing in its slot until the battle ends (the PP stay)
            Side* us = &b->side[s];
            const Side* ts = &b->side[t];
            uint8_t k = s == 0 ? (uint8_t)(b->act[0].arg2 & 3) : (uint8_t)rndr(b, 0, 3);
            for(int i = 0; s && i < 4 && !bc_mv(b, t, k); i++) k = (uint8_t)((k + 1) & 3); // (a creature holds its moves from slot 0 on: the foe's pick is always a real one; a damaged record with none fails)
            const uint8_t id = bc_mv(b, t, k);
            if(slot > 3 || (ts->lock & 0x80) || !id) fail(b, s);
            else {
                us->ovm |= (uint8_t)(1 << slot);
                us->mv[slot] = id;
                ev_msg(b, s, MSG_MOVE_COPY, id);
            }
            break;
        }
#endif
        uint8_t id = mv->arg0 ? b->side[t].last : (uint8_t)rndr(b, 1, (uint32_t)b->data->n_moves - 1);
        if(!id || id >= b->data->n_moves || bc_move_def(b, id)->effect == EF_MOVE_CALL) fail(b, s); // (`mv` is not used again: the move cache may have dropped it)
        else run_move(b, s, id, 0xFE);
        break;
    }
#if BF_DISABLE
    case EF_DISABLE: { // a random move of the target, 1 .. 8 of ITS turns; fails when it has one disabled already
        Side* ts = &b->side[t];
        if(ts->dis) {
            fail(b, s);
            break;
        }
        uint8_t k = 0;
        int tries = 32;
        while(tries-- && !bc_mv(b, t, k = rnd8(b) & 3)) {} // (a damaged record with no move at all fails instead of looping)
        if(!mv_ok(b, t, k)) { // 
            fail(b, s);
            break;
        }
        ts->dis = (uint8_t)(((k + 1) << 4) | ((rnd8(b) & 7) + 1));
        ev_msg(b, t, MSG_DISABLED, bc_mv(b, t, k));
        break;
    }
#endif
#if BF_CONVERT || BF_TRANSFORM
#if BF_CONVERT
    case EF_CONVERT: // Typecast: the user takes the target's types
#endif
#if BF_TRANSFORM
    case EF_TRANSFORM: // the species (picture), types, the four moves with 5 PP each, Atk / Def / Spd / Spc and the stat stages of the target; not the HP, level or status. Ends with the switch / the battle: nothing is saved
#endif
    {
        Side* us = &b->side[s];
        const Side* ts = &b->side[t];
        if(ts->lock & 0x80) { // 
            fail(b, s);
            break;
        }
        us->fl |= FL_TY;
        us->ty[0] = ty_of(b, t, 0);
        us->ty[1] = ty_of(b, t, 1);
#if BF_TRANSFORM
        if(fx == EF_TRANSFORM) {
            const Mon* tm = act_mon(b, t);
            for(int i = 0; i < 4; i++) {
                us->tst[i] = (uint16_t)RAW_STAT(tm, ts, i);
                us->mv[i] = bc_mv(b, t, i);
                us->pp5[i] = us->mv[i] ? 5 : 0;
            }
            memcpy(us->stage, ts->stage, sizeof(us->stage));
            us->ovm = 0xF;
            us->ecl = 0;
            us->tf = ts->tf ? ts->tf : tm->species;
            ev_msg(b, s, MSG_TRANSFORM, us->tf);
            break;
        }
#endif
        ev_msg(b, s, MSG_CONVERT, 0);
        break;
    }
#endif
    default: break;
    }
}

static void self_hit(Battle* b, int s) { // the creature hurts itself (confusion; contract 73: a disobedient one): a typeless power-40 hit of its own Attack against its own Defense
    Mon* m = act_mon(b, s);
    const Side* sd = &b->side[s];
    BcDmgIn in;
    memset(&in, 0, sizeof(in));
    in.level = m->level;
    in.power = 40;
    in.move_type = TY_NONE;
    in.a = bc_eff_stat(m, sd, STI_ATK);
    in.d = bc_eff_stat(m, sd, STI_DEF);
    in.rnd = 255;
    uint16_t dmg = bc_damage(b->data, &in, NULL);
    apply_damage(m, dmg);
    ev(b, EV_SELF_HIT, (uint8_t)s, 0, 0, dmg, m->hp);
}

// Status checks before a creature acts. Returns true if it may use its move.
static bool can_act(Battle* b, int s) {
    Mon* m = act_mon(b, s);
    Side* sd = &b->side[s];
    if(sd->vol & VF_TRAP) { // held by a Bind-like move (contract 64): it cannot act while the other side keeps it up
        if(b->side[s ^ 1].lock) {
            ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_TRAPPED);
            return false;
        }
        sd->vol &= (uint8_t)~VF_TRAP;
    }
    if(m->status & BS_SLP_MASK) {
        uint8_t n = (m->status & BS_SLP_MASK) - 1;
        m->status = (uint8_t)((m->status & ~BS_SLP_MASK) | n);
        if(n == 0) ev3(b, EV_CURED, (uint8_t)s, SK_SLEEP); // wakes up; the turn is lost
        else ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_SLEEP);
        return false;
    }
    if(m->status & BS_FRZ) {
        ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_FROZEN);
        return false;
    }
    if(sd->vol & VF_FLINCH) {
        sd->vol &= (uint8_t)~VF_FLINCH;
        ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_FLINCH);
        return false;
    }
#if BF_RECH
    if(sd->fl & FL_RECH) { // Mega Beam (contract 73): the turn after it is lost
        sd->fl &= (uint8_t)~FL_RECH;
        ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_RECHARGE);
        return false;
    }
#endif
#if BF_DISABLE
    if(sd->dis && !(--sd->dis & 15)) { // Disable: the counter runs down on the creature's own turns
        sd->dis = 0;
        ev_msg(b, s, MSG_UNDIS, 0);
    }
#endif
    if(sd->vol & VF_CONF) {
        sd->conf_turns--;
        if(sd->conf_turns == 0) {
            sd->vol &= (uint8_t)~VF_CONF;
            ev3(b, EV_CURED, (uint8_t)s, SK_CONFUSION);
        } else {
            ev3(b, EV_CONF_TURN, (uint8_t)s, 0);
            if(rnd8(b) < 128) {
                self_hit(b, s);
                return false;
            }
        }
    }
    if((m->status & BS_PAR) && rnd8(b) < 63) {
        ev3(b, EV_CANT_MOVE, (uint8_t)s, CM_PARALYZED);
        return false;
    }
    return true;
}

// Self-targeted effects: no accuracy check (Hold Fast's turns, Mist, Whiteveil, Recover, a decoy, Wildcard ...)
#define SELF_FX ((1u << EF_STAT_SELF) | (1u << EF_FOCUS) | (1u << EF_SCREEN) | (1u << EF_HEAL) | (1u << EF_STAT_RESET) | (1u << EF_MIST) | (1u << EF_MOVE_CALL) | (1u << EF_STORE_DMG) | (1u << EF_SUBSTITUTE) | (BF_TRANSFORM ? 1u << EF_TRANSFORM : 0))

// Everything a move does once it is used (the "X used Y!" line onwards): the creature `s` uses move `mid` (Wildcard / Parrot call this again for the called move).
static void run_move(Battle* b, int s, uint8_t mid, uint8_t slot) {
    const int t = s ^ 1;
    Mon* m = act_mon(b, s);
    Mon* tm = act_mon(b, t);
    Side* sd = &b->side[s];
    Side* ts = &b->side[t];
    const MoveDef* mv = bc_move_def(b, mid);
    const uint8_t fx = mv->effect;
    const bool cont = sd->lock != 0; // a multi-turn move goes on: no set-up this turn
    ev(b, EV_USE_MOVE, (uint8_t)s, mid, slot, 0, 0);
    sd->last = mid;
#if BF_RECH
    if(fx == EF_LOCK && mv->arg0 && !cont) ts->fl &= (uint8_t)~FL_RECH; // a binding move used on a recharging target ends its recharge (even if it misses)
#endif
    if(fx == EF_SUICIDE) apply_damage(m, m->hp); // Blow Up / Explosion: the user is gone whatever happens next
    if(fx == EF_DRAIN && mv->arg1 && !(tm->status & BS_SLP_MASK)) { // Dream Feast: only a sleeping target
        fail(b, s);
        return;
    }
    if(fx == EF_STORE_DMG) { // 2 .. 3 turns of storing, then twice the damage taken is returned
        if(!cont) {
            sd->lock = (uint8_t)rndr(b, 1, 2);
            sd->lmove = mid;
            sd->store = 0;
        } else if(--sd->lock == 0) {
            uint32_t d = (uint32_t)sd->store * 2;
            if(d > tm->hp) d = tm->hp;
            ev_msg(b, s, MSG_UNLEASH, 0);
            if(d) {
                apply_damage(tm, (uint16_t)d);
                ev(b, EV_DAMAGE, (uint8_t)t, 0, 10, (uint16_t)d, tm->hp);
            } else
                fail(b, s);
            return;
        }
        ev_msg(b, s, MSG_STORE, 0);
        return;
    }
    if(fx == EF_CHARGE) {
        if(!cont) { // the first turn only gathers (Fly: out of reach meanwhile)
            sd->lock = (uint8_t)(1 | (mv->arg0 << 7));
            sd->lmove = mid;
            ev_msg(b, s, (uint8_t)mv->arg1, 0);
            return;
        }
        sd->lock = 0; // now it hits
    }
    if(!fx_in(SELF_FX, fx)) {
        uint16_t thr = bc_hit_threshold(mv->acc, sd->stage[STI_ACC], ts->stage[STI_EVA], mv->flags & MF_NEVER_MISS);
        bool miss = ((ts->lock & 0x80) && !(mv->flags & MF_NEVER_MISS)) || (fx == EF_OHKO && bc_eff_stat(m, sd, STI_SPD) < bc_eff_stat(tm, ts, STI_SPD)); // out of reach (Fly, Dig; contract 82: only a never-miss move, Swift, reaches it) / a slower user's one-hit KO
        if(miss || (thr < 256 && rnd8(b) >= thr)) {
            ev3(b, EV_MISS, (uint8_t)s, 0);
            sd->lock = 0;
#if BF_CRASH
            if(fx == EF_CRASH) crash(b, s);
#endif
            return;
        }
    }
    if(mv->power) {
        bool crit = rnd8(b) <
                    bc_crit_threshold(bc_species(b->data, m->species)->base[STI_SPD + 1], mv->flags & MF_HIGH_CRIT, sd->vol & VF_FOCUS);
        uint8_t rnd = (uint8_t)rndr(b, 217, 255);
        uint8_t eff;
        uint16_t dmg = bc_compute_damage(b, s, mv, crit, rnd, &eff);
        if(fx == EF_FIXED) dmg = !eff ? 0 : mv->arg0 == 255 ? (uint16_t)((tm->hp + 1) / 2) : mv->arg0 == 254 ? mind_wave(b, s, m->level) : mv->arg0 ? mv->arg0 : m->level;
        else if(fx == EF_OHKO) dmg = eff ? tm->hp : 0;
        else if(fx == EF_COUNTER) {
            if(!sd->hurt) {
                fail(b, s);
                return;
            }
            dmg = eff ? (uint16_t)(sd->hurt * 2) : 0;
        }
        uint8_t n = 1;
        if(fx == EF_MULTI) {
            n = mv->arg0;
            if(!n) {
                const uint8_t r = rnd8(b);
                n = (uint8_t)(2 + (r >= 96) + (r >= 192) + (r >= 224)); // 3/8, 3/8, 1/8, 1/8
            }
        }
        uint16_t total = 0;
        uint8_t h = 0;
        bool subhit = false;
        while(h < n && tm->hp) {
            uint16_t d = dmg > tm->hp ? tm->hp : dmg;
            if(ts->sub && d) { // the decoy takes it
                if(d > ts->sub) d = ts->sub;
                ts->sub = (uint8_t)(ts->sub - d);
                subhit = true;
            } else
                apply_damage(tm, d);
            ev(b, EV_DAMAGE, (uint8_t)t, h ? 0 : crit, h ? 10 : eff, d, tm->hp);
            total = (uint16_t)(total + d);
            h++;
        }
        if(n > 1) ev_msg(b, t, MSG_HITS, h);
#if BF_COIN_HIT
        if(fx == EF_COIN_HIT && total) { // contract 82: 2 x the user's level scatter at every hit, whoever uses it; EV_MONEY pays the total at the end
            b->pay = (uint16_t)(b->pay + m->level * 2);
            ev_msg(b, s, MSG_COINS, (uint8_t)(m->level * 2));
        }
#endif
        if(fx == EF_OHKO && total) ev_msg(b, s, MSG_OHKO, 0);
        if(subhit) {
            ev_msg(b, t, MSG_SUBHIT, 0);
            if(!ts->sub) ev_msg(b, t, MSG_SUBEND, 0);
        }
        if(total && (tm->status & BS_FRZ) && mv->type == TY_FIRE) {
            tm->status &= (uint8_t)~BS_FRZ;
            ev3(b, EV_CURED, (uint8_t)t, SK_FREEZE);
        }
        if(total && (mv->type == TY_NORMAL || mv->type == TY_FIGHTING)) ts->hurt = total; // Counter
        if(ts->lock && ts->lmove == MV_BROOD) ts->store = (uint16_t)(ts->store + total);  // Hold Fast
        if(fx == EF_RECOIL && total) {
            uint16_t r = share(total, mv->arg0);
            apply_damage(m, r);
            ev_hp(b, s, RC_RECOIL, r);
        }
        if(fx == EF_DRAIN && total) heal(b, s, share(total, mv->arg0), RC_ABSORB);
#if BF_RECH
        if(fx == EF_RECHARGE && total && tm->hp && !(subhit && !ts->sub)) sd->fl |= FL_RECH; // Mega Beam: not when it KOs the target, breaks its decoy or does nothing (a miss never gets here)
#endif
#if BF_RAGE
        if(fx == EF_RAGE) { // Rage: the user is locked into it (no PP) like Rampage, but with no end: until it is switched out or faints
            sd->lock = LOCK_RAGE;
            sd->lmove = mid;
        }
        if(total && ts->lock == LOCK_RAGE && tm->hp && ts->stage[STI_ATK] < 6) { // the target is raging: its Attack builds each time it is hit
            ev_msg(b, t, MSG_RAGE, 0);
            stage_change(b, t, STI_ATK, 1);
        }
#endif
#if BF_CRASH
        if(fx == EF_CRASH && !eff) crash(b, s);
#endif
        if(fx == EF_LOCK) { // Rampage / Petal Waltz: 2 .. 3 turns, then confused; Bind and its kin: 2 .. 5 turns, the target held
            if(!cont) {
                sd->lock = (uint8_t)rndr(b, 1, mv->arg0 ? 4 : 2);
                sd->lmove = mid;
                if(mv->arg0) ts->vol |= VF_TRAP;
            } else if(--sd->lock == 0) {
                if(mv->arg0) ts->vol &= (uint8_t)~VF_TRAP;
                else inflict(b, s, SK_CONFUSION, 0, true);
            }
        }
        if(fx != EF_NONE && eff > 0 && tm->hp > 0 && m->hp > 0) apply_effect(b, s, t, mv, true, slot);
    } else if(fx != EF_NONE) {
        apply_effect(b, s, t, mv, false, slot);
    } else {
        fail(b, s);
    }
}

#if BF_OBEY
// Contract 73: a creature that came from another trainer (MX_TRADED) and is above the level the player's badges allow (Battle.cap: 10 / 30 / 50 / 70, none from the 8th) may not obey. draws:
// a byte below (level + cap) is under `cap` -> it obeys; a second one under `cap` -> it uses ANOTHER move; else a third byte decides between a nap (below level - cap), a hit on itself ("won't obey") and doing nothing.
// Returns true when it does NOT use the chosen move (a line was said); "another move" changes *slot and returns false.
static bool disobeys(Battle* b, Mon* m, uint8_t* slot) {
    const unsigned cap = b->cap, lv = m->level, lim = lv + cap > 255 ? 255 : lv + cap;
    if(rndr(b, 0, lim - 1) < cap) return false;
    if(rndr(b, 0, lim - 1) < cap) {
        for(int t = 0; t < 24; t++) { // (rerolls until it finds one: a creature with nothing else to use does nothing)
            const uint8_t k = rnd8(b) & 3;
            if(k != *slot && mv_ok(b, 0, k) && !BF_DIS_ON(b, 0)) {
                *slot = k;
                return false;
            }
        }
    } else {
        const unsigned over = lv - cap, r = rnd8(b);
        if(r < over) { // it naps
            m->status = (uint8_t)rndr(b, 1, 7);
            ev3(b, EV_STATUS, 0, SK_SLEEP);
            ev_msg(b, 0, MSG_NAP, 0);
            return true;
        }
        if(r - over < over) { // it will not obey: it hurts itself
            ev_msg(b, 0, MSG_NOOBEY, 0);
            self_hit(b, 0);
            return true;
        }
    }
    ev_msg(b, 0, (uint8_t)(MSG_LOAF + (rnd8(b) & 3)), 0); // loafing around / won't obey / turned away / ignored orders
    return true;
}

#endif
static __attribute__((noinline)) void exec_move(Battle* b, int s, uint8_t slot) {
    Mon* m = act_mon(b, s);
    Side* sd = &b->side[s];
    sd->acted = 1;
#if BF_AI_MOD2
    if(s && !(b->ai_st & 0x20)) b->ai_st += 0x10; // contract 82: the foe's turns this creature has had, stops at 2
#endif
    if(!can_act(b, s)) {
        sd->lock = 0; // asleep, frozen, flinching, paralysed, confused into itself or held: the multi-turn move is broken off
        return;
    }
    uint8_t mid;
    if(sd->lock) {
        mid = sd->lmove; // the multi-turn move goes on (the chosen move is ignored, no PP)
    } else if(slot == 0xFF) {
        mid = MV_STRUGGLE;
    } else {
#if BF_OBEY
        if(s == 0 && b->cap != 255 && !bc_mon_is_ot(m) && m->level > b->cap && !b->no_reward && disobeys(b, m, &slot)) return;
#endif
        mid = bc_mv(b, s, slot);
        (*bc_ppp(b, s, slot))--;
    }
    run_move(b, s, mid, slot);
}

// ---- enemy choice ----------------------------------------------------------------------------
#if BF_AI_MOD2 || BF_AI_MOD3
static void ai_mods(Battle* b, uint8_t* score); // battle_ai.c
#endif
static bool status_only_ailment(const MoveDef* mv) {
    return mv->power == 0 && mv->effect == EF_STATUS_TGT && mv->arg0 != SK_CONFUSION;
}

static void enemy_choose(Battle* b) {
    const Side* es = &b->side[1];
    const Mon* tm = act_mon(b, 0);
    b->act[1].kind = ACT_MOVE;
    if(BC_FORCED(b, 1) || !has_pp(b, 1)) {
        b->act[1].arg = 0xFF;
        return;
    }
    uint8_t score[4], best = 255;
    for(int i = 0; i < 4; i++) {
        score[i] = 255;
        if(!mv_ok(b, 1, i)) continue;
        score[i] = 10;
        if(b->is_trainer && (b->ai_mod & 1) && tm->status && status_only_ailment(bc_move_def(b, bc_mv(b, 1, i))))
            score[i] += 5;
#if BF_DISABLE
        if(b->side[1].dis >> 4 == i + 1) score[i] = 100; // a Disabled move is picked only when nothing else is left
#endif
    }
#if BF_AI_MOD2 || BF_AI_MOD3
    if(b->is_trainer) ai_mods(b, score); // contract 82: modifiers 2 and 3
#endif
    for(int i = 0; i < 4; i++)
        if(score[i] < best) best = score[i];
    uint8_t cand[4], n = 0;
    for(int i = 0; i < 4; i++)
        if(score[i] == best) cand[n++] = (uint8_t)i;
    b->act[1].arg = cand[n > 1 ? rndr(b, 0, n - 1) : 0];
}

// ---- running ---------------------------------------------------------------------------------
static bool try_run(Battle* b) {
    Side* ps = &b->side[0];
    if(ps->run_attempts < 255) ps->run_attempts++;
    uint16_t ps_spd = bc_eff_stat(act_mon(b, 0), ps, STI_SPD);
    uint16_t es_spd = bc_eff_stat(act_mon(b, 1), &b->side[1], STI_SPD);
    uint16_t thr = bc_run_threshold(ps_spd, es_spd, ps->run_attempts);
    if(thr >= 256 || rnd8(b) < thr) {
        ev3(b, EV_RUN_OK, 0, 0);
        return true;
    }
    ev3(b, EV_RUN_FAIL, 0, 0);
    return false;
}

// ---- experience ------------------------------------------------------------------------------
// Contract 65 (friendship): one function for the three places it moves in a battle (an outlined copy of bc_friend_add costs less than three inlined ones): d = +1 it took part in beating the foe, 0 = the gain of
// a level-up (bc_friend_level_gain), -3 it fainted.
static __attribute__((noinline)) void fr_event(Mon* m, int d) {
    bc_friend_add(m, d ? d : bc_friend_level_gain(m));
}

// Gives `gain` experience points to party member `idx` and levels it up (the events carry `idx`; contract 74: a creature that is NOT in play only learns a move while it has a free slot: no question is asked for it).
static void exp_add(Battle* b, uint8_t idx, uint32_t gain, bool play) { // play: it is the creature in play
    Mon* m = &b->party[0]->mon[idx];
    const SpeciesDef* ms = bc_species(b->data, m->species);
    uint32_t cap = bc_exp_for_level(ms->growth, 100);
    const uint8_t lv0 = m->level;
    m->exp += gain;
    if(m->exp > cap) m->exp = cap;
    while(m->level < 100 && m->exp >= bc_exp_for_level(ms->growth, (uint8_t)(m->level + 1))) {
        uint16_t old_max = m->max_hp;
        m->level++;
        fr_event(m, 0); // contract 65
        bc_mon_recalc(b->data, m);
        m->hp = (uint16_t)(m->hp + (m->max_hp - old_max));
        if(play) { // (a creature that is NOT in play says its level-up ONCE, after the loop, with its last level: the Reparto Total gives five of them a string of levels, and one event each would flood the step; its learns come first)
#if BF_MOVE_COPY
            if(!BF_TF_ON(b)) b->side[0].ovm = 0; // contract 79: the creature in play levelled up: a Echoing copy is gone (a KO without a level-up, or a level-up on the bench, keeps it)
#endif
            ev(b, EV_LEVELUP, 0, idx, m->level, 0, 0);
        }
        for(uint8_t i = 0; i < ms->n_learn; i++) {
            if(ms->learn_lv[i] != m->level) continue;
            uint8_t slot = 0;
            uint8_t r = bc_learn_move(b->data, m, ms->learn_mv[i], &slot);
            if(r != 0xFF && (r != BC_LEARN_FULL || play)) ev(b, EV_LEARN, 0, ms->learn_mv[i], slot, r, idx);
        }
    }
    if(!play && m->level != lv0) ev(b, EV_LEVELUP, 0, idx, m->level, 0, 0);
}

// One share of the experience for `m`: the foe's base stats / n go into its stat experience (capped at 65535), its base experience / n x level / 7 is the experience (x 3/2 for a creature the player did not raise (MX_TRADED), x 3/2 in a trainer battle).
static __attribute__((noinline)) uint32_t xp_share(const Battle* b, Mon* m, const uint8_t* bs, unsigned n, uint8_t elv) {
    for(int i = 0; i < 5; i++) {
        const uint32_t v = (uint32_t)m->statexp[i] + bs[i] / n;
        m->statexp[i] = (uint16_t)(v > 0xFFFF ? 0xFFFF : v);
    }
    const uint32_t g = bc_exp_gain((uint8_t)(bs[5] / n), elv, !bc_mon_is_ot(m));
    return b->is_trainer ? g + g / 2 : g;
}

// Contract 82 (shared experience): every creature that took part in the fight against the fallen foe (Battle.part) and is still able gets the foe's experience and stat experience DIVIDED by the number of participants (each base stat and the
// base experience, rounded down one by one). The Reparto Total (contract 74, Team Share) halves the figures first, and then gives every living creature a second share, the figures divided once more by the size of the party (divides in place: floor(floor(a / b) / c) =
// floor(a / (b c)), so each share is the base figure over ONE divisor). Only participants say their own line; the others have one line for them all (contract 74).
static __attribute__((noinline)) void award_exp(Battle* b, const Mon* fallen) {
    if(b->no_reward) return;
    Party* p = b->party[0];
    uint8_t bs[6], np = 0;
    const SpeciesDef* es = bc_species(b->data, fallen->species);
    memcpy(bs, es->base, 5);
    bs[5] = es->base_exp;
    for(uint8_t k = 0; k < p->count; k++) np += (b->part >> k) & 1;
    const unsigned n1 = (b->share ? 2u : 1u) * (np ? np : 1u), n2 = n1 * p->count;
    bool first = true;
    for(int pass = 0; pass < 2; pass++) { // 0: the participants, 1: the others (only with the Reparto Total)
        for(uint8_t k = 0; k < p->count; k++) {
            Mon* m = &p->mon[k];
            const bool in = (b->part >> k) & 1;
            if(!m->hp || m->level >= 100 || (pass ? in || !b->share : !in)) continue;
            uint32_t g = 0;
            if(!pass) {
                g = xp_share(b, m, bs, n1, fallen->level);
                fr_event(m, 1); // contract 65: it took part in beating the foe
            }
            if(b->share) g += xp_share(b, m, bs, n2, fallen->level);
            if(!pass || first) { // a participant's line; the others' one line (the first one's figure), then their level-ups
                first = pass == 0;
                ev(b, EV_EXP, 0, pass ? 0xFF : k, 0, (uint16_t)g, 0); /* (g <= 2 x 1.5 x 1.5 x 255 x 255 / 7 < 65536) */
            }
            exp_add(b, k, g, k == b->side[0].active); // (contract 79: only the creature in play loses its Echoing copy when it levels up)
        }
    }
    // stat exp only changes the stats at the next level-up (recalc happens in the loop of exp_add)
}

// ---- prize money and status cures (contract 16) -------------------------------------------
static void money_ev(Battle* b, uint32_t prize) { // EV_MONEY: the scene pays it and says it after the win text (nothing when it is 0)
    if(prize) ev(b, EV_MONEY, 0, 0, 0, (uint16_t)(prize & 0xFFFF), (uint16_t)(prize >> 16));
}
uint32_t bc_prize(uint16_t base_money, uint8_t last_level) {
    uint32_t v = (uint32_t)base_money * last_level;
    return v > BC_PRIZE_MAX ? BC_PRIZE_MAX : v;
}

// ---- faints ----------------------------------------------------------------------------------
// Processes fainted creatures. Returns true when the turn must stop (battle over or forced switch).
static bool resolve_faints(Battle* b) {
    bool enemy_out = false;
    for(int pass = 0; pass < 2; pass++) {
        int s = pass == 0 ? 1 : 0; // enemy first (exp is awarded before a simultaneous player faint)
        Mon* m = act_mon(b, s);
        Side* sd = &b->side[s];
        if(m->hp || sd->fainted_done) continue;
        sd->fainted_done = 1;
        m->status = 0;
        sd->vol = 0;
        sd->lock = sd->sub = sd->fl = 0;
        ev3(b, EV_FAINT, (uint8_t)s, 0);
        if(s == 0) {
            b->part &= (uint8_t)~(1u << sd->active); // contract 82: a creature that fell no longer shares the experience
            if(!b->no_reward) fr_event(m, -3); // contract 65: it fell (not in an exhibition battle: those fight copies)
        }
        if(s == 1) {
            award_exp(b, m);
            int nx = next_alive(&b->enemy, sd->active, -1);
            if(nx < 0) nx = any_alive_other(&b->enemy, sd->active);
            if(nx >= 0) {
                do_sendout(b, 1, (uint8_t)nx);
                b->side[1].acted = 1; // (contract 75): the replacement does not attack in the turn it is sent out
            } else enemy_out = true;
        }
    }
    bool player_out = b->side[0].fainted_done && any_alive_other(b->party[0], b->side[0].active) < 0;
    if(player_out) b->result = BR_LOSE; // the player loses even if the foe fell too
    else if(enemy_out) b->result = BR_WIN;
    if(b->result == BR_WIN && !b->no_reward) {
        // base money x the level of the opponent's last creature (no prize from a wild creature) + the coins Pilfer scattered (contract 82)
        uint32_t prize = BF_COIN_HIT ? b->pay : 0;
        if(b->is_trainer && b->trainer_id < b->data->n_trainers) prize += bc_prize(bc_trainer(b->data, b->trainer_id)->money, b->enemy.count ? b->enemy.mon[b->enemy.count - 1].level : 0);
        money_ev(b, prize);
    }
    if(b->result != BR_NONE) {
        b->phase = BP_OVER;
        ev3(b, EV_END, 0, b->result);
        return true;
    }
    if(b->side[0].fainted_done) {
        b->phase = BP_SWITCH;
        ev3(b, EV_NEED_SWITCH, 0, 0);
        return true;
    }
    return false;
}

static int move_priority(const Battle* b, int s) {
    if(b->act[s].arg >= 4) return 0; // Struggle / a move that goes on
    return bc_move_def(b, bc_mv(b, s, b->act[s].arg))->priority;
}

static void residual(Battle* b, int s) {
    Mon* m = act_mon(b, s);
    const bool seeded = (b->side[s].vol & VF_SEED) != 0;
    if(!m->hp || !((m->status & (BS_PSN | BS_BRN)) || seeded)) return;
    uint16_t dmg = m->max_hp / 16;
    if(dmg < 1) dmg = 1;
    if(m->status & (BS_PSN | BS_BRN)) {
        Side* sd = &b->side[s];
#if BF_TOXIC
        if((sd->fl & FL_TOX) && (m->status & BS_PSN)) dmg = (uint16_t)(dmg * (sd->tox < 255 ? ++sd->tox : 255)); // Toxic: N / 16 of the max HP, N = 1, 2, 3 ...
#endif
        if(dmg > m->hp) dmg = m->hp;
        apply_damage(m, dmg);
        ev(b, EV_RESIDUAL, (uint8_t)s, (m->status & BS_PSN) ? SK_POISON : SK_BURN, 0, dmg, m->hp);
    }
    if(seeded && m->hp) { // Sap Seed (contract 64): 1/16 of the maximum HP goes to the other creature
        if(dmg > m->hp) dmg = m->hp;
        apply_damage(m, dmg);
        ev_hp(b, s, RC_SEED, dmg);
        if(act_mon(b, s ^ 1)->hp) heal(b, s ^ 1, dmg, RC_ABSORB);
    }
}

static void cure(Battle* b, int sd, Mon* t, uint8_t idx, uint8_t item, uint8_t kind) { // a status cure: the creature's status goes (kind 0: whichever it had)
    const uint8_t st = t->status;
    t->status = 0;
    ev(b, EV_CURE, (uint8_t)sd, idx, item, kind ? kind : (st & BS_SLP_MASK) ? SK_SLEEP : (st & BS_PSN) ? SK_POISON : (st & BS_BRN) ? SK_BURN : (st & BS_FRZ) ? SK_FREEZE : SK_PARALYSIS, 0);
}

// An item on creature `idx` of side `sd` (the player's from the bag, or, contract 82, the trainer AI's): healing, Life Spark / Vital Draught, a status cure, an X item / Mist Veil / Crit Focus. false: it is none of those.
static bool item_on(Battle* b, int sd, uint8_t item, uint8_t idx) {
    const ItemDef* it = &b->data->items[item];
    Mon* t = &b->party[sd]->mon[idx];
    if(it->kind == IK_HEAL || it->kind == IK_REVIVE || it->kind == IK_RESTORE) { // contract 64: Life Spark (half HP) / Life Blaze (all) bring a fainted creature back; Vital Draught: all the HP and the status
        uint16_t room = (uint16_t)(t->max_hp - t->hp), amt = it->arg == 255 || it->kind == IK_RESTORE ? room : it->kind == IK_REVIVE ? (uint16_t)(t->max_hp / 2 ? t->max_hp / 2 : 1) : it->arg;
        if(amt > room) amt = room;
        t->hp = (uint16_t)(t->hp + amt);
        ev(b, EV_HEAL, (uint8_t)sd, idx, item, amt, t->hp);
        if(it->kind == IK_RESTORE && t->status) cure(b, sd, t, idx, item, 0);
    } else if(it->kind == IK_CURE)
        cure(b, sd, t, idx, item, it->arg); // (bc_has_status was checked by bc_choose; arg 0 = Cure-All, any status)
#if BF_ITEMS
    else if(it->kind == IK_BATTLE) { // contract 73: Attack Boost .. Accuracy Boost (arg 0 .. 4: +1 stage), Mist Veil (5: Mist), Crit Focus (6: Steady Gaze) on the creature in play: the move effects, run from a made-up move
        const MoveDef mv = {0, 0, 0, 0, it->arg < 5 ? EF_STAT_SELF : it->arg == 5 ? EF_MIST : EF_FOCUS, it->arg, 1, 0, 0, 0};
        apply_effect(b, sd, sd ^ 1, &mv, false, 0xFE);
    }
#endif
    else
        return false;
    return true;
}

#include "battle_ai.c" // contract 82: the trainer AI (item / switch turn, move-choice modifiers 2 and 3); the Flute

// The player's item for this turn. Returns true when the battle ends (the creature was caught).
static bool use_item(Battle* b) {
    const BcAction a = b->act[0];
    const ItemDef* it = &b->data->items[a.arg];
    Party* pp = b->party[0];
    if(item_on(b, 0, a.arg, a.arg2)) return false;
#if BF_FLUTE
    if(a.arg == IT_FLAUTA) {
        flute(b);
        return false;
    }
#endif
    Mon* e = act_mon(b, 1);
    ev3(b, EV_THROW, 0, a.arg);
    // both draws are always taken (the second only matters when the first passed): the RNG stream does not depend on the roll
    uint8_t n = (uint8_t)rndr(b, 0, bc_ball_range(it->arg)), m = rnd8(b), sh = 0;
    bool caught = bc_catch(it->arg, b->reserve ? b->sf_rate : bc_species(b->data, e->species)->catch_rate, e->max_hp, e->hp, e->status, n, m, &sh);
    ev3(b, EV_SHAKE, 0, sh);
    if(!caught) {
        ev3(b, EV_BROKE, 0, sh);
        return false;
    }
    uint8_t slot = 0xFF;
    if(pp->count < BC_MAX_PARTY) { // party full: slot 0xFF, the battle scene hands the creature to the pending slot (battle_stash_caught, contract 23)
        slot = pp->count++;
        pp->mon[slot] = *e;
    }
    ev3(b, EV_CAUGHT, 0, slot);
    if(BF_COIN_HIT) money_ev(b, b->pay); // the coins of Pilfer are picked up when the creature is caught too
    b->result = BR_CAUGHT;
    b->phase = BP_OVER;
    ev3(b, EV_END, 0, BR_CAUGHT);
    return true;
}

// Battle style "Shift" (contract 75): the player swaps his creature for free between a trainer's creature falling and the next one coming, no turn passes.
bool bc_shift(Battle* b, uint8_t idx) {
    const Party* p = b->party[0];
    if(b->phase != BP_CHOOSE || b->n_ev >= BC_MAX_EVENTS || idx >= p->count || !p->mon[idx].hp || idx == b->side[0].active) return false;
    do_sendout(b, 0, idx);
    return true;
}

int bc_step(Battle* b) {
    b->n_ev = 0;
    if(b->phase == BP_OVER) return 0;
    if(b->phase == BP_SWITCH) {
        if(b->act[0].kind != ACT_SWITCH) return 0;
        do_sendout(b, 0, b->act[0].arg);
        b->act[0].kind = ACT_NONE;
        b->phase = BP_CHOOSE;
        return b->n_ev;
    }
    if(b->act[0].kind == ACT_NONE) return 0;
    if(b->act[0].kind == ACT_RUN && b->is_trainer) { // cannot flee: the turn is not used
        ev3(b, EV_RUN_FAIL, 0, 1);
        b->act[0].kind = ACT_NONE;
        return b->n_ev;
    }
    b->turn++;
    b->side[0].hurt = b->side[1].hurt = 0; // (Counter looks at this turn's damage only)
    enemy_choose(b);
    b->side[0].acted = b->side[1].acted = 0;
    bool player_moves = b->act[0].kind == ACT_MOVE;

    bool ran = false;
    if(b->act[0].kind == ACT_RUN) {
        ran = try_run(b);
    } else if(b->act[0].kind == ACT_SWITCH) {
        do_sendout(b, 0, b->act[0].arg);
    } else if(b->act[0].kind == ACT_ITEM) {
        if(use_item(b)) { // caught
            b->act[0].kind = ACT_NONE;
            return b->n_ev;
        }
    } else if(b->act[0].kind >= ACT_ROCK) { // Reserva Salvaje (contract 43): a rock makes it angry (catch rate x2, at most 255), bait makes it eat (rate / 2, at least 1), for 1..5 turns
        const bool rock = b->act[0].kind == ACT_ROCK;
        ev3(b, EV_RESERVE, 0, rock ? SF_ROCK : SF_BAIT);
        const unsigned r = rock ? b->sf_rate * 2u : b->sf_rate / 2u;
        b->sf_rate = r > 255 ? 255 : r ? (uint8_t)r : 1;
        b->sf_mood = (int8_t)(rndr(b, 1, 5) * (rock ? 1 : -1));
    }
    if(b->reserve && !ran) { // the creature never attacks: after the action it may run away, a random byte under 2 x its base Speed (at most 255), x2 while angry, / 4 while eating
        unsigned t = bc_species(b->data, act_mon(b, 1)->species)->base[STI_SPD + 1] * 2u;
        if(t > 255) t = 255;
        if(b->sf_mood) {
            const bool angry = b->sf_mood > 0;
            ev3(b, EV_RESERVE, 0, angry ? SF_ANGRY : SF_EATING);
            t = angry ? t << 1 : t >> 2;
            if(t > 255) t = 255;
            b->sf_mood += angry ? -1 : 1;
        }
        ran = rnd8(b) < t;
        if(ran) ev3(b, EV_RESERVE, 0, SF_FLED);
        else {
            b->act[0].kind = ACT_NONE;
            return b->n_ev;
        }
    }
    if(ran) {
        b->result = BR_RAN;
        b->phase = BP_OVER;
        ev3(b, EV_END, 0, BR_RAN);
        b->act[0].kind = ACT_NONE;
        return b->n_ev;
    }

    int order[2], n = 0;
    if(player_moves) {
        int p0 = move_priority(b, 0), p1 = move_priority(b, 1);
        int first = 0;
        if(p0 != p1) {
            first = p0 > p1 ? 0 : 1;
        } else {
            uint16_t s0 = bc_eff_stat(act_mon(b, 0), &b->side[0], STI_SPD);
            uint16_t s1 = bc_eff_stat(act_mon(b, 1), &b->side[1], STI_SPD);
            if(s0 != s1) first = s0 > s1 ? 0 : 1;
            else first = rnd8(b) < 128 ? 0 : 1;
        }
        order[n++] = first;
        order[n++] = first ^ 1;
    } else {
        order[n++] = 1;
    }
    for(int i = 0; i < n; i++) {
        int s = order[i];
        if(!act_mon(b, s)->hp || b->side[s].acted) continue; // (a creature sent out in this turn (the foe's replacement) has used its turn: contract 75)
#if BF_AI_ITEMS || BF_AI_SWITCH
        if(s && ai_turn(b)) b->side[1].acted = 1; // contract 82: the trainer's item or switch takes the creature's turn
        else
#endif
            exec_move(b, s, b->act[s].arg);
        // timing: poison / burn damage right after the afflicted creature's own action
        if(act_mon(b, s ^ 1)->hp) residual(b, s);
        if(resolve_faints(b)) {
            b->act[0].kind = ACT_NONE;
            return b->n_ev;
        }
    }
    b->side[0].vol &= (uint8_t)~VF_FLINCH;
    b->side[1].vol &= (uint8_t)~VF_FLINCH;
    resolve_faints(b);
    b->act[0].kind = ACT_NONE;
    return b->n_ev;
}
