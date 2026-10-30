#include "pkb_test.h"
#include <pkb/random.h>

static void test_first_nine_pulls_read_table_in_order(void) {
    uint8_t rns[PKB_LINK_RN_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint8_t count = 0;

    for(int i = 0; i < 8; i++) {
        CHECK_EQ(pkb_link_battle_random(rns, &count), i);
        CHECK_EQ(count, i + 1);
    }
    /* 9th pull returns RN[8] and regenerates the table */
    CHECK_EQ(pkb_link_battle_random(rns, &count), 8);
    CHECK_EQ(count, 0);

    static const uint8_t expected[PKB_LINK_RN_COUNT] = {1, 6, 11, 16, 21, 26, 31, 36, 41, 46};
    CHECK_MEM_EQ(rns, expected, sizeof(expected));
}

static void test_rn9_is_never_returned_directly(void) {
    uint8_t rns[PKB_LINK_RN_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0xAA};
    uint8_t count = 0;

    for(int i = 0; i < 9; i++) {
        CHECK(pkb_link_battle_random(rns, &count) != 0xAA);
    }
    /* 0xAA * 5 + 1 = 0x353 -> 0x53 */
    CHECK_EQ(rns[9], 0x53);
    CHECK_EQ(pkb_link_battle_random(rns, &count), 1);
}

static void test_seed_update_wraps_mod_256(void) {
    uint8_t rns[PKB_LINK_RN_COUNT] = {0xFF, 0x33, 0x34, 0x80, 0xFC, 0, 0, 0, 0, 0};
    uint8_t count = 8;

    CHECK_EQ(pkb_link_battle_random(rns, &count), 0);
    CHECK_EQ(count, 0);
    CHECK_EQ(rns[0], 0xFC); /* 0xFF*5+1 = 0x4FC */
    CHECK_EQ(rns[1], 0x00); /* 0x33*5+1 = 0x100 */
    CHECK_EQ(rns[2], 0x05); /* 0x34*5+1 = 0x105 */
    CHECK_EQ(rns[3], 0x81); /* 0x80*5+1 = 0x281 */
    CHECK_EQ(rns[4], 0xED); /* 0xFC*5+1 = 0x4ED */
}

static void test_stream_matches_reference_model(void) {
    uint8_t rns[PKB_LINK_RN_COUNT] = {0x12, 0x9C, 0x47, 0x03, 0xF0, 0x5A, 0x77, 0xE1, 0x2B, 0x60};
    uint8_t model[PKB_LINK_RN_COUNT];
    uint8_t count = 0;
    memcpy(model, rns, sizeof(model));

    for(int pull = 0; pull < 1000; pull++) {
        int idx = pull % 9;
        uint8_t want = model[idx];
        if(idx == 8) {
            for(int k = 0; k < PKB_LINK_RN_COUNT; k++)
                model[k] = (uint8_t)(model[k] * 5 + 1);
        }
        CHECK_EQ(pkb_link_battle_random(rns, &count), want);
    }
}

int main(void) {
    RUN(test_first_nine_pulls_read_table_in_order);
    RUN(test_rn9_is_never_returned_directly);
    RUN(test_seed_update_wraps_mod_256);
    RUN(test_stream_matches_reference_model);
    return TEST_REPORT();
}
