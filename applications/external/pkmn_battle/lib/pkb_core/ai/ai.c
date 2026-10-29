/* The Flipper's battle AI (see include/pkb/ai.h). Read-only on the engine. */
#include <string.h>
#include <pkb/ai.h>
#include <pkb/moveinfo.h>
#include <pkb/section.h>

#include "pb_const.h"
#include "pb_wram.h"
#include "pb_tables.h"
#include "pb_battle.h"

#define P(a) pkb_engine_peek(E, (uint16_t)(a))
#define P16(a) pkb_engine_peek16(E, (uint16_t)(a))
#define BMON(field) ((field) - wBattleMon) /* offset in a battle mon struct */

/* A mon as the AI sees it: an active battle mon (stats with stat stages,
 * types as changed in battle) or a benched party mon (plain stats). */
typedef struct {
    uint8_t species; /* for Light Ball / Thick Club / Metal Powder */
    uint8_t level, item, status, happiness;
    uint8_t types[2];
    uint8_t dvs[2];
    uint8_t moves[NUM_MOVES], pp[NUM_MOVES];
    uint16_t hp, max_hp;
    uint16_t stat[5]; /* atk def spd sat sdf */
    uint8_t stage[7]; /* atk def spd sat sdf acc eva, 7 = neutral */
    uint8_t screens; /* its side's screens */
    bool sub, identified, invulnerable, flying, lock_on, active;
} AiMon;

static void ai_active(const PkbEngine* E, bool player, AiMon* m) {
    uint16_t base = player ? wBattleMon : wEnemyMon;
    memset(m, 0, sizeof(*m));
    m->active = true;
    m->species = player ? P(wPartyMon1Species + PARTYMON_STRUCT_LENGTH * P(wCurBattleMon)) :
                          P(wTempEnemyMonSpecies);
    m->level = P(base + BMON(wBattleMonLevel));
    m->item = P(base + BMON(wBattleMonItem));
    m->status = P(base + BMON(wBattleMonStatus));
    m->happiness = P(base + BMON(wBattleMonHappiness));
    m->types[0] = P(base + BMON(wBattleMonType1));
    m->types[1] = P(base + BMON(wBattleMonType1) + 1);
    m->dvs[0] = P(base + BMON(wBattleMonDVs));
    m->dvs[1] = P(base + BMON(wBattleMonDVs) + 1);
    for(int i = 0; i < NUM_MOVES; i++) {
        m->moves[i] = P(base + BMON(wBattleMonMoves) + i);
        m->pp[i] = P(base + BMON(wBattleMonPP) + i) & PP_MASK;
    }
    m->hp = P16(base + BMON(wBattleMonHP));
    m->max_hp = P16(base + BMON(wBattleMonMaxHP));
    for(int s = 0; s < 5; s++) m->stat[s] = P16(base + BMON(wBattleMonAttack) + 2 * s);
    for(int s = 0; s < 7; s++) m->stage[s] = P((player ? wPlayerStatLevels : wEnemyStatLevels) + s);
    m->screens = P(player ? wPlayerScreens : wEnemyScreens);
    uint8_t ss1 = P(player ? wPlayerSubStatus1 : wEnemySubStatus1);
    uint8_t ss3 = P(player ? wPlayerSubStatus3 : wEnemySubStatus3);
    uint8_t ss4 = P(player ? wPlayerSubStatus4 : wEnemySubStatus4);
    uint8_t ss5 = P(player ? wPlayerSubStatus5 : wEnemySubStatus5);
    m->sub = ss4 & (1 << SUBSTATUS_SUBSTITUTE);
    m->identified = ss1 & (1 << SUBSTATUS_IDENTIFIED);
    m->flying = ss3 & (1 << SUBSTATUS_FLYING);
    m->invulnerable = ss3 & (1 << SUBSTATUS_FLYING | 1 << SUBSTATUS_UNDERGROUND);
    m->lock_on = ss5 & (1 << SUBSTATUS_LOCK_ON);
}

