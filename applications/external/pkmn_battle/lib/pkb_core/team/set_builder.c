/*
 * Random-team movesets and held items: a heuristic in the spirit of
 * random-battle sets. Every move gets a value for the set built so far:
 * attacks by effective power, accuracy, STAB, the matching base attack stat
 * and the type coverage they add; status, boosting, recovery and utility
 * moves by fixed weights, with pair rules (Rest + Sleep Talk, Dream Eater
 * only with a sleep move, Baton Pass only with a boost...). Each slot is a
 * weighted pick among the best few, so sets vary between draws.
 */
#include <string.h>
#include <pkb/team.h>
#include <pkb/moveinfo.h>
#include <pkb/section.h>
#include "pb_tables.h"

#define NUM_DEF_TYPES 17 /* the real types, as single-type defenders */

static const uint8_t def_types[NUM_DEF_TYPES] = {
    NORMAL, FIGHTING, FLYING, POISON, GROUND, ROCK, BUG, GHOST, STEEL,
    FIRE, WATER, GRASS, ELECTRIC, PSYCHIC_TYPE, ICE, DRAGON, DARK};

typedef enum {
    RoleJunk,
    RoleAttack,
    RoleSleep,
    RoleStatus, /* paralysis, poison: one status move is enough */
    RoleConfuse,
    RoleBoost,
    RoleRecover,
    RoleRest,
    RoleSleepTalk,
    RoleUtility,
} Role;

typedef struct {
    const PbBaseStats* base;
    uint8_t level;
    uint8_t dvs[2];
    uint8_t moves[PKB_NUM_MOVES];
    uint8_t count;
    uint8_t attacks, physical, special; /* special/physical: by type, not fixed damage */
    bool sleep, status, boost, rest, sleep_talk, sub, trap, boom, screen;
    uint16_t best_mult[NUM_DEF_TYPES]; /* best attack multiplier per defender type */
} SetState;

static bool has_move(const SetState* s, uint8_t move) {
    for(int i = 0; i < s->count; i++)
        if(s->moves[i] == move) return true;
    return false;
}

static bool has_effect(const SetState* s, uint8_t effect) {
    for(int i = 0; i < s->count; i++)
        if(pb_moves[s->moves[i] - 1].effect == effect) return true;
    return false;
}

static uint8_t move_type(const SetState* s, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    if(m->effect == EFFECT_HIDDEN_POWER) {
        uint8_t type, power;
        pkb_hidden_power(s->dvs, &type, &power);
        return type;
    }
    return m->type;
}

/* Effective power of an attack for ranking (0 = not an attack). Fixed-damage
 * moves get a typical equivalent; they ignore STAB and stats (*fixed). */
