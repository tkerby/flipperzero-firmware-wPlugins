#include "pkb_test.h"
#include <pkb/team.h>
#include "pb_tables.h"

/* Level 50, DV 0, stat exp 65535 (sqrt capped at 255 -> 63):
 *   stat = (2*base + 63) * 50 / 100 + 5,  HP = (2*base + 63) * 50 / 100 + 60 */
static void test_level50_team_formula(void) {
    /* Pikachu 35/55/30/90/50/40 */
    CHECK_EQ(pkb_calc_stat(35, 0, 0xFFFF, true, 50, true), 126);
    CHECK_EQ(pkb_calc_stat(55, 0, 0xFFFF, true, 50, false), 91);
    CHECK_EQ(pkb_calc_stat(30, 0, 0xFFFF, true, 50, false), 66);
    CHECK_EQ(pkb_calc_stat(90, 0, 0xFFFF, true, 50, false), 126);
    CHECK_EQ(pkb_calc_stat(50, 0, 0xFFFF, true, 50, false), 86);
    CHECK_EQ(pkb_calc_stat(40, 0, 0xFFFF, true, 50, false), 76);
    /* Blissey HP 255, Shuckle Def 230, Shuckle Atk 10 */
    CHECK_EQ(pkb_calc_stat(255, 0, 0xFFFF, true, 50, true), 346);
    CHECK_EQ(pkb_calc_stat(230, 0, 0xFFFF, true, 50, false), 266);
    CHECK_EQ(pkb_calc_stat(10, 0, 0xFFFF, true, 50, false), 46);
}

static void test_square_root_quirks(void) {
    /* base 0, DV 0, level 100: stat = sqrt(statexp)/4 + 5 */
    CHECK_EQ(pkb_calc_stat(0, 0, 0, true, 100, false), 5); /* sqrt(0) -> 1 */
    CHECK_EQ(pkb_calc_stat(0, 0, 16, true, 100, false), 6); /* 4 */
    CHECK_EQ(pkb_calc_stat(0, 0, 17, true, 100, false), 6); /* 5 (ceil) */
    CHECK_EQ(pkb_calc_stat(0, 0, 64, true, 100, false), 7); /* 8 */
    CHECK_EQ(pkb_calc_stat(0, 0, 65025, true, 100, false), 68); /* 255 */
    CHECK_EQ(pkb_calc_stat(0, 0, 65024, true, 100, false), 68); /* 255 (cap) */
    CHECK_EQ(pkb_calc_stat(0, 0, 0xFFFF, false, 100, false), 5); /* stat exp ignored */
}

static void test_dv_and_level_100(void) {
    /* Blissey HP at L100 with DV 15 and max stat exp: (270*2 + 63) + 110 */
    CHECK_EQ(pkb_calc_stat(255, 15, 0xFFFF, true, 100, true), 713);
    /* Mewtwo SpAtk 154, DV 15, L100: (169*2+63) + 5 */
    CHECK_EQ(pkb_calc_stat(154, 15, 0xFFFF, true, 100, false), 406);
}

static void test_team_mon_init(void) {
    /* random-team defaults: DVs 15 (HP DV 15), stat exp 65535 */
    const uint8_t moves[4] = {THUNDERBOLT, QUICK_ATTACK, THUNDER_WAVE, POUND};
    PkbTeamMon m;
    pkb_team_mon_init(&m, PIKACHU, 50, moves);
    CHECK_EQ(m.species, PIKACHU);
    CHECK_EQ(m.level, 50);
    CHECK_EQ(m.item, 0);
    CHECK_EQ(m.dvs[0], 0xFF);
    CHECK_EQ(m.happiness, 255);
    CHECK_EQ(m.max_hp, 141);
    CHECK_EQ(m.stats[PKB_STAT_ATK], 106);
    CHECK_EQ(m.stats[PKB_STAT_DEF], 81);
    CHECK_EQ(m.stats[PKB_STAT_SPD], 141);
    CHECK_EQ(m.stats[PKB_STAT_SAT], 101);
    CHECK_EQ(m.stats[PKB_STAT_SDF], 91);
    CHECK_EQ(m.pp[0], 15);
    CHECK_EQ(m.pp[1], 30);
    CHECK_EQ(m.pp[2], 20);
    CHECK_EQ(m.pp[3], 35);
    CHECK_EQ(m.exp, 125000); /* medium fast: 50^3 */
}

static void test_red_pikachu(void) {
    /* Red's Pikachu: L81, RED class DVs 15/13/13/14 (HP DV 14), no stat exp */
    PkbTeamMon m = {
        .species = PIKACHU,
        .level = 81,
        .moves = {CHARM, QUICK_ATTACK, THUNDERBOLT, THUNDER},
        .dvs = {0xFD, 0xDE},
        .stat_exp = 0,
        .happiness = 70};
    pkb_team_mon_calc(&m);
    CHECK_EQ(pkb_hp_dv(m.dvs), 14);
    CHECK_EQ(m.max_hp, 170);
    CHECK_EQ(m.stats[PKB_STAT_ATK], 118);
    CHECK_EQ(m.stats[PKB_STAT_DEF], 74);
    CHECK_EQ(m.stats[PKB_STAT_SPD], 171);
    CHECK_EQ(m.stats[PKB_STAT_SAT], 108);
    CHECK_EQ(m.stats[PKB_STAT_SDF], 92);
    CHECK_EQ(m.exp, 531441);
    CHECK_EQ(m.pp[0], 20);
}

static void test_exp_at_level(void) {
    /* from tools/gen/legality.py exp_at_level, levels 2, 5, 50, 81, 100 */
    static const uint32_t want[NUM_GROWTH_RATES][5] = {
        {8, 125, 125000, 531441, 1000000},
        {16, 313, 118720, 464160, 849970},
        {16, 523, 143680, 529730, 949930},
        {9, 135, 117360, 547274, 1059860},
        {6, 100, 100000, 425152, 800000},
        {10, 156, 156250, 664301, 1250000},
    };
    static const uint8_t levels[5] = {2, 5, 50, 81, 100};
    for(int g = 0; g < NUM_GROWTH_RATES; g++)
        for(int i = 0; i < 5; i++)
            CHECK_EQ(pkb_exp_at_level((uint8_t)g, levels[i]), want[g][i]);
}

static void test_generated_tables_spot_checks(void) {
    const PbBaseStats* mewtwo = &pb_base_stats[MEWTWO - 1];
    CHECK_EQ(mewtwo->stats[0], 106);
    CHECK_EQ(mewtwo->stats[4], 154);
    CHECK_EQ(mewtwo->types[0], PSYCHIC_TYPE);
    CHECK_EQ(pb_base_stats[MAGNEMITE - 1].gender, 0xFF); /* GENDER_UNKNOWN */
    CHECK_EQ(pb_moves[TACKLE - 1].power, 35);
    CHECK_EQ(pb_moves[TACKLE - 1].accuracy, 242); /* 95 percent */
    CHECK_EQ(pb_moves[FIRE_PUNCH - 1].effect, EFFECT_BURN_HIT);
    CHECK_EQ(pb_moves[FIRE_PUNCH - 1].effect_chance, 25); /* 10 percent */
    CHECK_EQ(NUM_POKEMON, 251);
    CHECK_EQ(NUM_ATTACKS, 251);
}

int main(void) {
    RUN(test_level50_team_formula);
    RUN(test_square_root_quirks);
    RUN(test_dv_and_level_100);
    RUN(test_team_mon_init);
    RUN(test_red_pikachu);
    RUN(test_exp_at_level);
    RUN(test_generated_tables_spot_checks);
    return TEST_REPORT();
}
