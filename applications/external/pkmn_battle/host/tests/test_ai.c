/*
 * The AI's damage estimate against the engine's own DamageStats, DamageCalc
 * and Stab (pkb_engine_test_damage): fuzzed parties, both battle mons as at
 * the first battle menu, then randomized battle stats, held items, screens,
 * weather and Foresight. The estimate's max roll must equal the engine's
 * damage before the random roll for every move whose power comes from the
 * move struct.
 */
#include <stdlib.h>

#include "fuzz_party.h"
#include "pkb_test.h"
#include <pkb/ai.h>
#include <pkb/engine.h>
#include "pb_const.h"
#include "pb_tables.h"
#include "pb_wram.h"

#define BATTLES 1500

static PkbEngine live, est, calc;
static PkbPolicyRng rng;
static unsigned compared, mismatches;

/* moves whose damage comes from other commands, or none */
static bool plain_damage(uint8_t move) {
    const PbMove* m = &pb_moves[move - 1];
    if(m->power <= 1) return false;
    switch(m->effect) {
    case EFFECT_LEVEL_DAMAGE:
    case EFFECT_STATIC_DAMAGE:
    case EFFECT_SUPER_FANG:
    case EFFECT_PSYWAVE:
    case EFFECT_OHKO:
    case EFFECT_COUNTER:
    case EFFECT_MIRROR_COAT:
    case EFFECT_BIDE:
    case EFFECT_RETURN:
    case EFFECT_FRUSTRATION:
    case EFFECT_HIDDEN_POWER:
    case EFFECT_MAGNITUDE:
    case EFFECT_PRESENT:
    case EFFECT_REVERSAL:
    case EFFECT_TRIPLE_KICK:
    case EFFECT_BEAT_UP:
    case EFFECT_DREAM_EATER:
    case EFFECT_SNORE:
        return false;
    }
    return true;
}

static void poke(PkbEngine* E, uint16_t a, uint8_t v) {
    E->wram[a - 0xC000] = v;
}

static void poke16(PkbEngine* E, uint16_t a, uint16_t v) {
    poke(E, a, (uint8_t)(v >> 8));
    poke(E, (uint16_t)(a + 1), (uint8_t)v);
}

static const uint8_t items[] = {
    0,
    LEFTOVERS,
    CHARCOAL,
    MYSTIC_WATER,
    PINK_BOW,
    BLACKBELT_I,
    TWISTEDSPOON,
    MAGNET,
    NEVERMELTICE,
    SPELL_TAG,
    LIGHT_BALL,
    THICK_CLUB,
    METAL_POWDER,
    BRIGHTPOWDER};

static void randomize(PkbEngine* E) {
    for(int side = 0; side < 2; side++) {
        uint16_t mon = side ? wEnemyMon : wBattleMon;
        for(int s = 0; s < 5; s++) {
            /* mostly game-sized stats, sometimes huge ones (truncation) */
            uint16_t v = (uint16_t)(1 + pkb_policy_rng_below(
                                            &rng, pkb_policy_rng_below(&rng, 4) ? 400 : 999));
            poke16(E, (uint16_t)(mon + (wBattleMonAttack - wBattleMon) + 2 * s), v);
        }
        poke(
            E,
            (uint16_t)(mon + (wBattleMonItem - wBattleMon)),
            items[pkb_policy_rng_below(&rng, sizeof(items))]);
        poke(
            E,
            (uint16_t)(mon + (wBattleMonLevel - wBattleMon)),
            (uint8_t)(2 + pkb_policy_rng_below(&rng, 99)));
        poke(
            E,
            side ? wEnemyScreens : wPlayerScreens,
            (uint8_t)(pkb_policy_rng_below(&rng, 4) << SCREENS_LIGHT_SCREEN));
        uint8_t ss1 = E->wram[(side ? wEnemySubStatus1 : wPlayerSubStatus1) - 0xC000];
        if(pkb_policy_rng_below(&rng, 4) == 0) ss1 ^= 1 << SUBSTATUS_IDENTIFIED;
        poke(E, side ? wEnemySubStatus1 : wPlayerSubStatus1, ss1);
    }
    /* now and then a Pikachu / Marowak / Ditto, for the species items */
    static const uint8_t species[] = {PIKACHU, MAROWAK, CUBONE, DITTO};
    if(pkb_policy_rng_below(&rng, 3) == 0) {
        uint8_t sp = species[pkb_policy_rng_below(&rng, sizeof(species))];
        poke(
            E,
            (uint16_t)(wPartyMon1Species +
                       PARTYMON_STRUCT_LENGTH * E->wram[wCurBattleMon - 0xC000]),
            sp);
    }
    if(pkb_policy_rng_below(&rng, 3) == 0) {
        poke(E, wTempEnemyMonSpecies, species[pkb_policy_rng_below(&rng, sizeof(species))]);
    }
    poke(E, wBattleWeather, (uint8_t)pkb_policy_rng_below(&rng, 4));
}

static void at_menu(void* ctx, const PkbEngine* E, PkbChoice* c) {
    (void)ctx;
    c->switch_mon = false;
    c->slot = 0;
    for(int round = 0; round < 4; round++) {
        est = *E;
        randomize(&est);
        for(uint8_t move = 1; move <= NUM_ATTACKS && move != 0; move++) {
            if(!plain_damage(move)) continue;
            for(int player = 0; player < 2; player++) {
                PkbAiDamage dm;
                pkb_ai_active_damage(&est, player, move, &dm);
                calc = est;
                uint16_t want = pkb_engine_test_damage(&calc, player, move);
                compared++;
                if(dm.max != want && mismatches++ < 10) {
                    fprintf(
                        stderr,
                        "move %u player %d: estimate %u, engine %u\n",
                        move,
                        player,
                        dm.max,
                        want);
                }
                CHECK(dm.min <= dm.max);
            }
        }
    }
}

static bool stop(void* ctx, uint8_t mine, uint8_t* theirs) {
    (void)ctx;
    (void)mine;
    (void)theirs;
    return false; /* one battle menu per battle is enough */
}

static uint8_t replace(void* ctx, const PkbEngine* E) {
    (void)ctx;
    (void)E;
    return 0;
}

static void test_damage_matches_engine(void) {
    const PkbEngineIo io = {
        .exchange = stop, .choose_action = at_menu, .choose_replacement = replace};
    pkb_policy_rng_seed(&rng, 99);
    for(int b = 0; b < BATTLES; b++) {
        PkbLinkPartyData ours, theirs;
        uint8_t rns[PKB_RNS_LEN];
        fuzz_party(&rng, &ours);
        fuzz_party(&rng, &theirs);
        for(int i = 0; i < PKB_RNS_LEN; i++)
            rns[i] = (uint8_t)pkb_policy_rng_below(&rng, 0xFD);
        pkb_engine_init(&live, false, rns, &ours, &theirs);
        pkb_engine_run(&live, &io);
    }
    printf("%u damage estimates compared, %u mismatches\n", compared, mismatches);
    CHECK(compared > 100000);
    CHECK_EQ(mismatches, 0);
}

int main(void) {
    RUN(test_damage_matches_engine);
    return TEST_REPORT();
}