static uint16_t attack_power(const SetState* s, uint8_t move, bool* fixed) {
    const PbMove* m = &pb_moves[move - 1];
    uint16_t p = m->power;
    *fixed = false;
    switch(m->effect) {
    case EFFECT_LEVEL_DAMAGE:
        *fixed = true;
        return s->level > 30 ? 75 : 45;
    case EFFECT_STATIC_DAMAGE:
        *fixed = true;
        return 20;
    case EFFECT_SUPER_FANG:
        *fixed = true;
        return 55;
    case EFFECT_PSYWAVE:
        *fixed = true;
        return 50;
    case EFFECT_OHKO:
        *fixed = true;
        return 50;
    case EFFECT_MULTI_HIT:
        return (uint16_t)(p * 3);
    case EFFECT_DOUBLE_HIT:
    case EFFECT_POISON_MULTI_HIT:
        return (uint16_t)(p * 2);
    case EFFECT_TRIPLE_KICK:
        return 45;
    case EFFECT_HYPER_BEAM:
        return (uint16_t)(p * 6 / 10);
    case EFFECT_RAZOR_WIND:
    case EFFECT_SKY_ATTACK:
    case EFFECT_SKULL_BASH:
        return p / 2;
    case EFFECT_SOLARBEAM:
        return has_move(s, SUNNY_DAY) ? p : p / 2;
    case EFFECT_FLY:
        return (uint16_t)(p * 3 / 4);
    case EFFECT_RECOIL_HIT:
    case EFFECT_JUMP_KICK:
    case EFFECT_RAMPAGE:
        return (uint16_t)(p * 85 / 100);
    case EFFECT_ROLLOUT:
    case EFFECT_FURY_CUTTER:
    case EFFECT_RAGE:
        return p / 2;
    case EFFECT_FALSE_SWIPE:
        return (uint16_t)(p * 6 / 10);
    case EFFECT_FUTURE_SIGHT:
        return (uint16_t)(p * 7 / 10);
    case EFFECT_SELFDESTRUCT:
        return s->boom ? 0 : (uint16_t)(p * 35 / 100);
    case EFFECT_RETURN:
        return PKB_TEAM_HAPPINESS * 10 / 25;
    case EFFECT_FRUSTRATION:
        return 0; /* happiness 255 */
    case EFFECT_MAGNITUDE:
        return 70;
    case EFFECT_PRESENT:
        return 40;
    case EFFECT_REVERSAL:
        return has_effect(s, EFFECT_ENDURE) ? 120 : 40;
    case EFFECT_HIDDEN_POWER: {
        uint8_t type, power;
        pkb_hidden_power(s->dvs, &type, &power);
        return power;
    }
    case EFFECT_DREAM_EATER:
        return s->sleep ? p : 0;
    case EFFECT_SNORE:
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
    case EFFECT_BIDE:
        return 0; /* handled as utility */
    }
    return p > 1 ? p : 0;
}

/* The attack's value against a neutral target. */
static uint32_t attack_value(const SetState* s, uint8_t move) {
    bool fixed;
    uint32_t p = attack_power(s, move, &fixed);
    if(!p) return 0;
    uint32_t acc = pb_moves[move - 1].accuracy;
    uint32_t v = p * (acc + 1) / 256;
    if(fixed) return v;
    uint8_t type = move_type(s, move);
    if(type == s->base->types[0] || type == s->base->types[1]) v = v * 3 / 2;
    uint32_t stat = pkb_type_special(type) ? s->base->stats[4] : s->base->stats[1];
    return v * (stat + 50) / 150;
}

/* How much an attack of `type` widens the set's coverage (x100). */
static uint32_t coverage_factor(const SetState* s, uint8_t move, bool fixed) {
    if(fixed) return s->attacks ? 50 : 100;
    uint8_t type = move_type(s, move);
    if(!s->attacks) return 100;
    for(int i = 0; i < s->count; i++) {
        bool f;
        if(attack_power(s, s->moves[i], &f) && !f && move_type(s, s->moves[i]) == type) return 10;
    }
    uint32_t gain = 0;
    for(int t = 0; t < NUM_DEF_TYPES; t++) {
        uint16_t m = pkb_type_mult(type, def_types[t], def_types[t], false);
        uint16_t b = s->best_mult[t];
        if(m > PKB_MULT_NEUTRAL && b <= PKB_MULT_NEUTRAL) gain += 3;
        else if(m >= PKB_MULT_NEUTRAL && b < PKB_MULT_NEUTRAL) gain += 2;
    }
    uint32_t f = 35 + gain * 7;
    if(s->attacks >= 2) f = f * 3 / 4;
    return f > 130 ? 130 : f;
}