static void ai_party(const PkbEngine* E, bool player, uint8_t i, AiMon* m) {
    uint16_t p = (uint16_t)((player ? wPartyMon1 : wOTPartyMon1) + PARTYMON_STRUCT_LENGTH * i);
    memset(m, 0, sizeof(*m));
    m->species = P(p + MON_SPECIES);
    m->level = P(p + MON_LEVEL);
    m->item = P(p + MON_ITEM);
    m->status = P(p + MON_STATUS);
    m->happiness = P(p + MON_HAPPINESS);
    if(m->species >= 1 && m->species <= NUM_POKEMON) {
        memcpy(m->types, pb_base_stats[m->species - 1].types, 2);
    }
    m->dvs[0] = P(p + MON_DVS);
    m->dvs[1] = P(p + MON_DVS + 1);
    for(int k = 0; k < NUM_MOVES; k++) {
        m->moves[k] = P(p + MON_MOVES + k);
        m->pp[k] = P(p + MON_PP + k) & PP_MASK;
    }
    m->hp = P16(p + MON_HP);
    m->max_hp = P16(p + MON_MAXHP);
    for(int s = 0; s < 5; s++) m->stat[s] = P16(p + MON_STATS + 2 * s);
    memset(m->stage, BASE_STAT_LEVEL, sizeof(m->stage));
    m->screens = P(player ? wPlayerScreens : wEnemyScreens);
}

/* ---- damage (effect_commands.asm: DamageStats, DamageCalc, Stab,
 * DamageVariation; Colosseum rules) ---- */

static uint8_t move_type(const AiMon* a, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    if(m->effect == EFFECT_HIDDEN_POWER) {
        uint8_t type, power;
        pkb_hidden_power(a->dvs, &type, &power);
        return type;
    }
    return m->type;
}

static uint8_t move_priority(uint8_t move) {
    uint8_t effect = pb_moves[move - 1].effect;
    for(const uint8_t* p = pb_move_effect_priorities; *p != 0xFF; p += 2)
        if(p[0] == effect) return p[1];
    return 1;
}

static uint16_t weather_mod(const PkbEngine* E, uint8_t type, uint8_t effect, uint16_t v) {
    uint8_t w = P(wBattleWeather);
    const uint8_t* hit = NULL;
    for(const uint8_t* p = pb_weather_type_modifiers; *p != 0xFF && !hit; p += 3)
        if(p[0] == w && p[1] == type) hit = p;
    for(const uint8_t* p = pb_weather_move_modifiers; *p != 0xFF && !hit; p += 3)
        if(p[0] == w && p[1] == effect) hit = p;
    if(!hit) return v;
    uint32_t q = (uint32_t)v * hit[2] / 10;
    if(q > 0xFFFF) return 0xFFFF;
    return q ? (uint16_t)q : 1;
}

static uint16_t hit_chance(const PkbEngine* E, const AiMon* a, const AiMon* d, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    if(d->invulnerable) {
        bool reaches = d->flying ? (m->effect == EFFECT_GUST || m->effect == EFFECT_TWISTER || move == THUNDER) :
                                   (m->effect == EFFECT_EARTHQUAKE || m->effect == EFFECT_MAGNITUDE);
        if(!reaches) return 0;
    }
    if(m->effect == EFFECT_ALWAYS_HIT || a->lock_on) return 256;
    if(move == THUNDER && P(wBattleWeather) == WEATHER_RAIN) return 256;
    uint32_t acc = m->accuracy;
    const uint8_t* am = pb_accuracy_level_multipliers[a->stage[5] - 1];
    const uint8_t* em = pb_accuracy_level_multipliers[14 - d->stage[6] - 1];
    acc = acc * am[0] / am[1];
    acc = acc * em[0] / em[1];
    if(d->item && pb_item_held_effect[d->item - 1][0] == HELD_BRIGHTPOWDER) {
        uint8_t param = pb_item_held_effect[d->item - 1][1];
        acc = acc > param ? acc - param : 0;
    }
    if(acc > 255) acc = 255;
    return (uint16_t)acc;
}

