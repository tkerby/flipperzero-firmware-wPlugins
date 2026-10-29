#include "pkb_test.h"
#include <pkb/charset.h>
#include <pkb/team.h>
#include "pb_const.h"

static void test_encode_trainer_name(void) {
    uint8_t out[11];
    static const uint8_t want[11] = {
        0x85, 0x8B, 0x88, 0x8F, 0x8F, 0x84, 0x91, 0x50, 0x50, 0x50, 0x50};
    pkb_gb_encode("FLIPPER", out, sizeof(out));
    CHECK_MEM_EQ(out, want, sizeof(want));
}

static void test_encode_truncates_and_terminates(void) {
    uint8_t out[5];
    pkb_gb_encode("ABCDEFG", out, sizeof(out));
    CHECK_EQ(out[3], 0x83);
    CHECK_EQ(out[4], 0x50);
    pkb_gb_encode("a~", out, sizeof(out));
    CHECK_EQ(out[0], 0xA0);
    CHECK_EQ(out[1], 0xE6); /* unmapped -> '?' */
}

static void test_species_names(void) {
    char buf[16];
    pkb_species_name(BULBASAUR, buf, sizeof(buf));
    CHECK(strcmp(buf, "BULBASAUR") == 0);
    pkb_species_name(MR__MIME, buf, sizeof(buf));
    CHECK(strcmp(buf, "MR.MIME") == 0);
    pkb_species_name(FARFETCH_D, buf, sizeof(buf));
    CHECK(strcmp(buf, "FARFETCH'D") == 0);
    pkb_species_name(NIDORAN_F, buf, sizeof(buf));
    CHECK(strcmp(buf, "NIDORANF") == 0);
    pkb_species_name(PORYGON2, buf, sizeof(buf));
    CHECK(strcmp(buf, "PORYGON2") == 0);
    pkb_species_name(CELEBI, buf, sizeof(buf));
    CHECK(strcmp(buf, "CELEBI") == 0);
    pkb_species_name(0, buf, sizeof(buf));
    CHECK(buf[0] == '\0');
}

static void test_species_names_never_contain_no_data_byte(void) {
    /* '8' encodes to $FE (SERIAL_NO_DATA_BYTE), which the receiving GB drops
     * outside the patched region: nicknames must never contain it. */
    for(int s = 1; s <= NUM_POKEMON; s++) {
        const uint8_t* gb;
        size_t len = pkb_species_name_gb((uint8_t)s, &gb);
        CHECK(len >= 1 && len <= 10);
        for(size_t i = 0; i < len; i++)
            CHECK(gb[i] != 0xFE && gb[i] != 0xFD && gb[i] != 0x50);
    }
}

static void test_move_names(void) {
    char buf[16];
    pkb_move_name(POUND, buf, sizeof(buf));
    CHECK(strcmp(buf, "POUND") == 0);
    pkb_move_name(THUNDERPUNCH, buf, sizeof(buf));
    CHECK(strcmp(buf, "THUNDERPUNCH") == 0);
    pkb_move_name(BEAT_UP, buf, sizeof(buf));
    CHECK(strcmp(buf, "BEAT UP") == 0);
}

static void test_decode_ligature(void) {
    static const uint8_t gb[] = {0x8B, 0xD4, 0x50}; /* L 's */
    char buf[8];
    pkb_gb_decode(gb, sizeof(gb), buf, sizeof(buf));
    CHECK(strcmp(buf, "L's") == 0);
}

int main(void) {
    RUN(test_encode_trainer_name);
    RUN(test_encode_truncates_and_terminates);
    RUN(test_species_names);
    RUN(test_species_names_never_contain_no_data_byte);
    RUN(test_move_names);
    RUN(test_decode_ligature);
    return TEST_REPORT();
}