static Role move_role(uint8_t move) {
    switch(pb_moves[move - 1].effect) {
    case EFFECT_SLEEP:
        return RoleSleep;
    case EFFECT_PARALYZE:
    case EFFECT_TOXIC:
    case EFFECT_POISON:
        return RoleStatus;
    case EFFECT_CONFUSE:
    case EFFECT_SWAGGER:
        return RoleConfuse;
    case EFFECT_ATTACK_UP_2:
    case EFFECT_BELLY_DRUM:
    case EFFECT_CURSE:
    case EFFECT_SP_ATK_UP:
    case EFFECT_SP_DEF_UP_2:
    case EFFECT_SPEED_UP_2:
    case EFFECT_DEFENSE_UP_2:
        return RoleBoost;
    case EFFECT_HEAL:
        return move == REST ? RoleRest : RoleRecover;
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
        return RoleRecover;
    case EFFECT_SLEEP_TALK:
    case EFFECT_SNORE:
        return RoleSleepTalk;
    }
    return RoleUtility;
}


/* Value of a non-attacking move for the set so far. */
static uint32_t utility_value(const SetState* s, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    uint32_t acc = m->accuracy + 1u;
    bool ghost = s->base->types[0] == GHOST || s->base->types[1] == GHOST;

    switch(move_role(move)) {
    case RoleSleep:
        return s->sleep ? 0 : 110 * acc / 256 * (s->status ? 4 : 10) / 10;
    case RoleStatus: {
        uint32_t v = m->effect == EFFECT_PARALYZE ? 85 : m->effect == EFFECT_TOXIC ? 75 : 30;
        return s->status ? 0 : v * acc / 256 * (s->sleep ? 4 : 10) / 10;
    }
    case RoleConfuse:
        return s->status || s->sleep ? 15 : 35 * acc / 256;
    case RoleBoost: {
        if(s->boost) return 0;
        uint32_t v;
        switch(m->effect) {
        case EFFECT_ATTACK_UP_2:
        case EFFECT_BELLY_DRUM:
            v = s->physical ? 95 : 0;
            break;
        case EFFECT_CURSE:
            v = ghost ? 30 : s->physical ? (s->base->stats[3] < 70 ? 105 : 85) : 0;
            break;
        case EFFECT_SP_ATK_UP:
            v = s->special ? 45 : 0;
            break;
        case EFFECT_SP_DEF_UP_2:
            v = s->special ? 55 : 25;
            break;
        case EFFECT_SPEED_UP_2:
            v = 40;
            break;
        default:
            v = 35;
        }
        return s->attacks ? v : v / 3;
    }
    case RoleRecover:
        return has_effect(s, EFFECT_HEAL) || has_effect(s, EFFECT_MORNING_SUN) ||
                       has_effect(s, EFFECT_SYNTHESIS) || has_effect(s, EFFECT_MOONLIGHT) ?
                   0 :
                   90;
    case RoleRest:
        return has_effect(s, EFFECT_HEAL) ? 0 : 50;
    case RoleSleepTalk:
        if(!s->rest || s->sleep_talk) return 0;
        return m->effect == EFFECT_SLEEP_TALK ? 1000 : 60;
    default:
        break;
    }

    switch(m->effect) {
    case EFFECT_SPIKES:
        return 70;
    case EFFECT_LEECH_SEED:
        return 65;
    case EFFECT_SUBSTITUTE:
        return 55;
    case EFFECT_ENCORE:
        return 50;
    case EFFECT_PAIN_SPLIT:
        return 50;
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
        return has_effect(s, EFFECT_COUNTER) || has_effect(s, EFFECT_MIRROR_COAT) ? 20 : 45;
    case EFFECT_REFLECT:
    case EFFECT_LIGHT_SCREEN:
        return s->screen ? 20 : 45;
    case EFFECT_FORCE_SWITCH:
        return 40;
    case EFFECT_DESTINY_BOND:
        return 40;
    case EFFECT_RESET_STATS:
        return 35;
    case EFFECT_HEAL_BELL:
        return 35;
    case EFFECT_PROTECT:
        return 25;
    case EFFECT_SAFEGUARD:
        return 25;
    case EFFECT_RAPID_SPIN:
        return 20;
    case EFFECT_ATTRACT:
        return 15;
    case EFFECT_BATON_PASS:
        return s->boost || s->sub ? 75 : 0;
    case EFFECT_MEAN_LOOK:
        return has_effect(s, EFFECT_PERISH_SONG) ? 70 : 20;
    case EFFECT_PERISH_SONG:
        return s->trap ? 80 : 40;
    case EFFECT_LOCK_ON:
        return has_effect(s, EFFECT_OHKO) || has_move(s, ZAP_CANNON) || has_move(s, DYNAMICPUNCH) ? 60 : 0;
    case EFFECT_ENDURE:
        return has_effect(s, EFFECT_REVERSAL) ? 60 : 5;
    case EFFECT_DEFENSE_CURL:
        return has_move(s, ROLLOUT) ? 40 : 5;
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY: {
        uint8_t boosted = m->effect == EFFECT_RAIN_DANCE ? WATER : FIRE;
        uint32_t n = 0;
        for(int i = 0; i < s->count; i++) {
            uint8_t mv = s->moves[i];
            n += pb_moves[mv - 1].power > 1 && move_type(s, mv) == boosted;
            n += m->effect == EFFECT_RAIN_DANCE ? mv == THUNDER : mv == SOLARBEAM;
        }
        return n ? 20 + 30 * n : 0;
    }
    }
    return 3; /* junk: only used to fill a short pool */
}

