#include "pkb_test.h"
#include "host_io.h"
#include <pkb/team.h>
#include "pb_tables.h"

static HostBlob pools_storage;

static void test_movepool_file(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    uint8_t pool[PKB_MOVEPOOL_MAX];
    PkbSpeciesInfo info;
    CHECK(pkb_movepool_check(&r));
    CHECK_EQ(pkb_movepool_read(&r, DITTO, 50, pool, NULL), 1);
    CHECK_EQ(pool[0], TRANSFORM);
    CHECK_EQ(pkb_movepool_read(&r, WOBBUFFET, 50, pool, NULL), 4);
    CHECK_EQ(pkb_movepool_read(&r, 0, 50, pool, NULL), -1);
    CHECK_EQ(pkb_movepool_read(&r, 252, 50, pool, NULL), -1);
    /* Dragonite exists from level 55 */
    CHECK_EQ(pkb_movepool_read(&r, DRAGONITE, 54, pool, &info), 0);
    CHECK_EQ(info.min_level, 55);
    CHECK_EQ(info.evolve_level, 0);
    pkb_movepool_read(&r, DRAGONAIR, 54, pool, &info);
    CHECK_EQ(info.evolve_level, 55);
    pkb_movepool_read(&r, KADABRA, 54, pool, &info);
    CHECK_EQ(info.evolve_level, 30); /* trade */
    CHECK(pkb_movepool_read(&r, DRAGONITE, 55, pool, NULL) >= 4);
    /* egg moves: Pikachu (via Pichu) may know Encore; not before hatching */
    int n = pkb_movepool_read(&r, PIKACHU, 50, pool, NULL);
    bool encore = false;
    for(int i = 0; i < n; i++)
        encore |= pool[i] == ENCORE;
    CHECK(encore);
    for(int s = 1; s <= NUM_POKEMON; s++) {
        int prev = 0;
        for(int level = PKB_LEVEL_MIN; level <= PKB_LEVEL_MAX; level++) {
            n = pkb_movepool_read(&r, (uint8_t)s, (uint8_t)level, pool, &info);
            CHECK(n >= 0);
            CHECK(n >= prev); /* pools only grow with the level */
            CHECK_EQ(n == 0, level < info.min_level);
            for(int i = 0; i < n; i++) {
                CHECK(pool[i] >= 1 && pool[i] <= NUM_ATTACKS);
                CHECK(pool[i] != BEAT_UP);
                if(i) CHECK(pool[i] > pool[i - 1]);
            }
            prev = n;
        }
    }
}

static int eligible_at(const PkbBlobReader* r, uint8_t level) {
    uint8_t pool[PKB_MOVEPOOL_MAX];
    int n = 0;
    for(int s = 1; s <= NUM_POKEMON; s++)
        n += pkb_movepool_read(r, (uint8_t)s, level, pool, NULL) >= 4;
    return n;
}

static void test_eligible_species(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    uint8_t pool[PKB_MOVEPOOL_MAX];
    /* tools/gen/legality.py: 10 excluded at level 50 (EXPECTED_EXCLUDED), 8 at 100 */
    CHECK_EQ(eligible_at(&r, 50), NUM_POKEMON - 10);
    CHECK_EQ(eligible_at(&r, 100), NUM_POKEMON - 8);
    static const uint8_t never[] = {
        CATERPIE, METAPOD, WEEDLE, KAKUNA, MAGIKARP, DITTO, UNOWN, SMEARGLE};
    for(size_t i = 0; i < sizeof(never); i++)
        CHECK(pkb_movepool_read(&r, never[i], 100, pool, NULL) < 4);
    CHECK(pkb_movepool_read(&r, TYRANITAR, 100, pool, NULL) >= 4);
    CHECK(pkb_movepool_read(&r, MEW, 50, pool, NULL) >= 4);
    CHECK(pkb_movepool_read(&r, CELEBI, 50, pool, NULL) >= 4);
}

static bool legal_at(const PkbBlobReader* r, uint8_t species, uint8_t level, uint8_t move) {
    uint8_t pool[PKB_MOVEPOOL_MAX];
    int n = pkb_movepool_read(r, species, level, pool, NULL);
    for(int i = 0; i < n; i++)
        if(pool[i] == move) return true;
    return false;
}

static void
    check_team(const PkbBlobReader* r, const PkbTeam* t, int count, uint8_t lo, uint8_t hi) {
    CHECK_EQ(t->count, count);
    CHECK(strcmp(t->name, "FLIPPER") == 0);
    for(int i = 0; i < t->count; i++) {
        const PkbTeamMon* m = &t->mon[i];
        uint8_t pool[PKB_MOVEPOOL_MAX];
        PkbSpeciesInfo info;
        CHECK(m->level <= hi);
        CHECK(pkb_movepool_read(r, m->species, m->level, pool, &info) >= 4);
        CHECK(m->level >= lo || m->level == info.min_level);
        CHECK(m->level >= info.min_level);
        CHECK(!info.evolve_level || m->level <= info.evolve_level + PKB_NFE_LEVEL_SLACK);
        CHECK(m->item != 0);
        for(int j = 0; j < i; j++)
            CHECK(t->mon[j].species != m->species);
        for(int k = 0; k < 4; k++) {
            CHECK(legal_at(r, m->species, m->level, m->moves[k]));
            CHECK_EQ(m->pp[k], pb_moves[m->moves[k] - 1].pp);
            for(int l = 0; l < k; l++)
                CHECK(m->moves[l] != m->moves[k]);
        }
        CHECK(m->max_hp > 0);
    }
}