PKB_TEXT2 static void ai_damage(const PkbEngine* E, const AiMon* a, const AiMon* d, uint8_t move, PkbAiDamage* out) {
    const PbMove* m = &pb_moves[move - 1];
    uint8_t type = move_type(a, move);
    uint16_t power = m->power, fixed = 0;
    bool is_fixed = true;

    memset(out, 0, sizeof(*out));
    out->hits = 1;
    out->hit = hit_chance(E, a, d, move);
    /* Stab (weather, STAB, type matchups) skips Struggle entirely */
    bool immune = move != STRUGGLE && pkb_type_mult(type, d->types[0], d->types[1], d->identified) == 0;

    switch(m->effect) {
    case EFFECT_LEVEL_DAMAGE:
        fixed = a->level;
        break;
    case EFFECT_STATIC_DAMAGE:
        fixed = m->power;
        break;
    case EFFECT_SUPER_FANG:
        fixed = d->hp / 2 ? d->hp / 2 : 1;
        break;
    case EFFECT_PSYWAVE:
        out->min = 1;
        out->max = (uint16_t)(a->level * 3 / 2 - 1);
        if(immune) out->max = out->min = 0;
        return;
    case EFFECT_OHKO:
        fixed = a->level >= d->level ? d->hp : 0;
        break;
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
    case EFFECT_BIDE:
        return;
    case EFFECT_DREAM_EATER:
        if(!(d->status & SLP_MASK) || d->sub) return;
        is_fixed = false;
        break;
    case EFFECT_SNORE:
        if(!(a->status & SLP_MASK)) return;
        is_fixed = false;
        break;
    case EFFECT_RETURN:
        power = (uint16_t)(a->happiness * 10 / 25);
        is_fixed = false;
        break;
    case EFFECT_FRUSTRATION:
        power = (uint16_t)((255 - a->happiness) * 10 / 25);
        is_fixed = false;
        break;
    case EFFECT_HIDDEN_POWER: {
        uint8_t t, p;
        pkb_hidden_power(a->dvs, &t, &p);
        power = p;
        is_fixed = false;
        break;
    }
    case EFFECT_MAGNITUDE:
        power = 71;
        is_fixed = false;
        break;
    case EFFECT_PRESENT:
        power = 40;
        is_fixed = false;
        break;
    case EFFECT_REVERSAL: {
        uint32_t ratio = a->max_hp ? (uint32_t)a->hp * 48 / a->max_hp : 48;
        power = 20;
        for(int i = 0; i < 12; i += 2) {
            if(ratio <= pb_flail_reversal_power[i]) {
                power = pb_flail_reversal_power[i + 1];
                break;
            }
        }
        is_fixed = false;
        break;
    }
    case EFFECT_MULTI_HIT:
        out->hits = 3;
        is_fixed = false;
        break;
    case EFFECT_DOUBLE_HIT:
    case EFFECT_POISON_MULTI_HIT:
        out->hits = 2;
        is_fixed = false;
        break;
    case EFFECT_TRIPLE_KICK:
        out->hits = 6; /* 10 + 20 + 30 power */
        is_fixed = false;
        break;
    default:
        is_fixed = false;
    }
    if(is_fixed) {
        out->min = out->max = immune ? 0 : fixed;
        return;
    }
    if(!power || immune) return;

    /* AttackDamage */
    bool special = pkb_type_special(type);
    uint16_t atk = a->stat[special ? 3 : 0], def = d->stat[special ? 4 : 1];
    if(d->screens & (1 << (special ? SCREENS_LIGHT_SCREEN : SCREENS_REFLECT))) def = (uint16_t)(def << 1);
    if(special ? a->species == PIKACHU && a->item == LIGHT_BALL :
                 (a->species == CUBONE || a->species == MAROWAK) && a->item == THICK_CLUB)
        atk = (uint16_t)(atk << 1);
    if((atk >> 8) | (def >> 8)) { /* TruncateHL_BC: one step in the Colosseum */
        def >>= 2;
        if(!def) def = 1;
        atk >>= 2;
        if(!atk) atk = 1;
    }
    uint8_t b = (uint8_t)atk, c = (uint8_t)def;
    if(d->species == DITTO && d->item == METAL_POWDER) {
        unsigned v = (unsigned)c + (c >> 1);
        c = (uint8_t)v;
        if(v >= 0x100) {
            b >>= 1;
            if(!b) b = 1;
            c = (uint8_t)(0x80 | (c >> 1));
        }
    }

    /* DamageCalc */
    if(m->effect == EFFECT_SELFDESTRUCT) {
        c >>= 1;
        if(!c) c = 1;
    }
    if(!c) c = 1;
    uint32_t v = (uint32_t)(a->level * 2) / 5 + 2;
    v = v * power * b / c / 50;
    if(a->item) {
        uint8_t ib = pb_item_held_effect[a->item - 1][0], ic = pb_item_held_effect[a->item - 1][1];
        for(const uint8_t* p = pb_type_boost_items; *p != 0xFF; p += 2) {
            if(p[0] != ib) continue;
            if(p[1] == type) v = v * (ic + 100u) / 100;
            break;
        }
    }
    if(v > 997) v = 997;
    v += 2;

    /* Stab: weather, STAB, then each type matchup in turn */
    uint16_t dmg = (uint16_t)v;
    if(move == STRUGGLE) {
        out->max = dmg;
        out->min = (uint16_t)((uint32_t)dmg * 217 / 255);
        return;
    }
    dmg = weather_mod(E, type, m->effect, dmg);
    if(type == a->types[0] || type == a->types[1]) dmg = (uint16_t)(dmg + (dmg >> 1));
    for(const uint8_t* p = pb_type_matchups; *p != 0xFF; p += 3) {
        if(*p == 0xFE) {
            if(d->identified) break;
            p -= 2;
            continue;
        }
        if(p[0] == type && (p[1] == d->types[0] || p[1] == d->types[1])) {
            uint32_t q = (uint32_t)dmg * p[2];
            if(q) {
                q /= 10;
                dmg = q ? (uint16_t)q : 1;
            } else {
                dmg = 0;
            }
        }
    }
    out->max = dmg;
    out->min = dmg >= 2 ? (uint16_t)((uint32_t)dmg * 217 / 255) : dmg;
}

