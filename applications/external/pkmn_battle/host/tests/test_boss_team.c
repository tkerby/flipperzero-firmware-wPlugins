#include "pkb_test.h"
#include <pkb/team.h>
#include "pb_tables.h"
#include "pb_trainers.h"
#include "pb_battle.h"

static int find_boss(const char* label) {
    for(int i = 0; i < PB_NUM_BOSSES; i++)
        if(strcmp(pb_bosses[i].label, label) == 0) return i;
    return -1;
}

static void test_table(void) {
    int per_cat[PB_NUM_BOSS_CATEGORIES] = {0};
    CHECK_EQ(PB_NUM_BOSSES, 26);
    for(int i = 0; i < PB_NUM_BOSSES; i++) {
        const PbBoss* b = &pb_bosses[i];
        per_cat[b->category]++;
        CHECK(b->count >= 1 && b->count <= 6);
        CHECK(strlen(b->name) >= 1 && strlen(b->name) <= 10);
        for(int k = 0; k < b->count; k++) {
            const PbBossMon* m = &b->mon[k];
            CHECK(m->species >= 1 && m->species <= NUM_POKEMON);
            CHECK(m->level >= 2 && m->level <= 100);
            CHECK(m->item != 0); /* every boss mon holds something */
            CHECK(pb_item_held_effect[m->item - 1][0] != 0 || m->item == LIGHT_BALL);
            CHECK(m->moves[0] != 0);
        }
    }
    CHECK_EQ(per_cat[PB_BOSS_GYM], 16);
    CHECK_EQ(per_cat[PB_BOSS_E4], 4);
    CHECK_EQ(per_cat[PB_BOSS_RIVAL], 3);
    CHECK_EQ(per_cat[PB_BOSS_RED], 1);
    CHECK_EQ(per_cat[PB_BOSS_CHAMPION], 1);
    CHECK_EQ(per_cat[PB_BOSS_OAK], 1);
}

/* TrainerPicPointers order: Falkner is class 1, Lance is CHAMPION, the three
 * Silver rows share RIVAL2, Red is his own class. */
static void test_trainer_class(void) {
    CHECK_EQ(pb_bosses[find_boss("Falkner")].trainer_class, 0x01);
    CHECK_EQ(pb_bosses[find_boss("Lance")].trainer_class, 0x10);
    CHECK_EQ(pb_bosses[find_boss("Meganium")].trainer_class, 0x2A);
    CHECK_EQ(pb_bosses[find_boss("Typhlosion")].trainer_class, 0x2A);
    CHECK_EQ(pb_bosses[find_boss("Feraligatr")].trainer_class, 0x2A);
    CHECK_EQ(pb_bosses[find_boss("Red")].trainer_class, 0x3F);
    CHECK_EQ(pb_bosses[find_boss("Pryce")].trainer_class, 0x05);
}

/* data/trainers/parties.asm RedGroup and dvs.asm RED */
static void test_red(void) {
    static const uint8_t want[6][6] = {
        {81, PIKACHU, CHARM, QUICK_ATTACK, THUNDERBOLT, THUNDER},
        {73, ESPEON, MUD_SLAP, REFLECT, SWIFT, PSYCHIC_M},
        {75, SNORLAX, AMNESIA, SNORE, REST, BODY_SLAM},
        {77, VENUSAUR, SUNNY_DAY, GIGA_DRAIN, SYNTHESIS, SOLARBEAM},
        {77, CHARIZARD, FLAMETHROWER, WING_ATTACK, SLASH, FIRE_SPIN},
        {77, BLASTOISE, RAIN_DANCE, SURF, BLIZZARD, WHIRLPOOL},
    };
    int red = find_boss("Red");
    PkbTeam t;
    CHECK(red >= 0);
    CHECK(pkb_boss_team_build(&t, (uint8_t)red, 0, 0x1234));
    CHECK(strcmp(t.name, "RED") == 0);
    CHECK_EQ(t.trainer_id, 0x1234);
    CHECK_EQ(t.count, 6);
    for(int i = 0; i < 6; i++) {
        CHECK_EQ(t.mon[i].level, want[i][0]);
        CHECK_EQ(t.mon[i].species, want[i][1]);
        for(int k = 0; k < 4; k++)
            CHECK_EQ(t.mon[i].moves[k], want[i][2 + k]);
        CHECK_EQ(t.mon[i].dvs[0], 0xFD);
        CHECK_EQ(t.mon[i].dvs[1], 0xDE);
        CHECK_EQ(t.mon[i].stat_exp, 0);
        CHECK_EQ(t.mon[i].happiness, BASE_HAPPINESS);
    }
    CHECK_EQ(t.mon[0].item, LIGHT_BALL);
    CHECK_EQ(t.mon[0].max_hp, 170); /* see test_stats test_red_pikachu */
    CHECK_EQ(t.mon[0].stats[PKB_STAT_SPD], 171);
}