static uint32_t move_value(const SetState* s, uint8_t move) {
    bool fixed;
    if(attack_power(s, move, &fixed)) {
        uint32_t v = attack_value(s, move) * coverage_factor(s, move, fixed) / 100;
        if(s->count >= 2 && s->attacks < 2) v = v * 13 / 10;
        return v ? v : 1;
    }
    /* Struggle-proof: the first slot is always an attack if there is one */
    return s->count ? utility_value(s, move) : 0;
}

static void add_move(SetState* s, uint8_t move) {
    bool fixed;
    const PbMove* m = &pb_moves[move - 1];
    s->moves[s->count++] = move;
    if(attack_power(s, move, &fixed) || m->effect == EFFECT_SELFDESTRUCT) {
        s->attacks++;
        if(m->effect == EFFECT_SELFDESTRUCT) s->boom = true;
        if(!fixed) {
            uint8_t type = move_type(s, move);
            if(pkb_type_special(type)) s->special++;
            else s->physical++;
            for(int t = 0; t < NUM_DEF_TYPES; t++) {
                uint16_t mult = pkb_type_mult(type, def_types[t], def_types[t], false);
                if(mult > s->best_mult[t]) s->best_mult[t] = mult;
            }
        }
    }
    switch(move_role(move)) {
    case RoleSleep:
        s->sleep = true;
        break;
    case RoleStatus:
        s->status = true;
        break;
    case RoleBoost:
        s->boost = true;
        break;
    case RoleRest:
        s->rest = true;
        break;
    case RoleSleepTalk:
        s->sleep_talk = s->sleep_talk || m->effect == EFFECT_SLEEP_TALK;
        break;
    default:
        break;
    }
    s->sub |= m->effect == EFFECT_SUBSTITUTE;
    s->trap |= m->effect == EFFECT_MEAN_LOOK;
    s->screen |= m->effect == EFFECT_REFLECT || m->effect == EFFECT_LIGHT_SCREEN;
}

#define PICK_TOP 3