void pkb_ai_active_damage(const PkbEngine* E, bool player, uint8_t move, PkbAiDamage* out) {
    AiMon a, d;
    ai_active(E, player, &a);
    ai_active(E, !player, &d);
    ai_damage(E, &a, &d, move, out);
}

/* expected damage of a move (both rolls, hits, accuracy), at most the
 * defender's HP */
static uint32_t expected(const PkbAiDamage* dm, uint16_t hp) {
    uint32_t v = ((uint32_t)dm->min + dm->max) / 2 * dm->hits * dm->hit / 256;
    return v > hp ? hp : v;
}

static bool usable(const AiMon* m, int slot, uint8_t disabled) {
    return m->moves[slot] && m->pp[slot] && disabled != slot + 1;
}

/* The best expected damage `a` does to `d` with its usable moves, as a
 * percentage of d's HP; *ko: a move that KOs with its lowest roll. */
static int best_pct(const PkbEngine* E, const AiMon* a, const AiMon* d, uint8_t disabled, bool* ko, bool* special) {
    int best = 0;
    *ko = false;
    if(special) *special = false;
    if(!d->hp) return 0;
    for(int i = 0; i < NUM_MOVES; i++) {
        if(!usable(a, i, disabled)) continue;
        PkbAiDamage dm;
        ai_damage(E, a, d, a->moves[i], &dm);
        int pct = (int)(expected(&dm, d->hp) * 100 / d->hp);
        if((uint32_t)dm.min * dm.hits >= d->hp && dm.hit >= 200) *ko = true;
        if(pct > best) {
            best = pct;
            if(special) *special = pkb_type_special(move_type(a, a->moves[i]));
        }
    }
    return best;
}

static bool faster(const AiMon* a, const AiMon* b) {
    return a->stat[2] > b->stat[2];
}