static void test_short_moveset_and_scaling(void) {
    int falkner = find_boss("Falkner");
    PkbTeam t;
    CHECK(falkner >= 0);
    CHECK_EQ(pkb_boss_ace_level((uint8_t)falkner), 9);
    CHECK(pkb_boss_team_build(&t, (uint8_t)falkner, 0, 1));
    CHECK_EQ(t.count, 2);
    CHECK_EQ(t.mon[0].moves[2], 0);
    CHECK_EQ(t.mon[0].pp[2], 0);
    CHECK_EQ(t.mon[0].dvs[0], 0x9A);

    /* ace to 100: Pidgey 7 -> 98, Pidgeotto 9 -> 100 */
    CHECK(pkb_boss_team_build(&t, (uint8_t)falkner, 100, 1));
    CHECK_EQ(t.mon[0].level, 98);
    CHECK_EQ(t.mon[1].level, 100);
    CHECK_EQ(t.mon[1].exp, pkb_exp_at_level(pb_base_stats[PIDGEOTTO - 1].growth_rate, 100));

    /* Red down to 50: Pikachu 81 -> 50, Espeon 73 -> 42 */
    int red = find_boss("Red");
    CHECK(pkb_boss_team_build(&t, (uint8_t)red, 50, 1));
    CHECK_EQ(t.mon[0].level, 50);
    CHECK_EQ(t.mon[1].level, 42);

    /* clamped at the bottom */
    CHECK(pkb_boss_team_build(&t, (uint8_t)red, 9, 1));
    CHECK_EQ(t.mon[1].level, PKB_LEVEL_MIN);
    CHECK(!pkb_boss_team_build(&t, PB_NUM_BOSSES, 0, 1));
}

static void test_rival_and_lance(void) {
    int i = find_boss("Typhlosion");
    PkbTeam t;
    CHECK(i >= 0);
    CHECK(pkb_boss_team_build(&t, (uint8_t)i, 0, 1));
    CHECK(strcmp(t.name, "SILVER") == 0);
    CHECK_EQ(t.mon[5].species, TYPHLOSION);
    CHECK_EQ(t.mon[5].level, 50);
    CHECK_EQ(t.mon[1].species, CROBAT);
    CHECK_EQ(t.mon[0].dvs[0], 0x98); /* RIVAL2 DVs (lower than RIVAL1's, a game bug) */

    i = find_boss("Lance");
    CHECK_EQ(pb_bosses[i].category, PB_BOSS_CHAMPION);
    CHECK(pkb_boss_team_build(&t, (uint8_t)i, 0, 1));
    CHECK(strcmp(t.name, "LANCE") == 0);
    CHECK_EQ(t.mon[5].species, DRAGONITE);
    CHECK_EQ(t.mon[5].moves[2], OUTRAGE);
}

/* Prof. Oak: Red DVs, stat exp 0, #MON PROF picture, the listed party. */
static void test_oak(void) {
    static const uint8_t want_species[] = {TAUROS, EXEGGUTOR, ARCANINE, GYARADOS, VENUSAUR, MEW};
    static const uint8_t want_level[] = {79, 80, 81, 82, 83, 85};
    static const uint8_t want_item[] = {
        LEFTOVERS, MIRACLEBERRY, LEFTOVERS, MYSTIC_WATER, LEFTOVERS, LEFTOVERS};
    static const uint8_t want_moves[6][4] = {
        {DOUBLE_EDGE, EARTHQUAKE, REST, SLEEP_TALK},
        {SLEEP_POWDER, PSYCHIC_M, GIGA_DRAIN, EXPLOSION},
        {FLAMETHROWER, EXTREMESPEED, CRUNCH, MORNING_SUN},
        {RAIN_DANCE, SURF, THUNDER, HYPER_BEAM},
        {SUNNY_DAY, GIGA_DRAIN, SYNTHESIS, SOLARBEAM},
        {SOFTBOILED, PSYCHIC_M, ICE_BEAM, THUNDERBOLT},
    };
    int oak = find_boss("Prof. Oak");
    PkbTeam t;
    CHECK(oak >= 0);
    CHECK_EQ(pb_bosses[oak].category, PB_BOSS_OAK);
    CHECK_EQ(pb_bosses[oak].trainer_class, 0x0A);
    CHECK(pkb_boss_team_build(&t, (uint8_t)oak, 0, 1));
    CHECK(strcmp(t.name, "OAK") == 0);
    CHECK_EQ(t.count, 6);
    CHECK_EQ(pkb_boss_ace_level((uint8_t)oak), 85);
    for(int i = 0; i < 6; i++) {
        CHECK_EQ(t.mon[i].species, want_species[i]);
        CHECK_EQ(t.mon[i].level, want_level[i]);
        CHECK_EQ(t.mon[i].item, want_item[i]);
        CHECK_EQ(t.mon[i].dvs[0], 0xFD);
        CHECK_EQ(t.mon[i].dvs[1], 0xDE);
        CHECK_EQ(t.mon[i].stat_exp, 0);
        for(int k = 0; k < 4; k++)
            CHECK_EQ(t.mon[i].moves[k], want_moves[i][k]);
    }
}

int main(void) {
    RUN(test_table);
    RUN(test_trainer_class);
    RUN(test_red);
    RUN(test_short_moveset_and_scaling);
    RUN(test_rival_and_lance);
    RUN(test_oak);
    return TEST_REPORT();
}
