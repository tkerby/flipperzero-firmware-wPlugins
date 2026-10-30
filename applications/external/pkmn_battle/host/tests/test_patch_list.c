#include "pkb_test.h"
#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

static void check_list_shape(const uint8_t* patch) {
    for(int i = 0; i < 3; i++)
        CHECK_EQ(patch[i], 0xFD);
    for(int i = 3; i < 10; i++)
        CHECK_EQ(patch[i], 0);
}

static void test_no_fe_bytes(void) {
    uint8_t region[PKB_PATCH_REGION_LEN];
    uint8_t patch[PKB_PATCH_BLOCK_LEN];
    for(size_t i = 0; i < sizeof(region); i++)
        region[i] = (uint8_t)(i % 0xFE);
    CHECK(pkb_patch_list_build(region, sizeof(region), patch));
    check_list_shape(patch);
    CHECK_EQ(patch[10], 0xFF);
    CHECK_EQ(patch[11], 0xFF);
    CHECK_EQ(patch[12], 0);
}

static void test_known_positions(void) {
    uint8_t region[PKB_PATCH_REGION_LEN] = {0};
    uint8_t patch[PKB_PATCH_BLOCK_LEN];
    region[0] = 0xFE; /* part 1 index 1 */
    region[251] = 0xFE; /* part 1 index 252 (last of SERIAL_PATCH_DATA_SIZE) */
    region[252] = 0xFE; /* part 2 index 1 */
    region[289] = 0xFE; /* part 2 index 38 */
    CHECK(pkb_patch_list_build(region, sizeof(region), patch));
    check_list_shape(patch);
    static const uint8_t want[] = {1, 252, 0xFF, 1, 38, 0xFF, 0};
    CHECK_MEM_EQ(patch + 10, want, sizeof(want));
    CHECK_EQ(region[0], 0xFF);
    CHECK_EQ(region[289], 0xFF);
}

static void test_round_trip_fe_heavy(void) {
    PkbPolicyRng rng;
    pkb_policy_rng_seed(&rng, 1234);
    for(int iter = 0; iter < 2000; iter++) {
        uint8_t orig[PKB_PATCH_REGION_LEN], region[PKB_PATCH_REGION_LEN];
        uint8_t patch[PKB_PATCH_BLOCK_LEN];
        int fe = 0;
        uint32_t density = 1 + pkb_policy_rng_below(&rng, 60); /* up to 60% $FE */
        for(size_t i = 0; i < sizeof(orig); i++) {
            bool want_fe = pkb_policy_rng_below(&rng, 100) < density && fe < 188;
            orig[i] = want_fe ? 0xFE : (uint8_t)pkb_policy_rng_below(&rng, 0xFE);
            fe += want_fe;
        }
        memcpy(region, orig, sizeof(orig));
        CHECK(pkb_patch_list_build(region, sizeof(region), patch));
        for(size_t i = 0; i < sizeof(region); i++)
            CHECK(region[i] != 0xFE);
        pkb_patch_list_apply(patch, sizeof(patch), region, sizeof(region));
        CHECK_MEM_EQ(region, orig, sizeof(orig));
    }
}

static void test_overflow_is_reported(void) {
    uint8_t region[PKB_PATCH_REGION_LEN];
    uint8_t patch[PKB_PATCH_BLOCK_LEN];
    memset(region, 0xFE, sizeof(region));
    CHECK(!pkb_patch_list_build(region, sizeof(region), patch));
    /* 188 entries + 2 terminators exactly fill bytes 10..199 */
    memset(region, 0, sizeof(region));
    memset(region, 0xFE, 188);
    CHECK(pkb_patch_list_build(region, sizeof(region), patch));
    CHECK_EQ(patch[198], 0xFF);
    CHECK_EQ(patch[199], 0xFF);
}

static void test_apply_skips_control_bytes(void) {
    /* As received: leading $FE/$FD and zeros are skipped */
    uint8_t region[PKB_PATCH_REGION_LEN] = {0};
    uint8_t patch[PKB_PATCH_BLOCK_LEN] = {0xFE, 0xFE, 0xFD, 0xFD, 0xFD, 0, 0, 5, 0xFF, 2, 0xFF, 7};
    pkb_patch_list_apply(patch, sizeof(patch), region, sizeof(region));
    CHECK_EQ(region[4], 0xFE);
    CHECK_EQ(region[0xFC + 1], 0xFE);
    CHECK_EQ(region[6], 0); /* after the second terminator: ignored */
}

int main(void) {
    RUN(test_no_fe_bytes);
    RUN(test_known_positions);
    RUN(test_round_trip_fe_heavy);
    RUN(test_overflow_is_reported);
    RUN(test_apply_skips_control_bytes);
    return TEST_REPORT();
}