/* How well `mine` does against `foe` (higher is better); switch_in: mine
 * takes a free hit first. */
static int matchup(const PkbEngine* E, const AiMon* mine, const AiMon* foe, bool switch_in) {
    bool ko_them, ko_us;
    int off = best_pct(E, mine, foe, 0, &ko_them, NULL);
    AiMon m = *mine;
    int def = best_pct(E, foe, &m, 0, &ko_us, NULL);
    int v = off - def;
    if(switch_in) {
        if(ko_us) return -300;
        /* the hit taken while switching in */
        m.hp = (uint16_t)(m.hp - (uint32_t)m.hp * (def > 100 ? 100 : def) / 100);
        def = best_pct(E, foe, &m, 0, &ko_us, NULL);
        v = off - def / 2;
    }
    bool first = faster(mine, foe);
    if(ko_them && first) v += 100;
    if(ko_us && !first) v -= 100;
    else if(ko_us && !ko_them) v -= 50;
    return v;
}

/* ---- move choice ---- */

typedef struct {
    AiMon us, them;
    int threat; /* their best expected damage, % of our HP */
    bool they_ko; /* they can KO us this turn */
    bool their_special; /* their best attack is special */
    bool we_first;
    bool party_statused;
    uint8_t their_alive, our_alive; /* fit mons besides the active one */
    int boosts; /* sum of our stat stages above neutral */
    int their_boosts;
} Situation;

static int sum_boosts(const AiMon* m) {
    int s = 0;
    for(int i = 0; i < 7; i++) s += m->stage[i] - BASE_STAT_LEVEL;
    return s;
}

static bool can_status(const Situation* s) {
    return !s->them.status && !s->them.sub && !(s->them.screens & (1 << SCREENS_SAFEGUARD));
}

static bool has_type(const AiMon* m, uint8_t t) {
    return m->types[0] == t || m->types[1] == t;
}