PKB_TEXT2 void pkb_set_build(PkbTeamMon* mon, const uint8_t* pool, int n, PkbPolicyRng* rng) {
    SetState s;
    memset(&s, 0, sizeof(s));
    s.base = &pb_base_stats[mon->species - 1];
    s.level = mon->level;
    memcpy(s.dvs, mon->dvs, sizeof(s.dvs));

    while(s.count < PKB_NUM_MOVES && s.count < n) {
        uint8_t top[PICK_TOP] = {0};
        uint32_t top_v[PICK_TOP] = {0};
        for(int i = 0; i < n; i++) {
            if(has_move(&s, pool[i])) continue;
            uint32_t v = move_value(&s, pool[i]);
            for(int k = 0; k < PICK_TOP; k++) {
                if(v > top_v[k] || !top[k]) {
                    memmove(&top[k + 1], &top[k], (size_t)(PICK_TOP - 1 - k));
                    memmove(&top_v[k + 1], &top_v[k], sizeof(top_v[0]) * (PICK_TOP - 1 - k));
                    top[k] = pool[i];
                    top_v[k] = v;
                    break;
                }
            }
        }
        /* weighted pick among the best few: weight v^2, ignoring options
         * worth less than half the best */
        uint32_t w[PICK_TOP], total = 0;
        for(int k = 0; k < PICK_TOP; k++) {
            w[k] = top[k] && top_v[k] * 2 >= top_v[0] ? (top_v[k] / 4 + 1) * (top_v[k] / 4 + 1) : 0;
            total += w[k];
        }
        uint32_t r = pkb_policy_rng_below(rng, total);
        int k = 0;
        while(r >= w[k]) r -= w[k++];
        add_move(&s, top[k]);
    }
    memcpy(mon->moves, s.moves, sizeof(mon->moves));
}

static bool set_has_effect(const PkbTeamMon* mon, uint8_t effect) {
    for(int i = 0; i < PKB_NUM_MOVES; i++)
        if(mon->moves[i] && pb_moves[mon->moves[i] - 1].effect == effect) return true;
    return false;
}

uint8_t pkb_set_item(const PkbTeamMon* mon, uint8_t* leftovers_left, PkbPolicyRng* rng) {
    if(mon->species == PIKACHU) return LIGHT_BALL;
    if(mon->species == CUBONE || mon->species == MAROWAK) return THICK_CLUB;
    bool rest = false, sleep_talk = false, multi = false, crit = false;
    for(int i = 0; i < PKB_NUM_MOVES; i++) rest |= mon->moves[i] == REST;
    sleep_talk = set_has_effect(mon, EFFECT_SLEEP_TALK);
    if(rest && !sleep_talk) return MINT_BERRY;

    /* the strongest attack's type, for a booster */
    SetState s;
    memset(&s, 0, sizeof(s));
    s.base = &pb_base_stats[mon->species - 1];
    s.level = mon->level;
    memcpy(s.dvs, mon->dvs, sizeof(s.dvs));
    uint32_t best = 0;
    uint8_t best_type = 0;
    for(int i = 0; i < PKB_NUM_MOVES; i++) {
        uint8_t mv = mon->moves[i];
        bool fixed;
        if(!mv) continue;
        uint8_t e = pb_moves[mv - 1].effect;
        multi |= e == EFFECT_MULTI_HIT || e == EFFECT_DOUBLE_HIT || e == EFFECT_TRIPLE_KICK ||
                 e == EFFECT_POISON_MULTI_HIT;
        crit |= pkb_move_high_crit(mv);
        uint32_t v = attack_power(&s, mv, &fixed) && !fixed ? attack_value(&s, mv) : 0;
        if(v > best) {
            best = v;
            best_type = move_type(&s, mv);
        }
    }
    uint8_t stat = pkb_type_special(best_type) ? s.base->stats[4] : s.base->stats[1];
    uint32_t r = pkb_policy_rng_below(rng, 100);
    if(best && stat >= 85 && r < 45) {
        uint8_t booster = pkb_type_boost_item(best_type);
        if(booster) return booster;
    }
    if(*leftovers_left && r < 80) {
        (*leftovers_left)--;
        return LEFTOVERS;
    }
    uint8_t extras[7];
    int n = 0;
    extras[n++] = QUICK_CLAW;
    extras[n++] = FOCUS_BAND;
    extras[n++] = BRIGHTPOWDER;
    extras[n++] = MIRACLEBERRY;
    extras[n++] = GOLD_BERRY;
    if(multi) extras[n++] = KINGS_ROCK;
    if(crit) extras[n++] = SCOPE_LENS;
    return extras[pkb_policy_rng_below(rng, (uint32_t)n)];
}