static void test_random_teams(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    PkbPolicyRng rng;
    int seen[NUM_POKEMON + 1] = {0};
    for(uint64_t seed = 0; seed < 2000; seed++) {
        PkbTeam t;
        pkb_policy_rng_seed(&rng, seed);
        CHECK_EQ(pkb_team_generate(&t, 6, 50, 50, &r, NULL, NULL, &rng), PkbTeamOk);
        check_team(&r, &t, 6, 50, 50);
        for(int i = 0; i < t.count; i++)
            seen[t.mon[i].species]++;
    }
    /* 12000 draws cover every species usable at 50: 4+ moves, not too far
     * past its evolution */
    for(int s = 1; s <= NUM_POKEMON; s++) {
        uint8_t pool[PKB_MOVEPOOL_MAX];
        PkbSpeciesInfo info;
        bool usable = pkb_movepool_read(&r, (uint8_t)s, 50, pool, &info) >= 4 &&
                      (!info.evolve_level || 50 <= info.evolve_level + PKB_NFE_LEVEL_SLACK);
        CHECK_EQ(seen[s] > 0, usable);
    }
}

static void test_level_ranges(void) {
    static const uint8_t ranges[][2] = {{2, 2}, {5, 10}, {50, 50}, {75, 80}, {2, 100}, {100, 100}};
    PkbBlobReader r = host_movepools(&pools_storage);
    PkbPolicyRng rng;
    for(size_t c = 0; c < sizeof(ranges) / sizeof(ranges[0]); c++) {
        bool hit_lo = false, hit_hi = false;
        for(uint64_t seed = 0; seed < 300; seed++) {
            PkbTeam t;
            pkb_policy_rng_seed(&rng, seed);
            CHECK_EQ(
                pkb_team_generate(&t, 6, ranges[c][0], ranges[c][1], &r, NULL, NULL, &rng),
                PkbTeamOk);
            check_team(&r, &t, 6, ranges[c][0], ranges[c][1]);
            for(int i = 0; i < 6; i++) {
                hit_lo |= t.mon[i].level == ranges[c][0];
                hit_hi |= t.mon[i].level == ranges[c][1];
                if(t.mon[i].species == DRAGONITE || t.mon[i].species == TYRANITAR)
                    CHECK(t.mon[i].level >= 55);
            }
        }
        CHECK(hit_lo);
        CHECK(hit_hi);
    }
}

static bool normal_hit_only(void* ctx, uint8_t move) {
    (void)ctx;
    return pb_moves[move - 1].effect == EFFECT_NORMAL_HIT;
}

static void test_filtered_teams(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    PkbPolicyRng rng;
    for(uint64_t seed = 0; seed < 500; seed++) {
        PkbTeam t;
        pkb_policy_rng_seed(&rng, seed);
        CHECK_EQ(pkb_team_generate(&t, 6, 50, 50, &r, normal_hit_only, NULL, &rng), PkbTeamOk);
        check_team(&r, &t, 6, 50, 50);
        for(int i = 0; i < 6; i++)
            for(int k = 0; k < 4; k++)
                CHECK(normal_hit_only(NULL, t.mon[i].moves[k]));
    }
}

static bool nothing(void* ctx, uint8_t move) {
    (void)ctx;
    (void)move;
    return false;
}

static void test_impossible_filter(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    PkbPolicyRng rng;
    PkbTeam t;
    pkb_policy_rng_seed(&rng, 1);
    CHECK_EQ(pkb_team_generate(&t, 6, 50, 50, &r, nothing, NULL, &rng), PkbTeamErrNotEnough);
}

static void test_deterministic_for_seed(void) {
    PkbBlobReader r = host_movepools(&pools_storage);
    PkbPolicyRng a, b;
    PkbTeam ta, tb;
    pkb_policy_rng_seed(&a, 42);
    pkb_policy_rng_seed(&b, 42);
    pkb_team_generate(&ta, 6, 60, 70, &r, NULL, NULL, &a);
    pkb_team_generate(&tb, 6, 60, 70, &r, NULL, NULL, &b);
    CHECK_MEM_EQ(&ta, &tb, sizeof(ta)); /* the generator clears the whole struct */
}

int main(void) {
    RUN(test_movepool_file);
    RUN(test_eligible_species);
    RUN(test_random_teams);
    RUN(test_level_ranges);
    RUN(test_filtered_teams);
    RUN(test_impossible_filter);
    RUN(test_deterministic_for_seed);
    return TEST_REPORT();
}