static int score_move(const PkbEngine* E, const Situation* s, uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    const AiMon* us = &s->us;
    const AiMon* them = &s->them;
    int hp_pct = us->max_hp ? us->hp * 100 / us->max_hp : 0;
    PkbAiDamage dm;
    ai_damage(E, us, them, move, &dm);
    bool acts = s->we_first || move_priority(move) > 1 || !s->they_ko;
    uint16_t type_mult = pkb_type_mult(move_type(us, move), them->types[0], them->types[1], them->identified);

    if(dm.max) {
        int pct = them->hp ? (int)(expected(&dm, them->hp) * 100 / them->hp) : 0;
        bool ko = (uint32_t)dm.min * dm.hits >= them->hp && !them->sub && m->effect != EFFECT_FALSE_SWIPE;
        int score = pct;
        if(ko) score += 60 * dm.hit / 256;
        switch(m->effect) {
        case EFFECT_RAZOR_WIND:
        case EFFECT_SKY_ATTACK:
        case EFFECT_SKULL_BASH:
        case EFFECT_FLY:
            score /= 2;
            break;
        case EFFECT_SOLARBEAM:
            if(P(wBattleWeather) != WEATHER_SUN) score /= 2;
            break;
        case EFFECT_HYPER_BEAM:
            if(!ko) score = score * 6 / 10;
            break;
        case EFFECT_FUTURE_SIGHT:
            score = P(wPlayerFutureSightCount) ? 0 : score / 2;
            break;
        case EFFECT_SELFDESTRUCT:
            if(hp_pct > 35 && !s->they_ko) score = score * 3 / 10;
            if(!s->their_alive && !ko) score = 0;
            break;
        case EFFECT_RAMPAGE:
        case EFFECT_ROLLOUT:
            score = score * 85 / 100;
            break;
        case EFFECT_RECOIL_HIT:
            score = score * 9 / 10;
            break;
        case EFFECT_LEECH_HIT:
            if(hp_pct < 70) score += pct / 4;
            break;
        }
        if(!acts) score /= 4;
        return score;
    }

    bool safe = s->threat < 35 && !s->they_ko;
    switch(m->effect) {
    case EFFECT_SLEEP:
        return can_status(s) && type_mult && acts ? 85 * dm.hit / 256 : 0;
    case EFFECT_PARALYZE:
        if(!can_status(s) || !type_mult || has_type(them, ELECTRIC) || !acts) return 0;
        return (faster(them, us) ? 75 : 45) * dm.hit / 256;
    case EFFECT_TOXIC:
    case EFFECT_POISON:
        if(!can_status(s) || has_type(them, POISON) || has_type(them, STEEL) || !acts) return 0;
        return (m->effect == EFFECT_TOXIC ? 50 : 25) * dm.hit / 256;
    case EFFECT_CONFUSE:
        return !them->sub && !(P(wEnemySubStatus3) & (1 << SUBSTATUS_CONFUSED)) && acts ? 25 * dm.hit / 256 : 0;
    case EFFECT_ATTACK_UP_2:
    case EFFECT_BELLY_DRUM:
    case EFFECT_CURSE:
    case EFFECT_SP_ATK_UP:
    case EFFECT_SP_DEF_UP_2:
    case EFFECT_SPEED_UP_2:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_ATTACK_UP:
    case EFFECT_DEFENSE_UP: {
        if(m->effect == EFFECT_CURSE && has_type(us, GHOST)) {
            return them->sub || (P(wEnemySubStatus1) & (1 << SUBSTATUS_CURSE)) || hp_pct < 60 ? 0 : 30;
        }
        uint8_t stat = m->effect == EFFECT_CURSE || m->effect == EFFECT_ATTACK_UP_2 ||
                               m->effect == EFFECT_BELLY_DRUM || m->effect == EFFECT_ATTACK_UP ? 0 :
                       m->effect == EFFECT_SP_ATK_UP ? 3 :
                       m->effect == EFFECT_SP_DEF_UP_2 ? 4 :
                       m->effect == EFFECT_SPEED_UP_2 ? 2 : 1;
        if(us->stage[stat] >= MAX_STAT_LEVEL - 1 || !safe) return 0;
        if(m->effect == EFFECT_BELLY_DRUM) return hp_pct > 75 && us->stage[0] < 9 ? 70 : 0;
        int v = s->boosts >= 4 ? 10 : 45;
        return stat == 2 && faster(us, them) ? 5 : v;
    }
    /* Recover, Softboiled, Milk Drink share EFFECT_HEAL with Rest */
    case EFFECT_HEAL:
        if(move == REST) {
            if(us->status & SLP_MASK) return 0;
            return hp_pct < 40 && s->threat < 90 ? 60 : us->status && hp_pct < 70 ? 40 : 0;
        }
        /* fall through */
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
        if(hp_pct >= 60 || s->threat >= 45 + (100 - hp_pct) / 2) return 0;
        return 100 - hp_pct;
    case EFFECT_SLEEP_TALK:
        return us->status & SLP_MASK && (us->status & SLP_MASK) > 1 ? 70 : 0;
    case EFFECT_REFLECT:
        return !(us->screens & (1 << SCREENS_REFLECT)) && !s->their_special && s->threat > 20 && !s->they_ko ? 40 : 0;
    case EFFECT_LIGHT_SCREEN:
        return !(us->screens & (1 << SCREENS_LIGHT_SCREEN)) && s->their_special && s->threat > 20 && !s->they_ko ? 40 : 0;
    case EFFECT_SPIKES:
        return !(them->screens & (1 << SCREENS_SPIKES)) && s->their_alive >= 2 ? 45 : 0;
    case EFFECT_LEECH_SEED:
        return !has_type(them, GRASS) && !them->sub && !(P(wEnemySubStatus4) & (1 << SUBSTATUS_LEECH_SEED)) && acts ?
                   40 * dm.hit / 256 :
                   0;
    case EFFECT_SUBSTITUTE:
        return !us->sub && hp_pct > 50 && s->threat < 25 ? 35 : 0;
    case EFFECT_PROTECT:
        if(P(wPlayerProtectCount)) return 0;
        return them->invulnerable && !s->we_first ? 5 : them->invulnerable ? 80 : 0;
    case EFFECT_ENCORE: {
        uint8_t last = P(wLastEnemyMove);
        return last && pb_moves[last - 1].power <= 1 && !(P(wEnemySubStatus5) & (1 << SUBSTATUS_ENCORED)) ? 55 : 0;
    }
    case EFFECT_RESET_STATS:
        return s->their_boosts >= 2 ? 25 * s->their_boosts : 0;
    case EFFECT_FORCE_SWITCH:
        return s->their_boosts >= 2 && s->their_alive ? 20 * s->their_boosts : 0;
    case EFFECT_BATON_PASS:
        return s->boosts >= 2 && s->our_alive ? 45 : 0;
    case EFFECT_DESTINY_BOND:
        return s->they_ko && s->we_first ? 70 : 0;
    case EFFECT_PAIN_SPLIT: {
        int their_pct = them->max_hp ? them->hp * 100 / them->max_hp : 0;
        return them->sub ? 0 : (their_pct - hp_pct) / 2;
    }
    case EFFECT_HEAL_BELL:
        return s->party_statused || us->status ? 35 : 0;
    case EFFECT_SAFEGUARD:
        return !(us->screens & (1 << SCREENS_SAFEGUARD)) && safe ? 15 : 0;
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY: {
        uint8_t w = m->effect == EFFECT_RAIN_DANCE ? WEATHER_RAIN : WEATHER_SUN;
        uint8_t t = w == WEATHER_RAIN ? WATER : FIRE;
        int n = 0;
        for(int i = 0; i < NUM_MOVES; i++)
            if(us->moves[i] && pb_moves[us->moves[i] - 1].power > 1 && move_type(us, us->moves[i]) == t) n++;
        return P(wBattleWeather) != w && n && safe ? 30 : 0;
    }
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
        return s->they_ko || s->we_first || (m->effect == EFFECT_COUNTER) == s->their_special ? 0 : 45;
    case EFFECT_MEAN_LOOK:
        for(int i = 0; i < NUM_MOVES; i++)
            if(us->moves[i] == PERISH_SONG && !(P(wEnemySubStatus5) & (1 << SUBSTATUS_CANT_RUN))) return 50;
        return 0;
    case EFFECT_PERISH_SONG:
        return !(P(wEnemySubStatus1) & (1 << SUBSTATUS_PERISH)) && (s->their_boosts >= 2 || s->our_alive) ? 30 : 0;
    case EFFECT_LOCK_ON:
        if(us->lock_on) return 0;
        for(int i = 0; i < NUM_MOVES; i++) {
            uint8_t mv = us->moves[i];
            if(mv && (pb_moves[mv - 1].effect == EFFECT_OHKO || mv == ZAP_CANNON || mv == DYNAMICPUNCH)) return 40;
        }
        return 0;
    case EFFECT_SWAGGER: /* never: a Swagger game desync (docs/PORTING.md) */
        return 0;
    }
    return 1;
}

static uint8_t disabled_slot(const PkbEngine* E, bool player) {
    uint8_t d = P(player ? wPlayerDisableCount : wEnemyDisableCount);
    return d ? (uint8_t)(d >> 4) : 0;
}

static void situation(const PkbEngine* E, Situation* s) {
    memset(s, 0, sizeof(*s));
    ai_active(E, true, &s->us);
    ai_active(E, false, &s->them);
    s->threat = best_pct(E, &s->them, &s->us, disabled_slot(E, false), &s->they_ko, &s->their_special);
    s->we_first = faster(&s->us, &s->them);
    s->boosts = sum_boosts(&s->us);
    s->their_boosts = sum_boosts(&s->them);
    uint8_t cur = P(wCurBattleMon), ocur = P(wCurOTMon);
    for(uint8_t i = 0; i < P(wPartyCount) && i < PARTY_LENGTH; i++) {
        uint16_t p = (uint16_t)(wPartyMon1 + PARTYMON_STRUCT_LENGTH * i);
        if(i != cur && P16(p + MON_HP)) s->our_alive++;
        if(P(p + MON_STATUS)) s->party_statused = true;
    }
    for(uint8_t i = 0; i < P(wOTPartyCount) && i < PARTY_LENGTH; i++) {
        if(i != ocur && P16(wOTPartyMon1 + MON_HP + PARTYMON_STRUCT_LENGTH * i)) s->their_alive++;
    }
}

/* the fit teammate with the best matchup against the Game Boy's mon (or
 * its whole fit party if none is out); -1 if none. *value: its score */
static int best_teammate(const PkbEngine* E, bool switch_in, int* value) {
    AiMon them, cand;
    bool them_out = P16(wEnemyMonHP) != 0;
    if(them_out) ai_active(E, false, &them);
    int best = -1;
    *value = -10000;
    for(uint8_t i = 0; i < P(wPartyCount) && i < PARTY_LENGTH; i++) {
        if(i == P(wCurBattleMon)) continue;
        ai_party(E, true, i, &cand);
        if(!cand.hp) continue;
        int v = 0;
        if(them_out) {
            v = matchup(E, &cand, &them, switch_in);
        } else {
            int n = 0;
            for(uint8_t j = 0; j < P(wOTPartyCount) && j < PARTY_LENGTH; j++) {
                AiMon foe;
                ai_party(E, false, j, &foe);
                if(!foe.hp) continue;
                v += matchup(E, &cand, &foe, false);
                n++;
            }
            if(n) v /= n;
        }
        if(v > *value) {
            *value = v;
            best = i;
        }
    }
    return best;
}

void pkb_ai_init(PkbAi* ai, uint64_t seed) {
    memset(ai, 0, sizeof(*ai));
    pkb_policy_rng_seed(&ai->rng, seed);
}

PKB_TEXT2 void pkb_ai_choose_action(void* ctx, const PkbEngine* E, PkbChoice* c) {
    PkbAi* ai = ctx;
    Situation s;
    ai->turn++;
    situation(E, &s);

    int scores[NUM_MOVES], best = -1, best_slot = 0;
    uint8_t dis = disabled_slot(E, true);
    for(int i = 0; i < NUM_MOVES; i++) {
        scores[i] = -1;
        if(!usable(&s.us, i, dis)) continue;
        scores[i] = score_move(E, &s, s.us.moves[i]);
        if(scores[i] > best) {
            best = scores[i];
            best_slot = i;
        }
    }

    /* switch out of a clearly lost matchup */
    c->switch_mon = false;
    if(pkb_engine_can_switch(E)) {
        bool perish = (P(wPlayerSubStatus1) & (1 << SUBSTATUS_PERISH)) && P(wPlayerPerishCount) == 1;
        int stay = matchup(E, &s.us, &s.them, false) + s.boosts * 15;
        int go_value;
        int go = best_teammate(E, true, &go_value);
        bool recent = ai->last_switch && ai->turn - ai->last_switch < 2;
        if(go >= 0 && (perish || (!recent && s.boosts <= 0 && go_value > stay + 60 && best < 100))) {
            c->switch_mon = true;
            c->slot = (uint8_t)go;
            ai->last_switch = ai->turn;
            return;
        }
    }

    /* the best move, or now and then one that is nearly as good */
    c->slot = (uint8_t)best_slot;
    if(best > 0) {
        uint8_t near[NUM_MOVES];
        int n = 0;
        for(int i = 0; i < NUM_MOVES; i++)
            if(scores[i] >= 0 && scores[i] * 10 >= best * 9) near[n++] = (uint8_t)i;
        if(n > 1 && pkb_policy_rng_below(&ai->rng, 4) == 0) c->slot = near[pkb_policy_rng_below(&ai->rng, (uint32_t)n)];
    } else if(best < 0) {
        c->slot = 0; /* nothing usable: the engine uses Struggle */
    }
}

uint8_t pkb_ai_choose_replacement(void* ctx, const PkbEngine* E) {
    PkbAi* ai = ctx;
    (void)ai;
    int value;
    int i = best_teammate(E, false, &value);
    return i >= 0 ? (uint8_t)i : 0;
}
