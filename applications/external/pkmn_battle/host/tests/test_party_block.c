#include "pkb_test.h"
#include <pkb/charset.h>
#include <pkb/music_test.h>
#include <pkb/picture.h>
#include <pkb/party_block.h>
#include <pkb/policy_rng.h>
#include "pb_const.h"
#include "pb_trainers.h"

static void make_team(PkbTeam* t) {
    static const uint8_t species[6] = {PIKACHU, BLISSEY, SHUCKLE, MEWTWO, CELEBI, PORYGON2};
    static const uint8_t moves[6][4] = {
        {THUNDERBOLT, QUICK_ATTACK, THUNDER_WAVE, SURF},
        {SOFTBOILED, EGG_BOMB, TOXIC, ICE_BEAM},
        {WRAP, ENCORE, TOXIC, REST},
        {PSYCHIC_M, RECOVER, AMNESIA, ICE_BEAM},
        {GIGA_DRAIN, RECOVER, HEAL_BELL, PSYCHIC_M},
        {TRI_ATTACK, RECOVER, CONVERSION2, THUNDERBOLT},
    };
    t->count = 6;
    t->trainer_id = 0xFE01; /* forces a patched byte in the region */
    for(int i = 0; i < 6; i++)
        pkb_team_mon_init(&t->mon[i], species[i], 50, moves[i]);
    /* a boss-style mon: own level, item, DVs, no stat exp */
    t->mon[1].level = 63;
    t->mon[1].item = LEFTOVERS;
    t->mon[1].dvs[0] = 0x9A;
    t->mon[1].dvs[1] = 0x77;
    t->mon[1].stat_exp = 0;
    t->mon[1].happiness = 70;
    pkb_team_mon_calc(&t->mon[1]);
}

static uint16_t be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void test_block_layout(void) {
    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    make_team(&t);
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));

    for(int i = 0; i < 6; i++)
        CHECK_EQ(block[i], 0xFD);
    CHECK_EQ(block[6], 0x85); /* 'F' */
    CHECK_EQ(block[6 + 7], 0x50); /* name terminator */
    CHECK_EQ(block[17], 6); /* party count */
    CHECK_EQ(block[18], PIKACHU);
    CHECK_EQ(block[23], PORYGON2);
    CHECK_EQ(block[24], 0xFF); /* species terminator */
    CHECK_EQ(block[25], 0xFF); /* trainer ID high byte $FE -> $FF (patched) */
    CHECK_EQ(block[26], 0x01);

    const uint8_t* mon0 = &block[27];
    CHECK_EQ(mon0[MON_SPECIES], PIKACHU);
    CHECK_EQ(mon0[MON_ITEM], 0);
    CHECK_EQ(mon0[MON_MOVES], THUNDERBOLT);
    CHECK_EQ(mon0[MON_OT_ID], 0xFF); /* patched */
    CHECK_EQ(mon0[MON_OT_ID + 1], 0x01);
    CHECK_EQ((mon0[MON_EXP] << 16) | (mon0[MON_EXP + 1] << 8) | mon0[MON_EXP + 2], 125000);
    CHECK_EQ(be16(&mon0[MON_HP_EXP]), 0xFFFF);
    CHECK_EQ(be16(&mon0[MON_SPC_EXP]), 0xFFFF);
    CHECK_EQ(be16(&mon0[MON_DVS]), 0xFFFF);
    CHECK_EQ(mon0[MON_PP], 15);
    CHECK_EQ(mon0[MON_HAPPINESS], 255);
    CHECK_EQ(mon0[MON_LEVEL], 50);
    CHECK_EQ(mon0[MON_STATUS], 0);
    CHECK_EQ(be16(&mon0[MON_HP]), 141);
    CHECK_EQ(be16(&mon0[MON_MAXHP]), 141);
    CHECK_EQ(be16(&mon0[MON_SPD]), 141);

    /* Blissey L63, Leftovers, DVs 9/10/7/7 (HP DV 11), no stat exp:
     * HP = (255 + 11) * 2 * 63 / 100 + 73, exp = 4/5 * 63^3 (fast) */
    const uint8_t* mon1 = &block[27 + 48];
    CHECK_EQ(mon1[MON_ITEM], LEFTOVERS);
    CHECK_EQ(mon1[MON_LEVEL], 63);
    CHECK_EQ(be16(&mon1[MON_DVS]), 0x9A77);
    CHECK_EQ(be16(&mon1[MON_HP_EXP]), 0);
    CHECK_EQ(mon1[MON_HAPPINESS], 70);
    CHECK_EQ(be16(&mon1[MON_MAXHP]), 408);
    CHECK_EQ((mon1[MON_EXP] << 16) | (mon1[MON_EXP + 1] << 8) | mon1[MON_EXP + 2], 200037);

    const uint8_t* ots = &block[27 + 6 * 48];
    CHECK_EQ(ots[0], 0x85);
    CHECK_EQ(ots[11 * 5], 0x85);
    const uint8_t* nicks = ots + 66;
    CHECK_EQ(nicks[0], 0x8F); /* 'P'IKACHU */
    CHECK_EQ(nicks[7], 0x50);
    CHECK_EQ(nicks[10], 0x50);
    CHECK_EQ(block[447], 0);
    CHECK_EQ(block[449], 0);

    /* Each $FE in ID/OT IDs became $FF and is listed (ID byte 0 = index 1,
     * mon n OT ID byte 0 = index 3 + 6 + 48n) */
    CHECK_EQ(patch[10], 1);
    CHECK_EQ(patch[11], 3 + MON_OT_ID);
}

static void test_parse_round_trip(void) {
    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    make_team(&t);
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));

    /* What the other GB receives: leading no-data bytes, our block, then
     * the patch list after its own leading junk. */
    uint8_t rx[8 + PKB_PARTY_BLOCK_LEN];
    memset(rx, 0xFE, 8);
    memcpy(rx + 8, block, sizeof(block));
    uint8_t patch_rx[4 + PKB_PATCH_BLOCK_LEN];
    memset(patch_rx, 0xFE, 4);
    memcpy(patch_rx + 4, patch, sizeof(patch));

    PkbLinkPartyData d;
    CHECK(pkb_party_block_parse(rx, sizeof(rx), patch_rx, sizeof(patch_rx), &d));
    CHECK_EQ(d.count, 6);
    CHECK_EQ(d.player_id[0], 0xFE); /* restored */
    CHECK_EQ(d.player_id[1], 0x01);
    for(int i = 0; i < 6; i++) {
        uint8_t want[48];
        pkb_party_struct_build(&t.mon[i], t.trainer_id, want);
        CHECK_MEM_EQ(d.mons[i], want, 48);
        CHECK_EQ(d.species[i], t.mon[i].species);
    }
    CHECK_EQ(d.species[6], 0xFF);
}

static void test_short_team(void) {
    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    make_team(&t);
    t.count = 1;
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK_EQ(block[17], 1);
    CHECK_EQ(block[18], PIKACHU);
    CHECK_EQ(block[19], 0xFF);
    CHECK_EQ(block[27 + 48], 0); /* empty second slot */
}

static uint8_t* ots_of(uint8_t* block) {
    return block + 27 + 6 * PKB_PARTY_STRUCT_LEN;
}

static uint8_t* nicks_of(uint8_t* block) {
    return ots_of(block) + 6 * PKB_NAME_LEN;
}

static bool region_has_fe(const uint8_t* p, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(p[i] == PKB_SERIAL_NO_DATA_BYTE) return true;
    }
    return false;
}

static void round_trip_names(const uint8_t* block, const uint8_t* patch) {
    uint8_t rx[8 + PKB_PARTY_BLOCK_LEN];
    uint8_t patch_rx[4 + PKB_PATCH_BLOCK_LEN];
    PkbLinkPartyData d;
    memset(rx, 0xFE, 8);
    memcpy(rx + 8, block, PKB_PARTY_BLOCK_LEN);
    memset(patch_rx, 0xFE, 4);
    memcpy(patch_rx + 4, patch, PKB_PATCH_BLOCK_LEN);
    CHECK(pkb_party_block_parse(rx, sizeof(rx), patch_rx, sizeof(patch_rx), &d));
    CHECK_MEM_EQ(d.ot_names, ots_of((uint8_t*)block), 6 * PKB_NAME_LEN);
    CHECK_MEM_EQ(d.nicknames, nicks_of((uint8_t*)block), 6 * PKB_NAME_LEN);
}

static void test_music_overlay(void) {
    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], plain[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    static const uint8_t boot[PKB_NAME_LEN] = {
        0x4F,
        0x15,
        0x08,
        0x05,
        0xC9,
        0x00,
        0xCD,
        0xA8,
        0xD3,
        0x50,
        0x50,
    };
    make_team(&t);
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    memcpy(plain, block, sizeof(block));
    CHECK(pkb_music_test_apply(block, PKB_MUSIC_OFF));
    CHECK_MEM_EQ(block, plain, sizeof(block));
    CHECK(!pkb_music_test_apply(block, 99));

    CHECK(pkb_music_test_apply(block, PKB_MUSIC_TRACE));
    uint8_t mark[PKB_NAME_LEN];
    pkb_gb_encode("MUSICTEST", mark, sizeof(mark));
    CHECK_MEM_EQ(nicks_of(block), mark, PKB_NAME_LEN);
    CHECK_MEM_EQ(nicks_of(block) + PKB_NAME_LEN, nicks_of(plain) + PKB_NAME_LEN, 5 * PKB_NAME_LEN);
    CHECK_MEM_EQ(ots_of(block), ots_of(plain), 6 * PKB_NAME_LEN);
    CHECK(!region_has_fe(nicks_of(block), 6 * PKB_NAME_LEN));
    round_trip_names(block, patch);

    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(pkb_music_test_apply(block, PKB_MUSIC_RETURN));
    CHECK_MEM_EQ(block + 27, plain + 27, 6 * PKB_PARTY_STRUCT_LEN); /* party structs stay put */
    CHECK_MEM_EQ(block + PKB_PARTY_PREAMBLE_LEN, boot, PKB_NAME_LEN);
    CHECK_MEM_EQ(nicks_of(block), nicks_of(plain), 6 * PKB_NAME_LEN);
    CHECK_EQ(ots_of(block)[0], 0xD1);
    CHECK_EQ(ots_of(block)[6 * PKB_NAME_LEN - 1], 0); /* flag starts clear */
    CHECK(!region_has_fe(ots_of(block), 6 * PKB_NAME_LEN));
    CHECK(!region_has_fe(nicks_of(block), 6 * PKB_NAME_LEN));
    round_trip_names(block, patch);

    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(pkb_music_test_apply(block, PKB_MUSIC_CHAMPION));
    CHECK_MEM_EQ(block + PKB_PARTY_PREAMBLE_LEN, boot, PKB_NAME_LEN);
    CHECK_MEM_EQ(nicks_of(block), nicks_of(plain), 6 * PKB_NAME_LEN);
    CHECK_EQ(ots_of(block)[17], PKB_SONG_CHAMPION);
    CHECK_EQ(ots_of(block)[20], PKB_PLAY_MUSIC_ADDR & 0xFF);
    CHECK_EQ(ots_of(block)[21], PKB_PLAY_MUSIC_ADDR >> 8);
    CHECK(!region_has_fe(ots_of(block), 6 * PKB_NAME_LEN));
    round_trip_names(block, patch);
}

static uint8_t boss_named(const char* name) {
    for(uint8_t i = 0; i < pkb_boss_count(); i++) {
        if(strcmp(pkb_boss_label(i), name) == 0) return i;
    }
    return 0xFF;
}

static void test_music_choices(void) {
    CHECK(pkb_music_setting_ok(0));
    CHECK(pkb_music_setting_ok(PKB_MUSIC_CHAMPION));
    CHECK(pkb_music_setting_ok(PKB_SONG_ROCKET));
    CHECK(!pkb_music_setting_ok(PKB_MUSIC_TRACE));
    CHECK_EQ(strcmp(pkb_music_setting_label(0), "Default"), 0);
    CHECK_EQ(strcmp(pkb_music_setting_label(PKB_SONG_CHAMPION), "Champion"), 0);

    CHECK_EQ(pkb_music_for_boss(boss_named("Falkner")), PKB_SONG_JOHTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Clair")), PKB_SONG_JOHTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Brock")), PKB_SONG_KANTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Blue")), PKB_SONG_KANTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Will")), PKB_SONG_JOHTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Karen")), PKB_SONG_JOHTO_GYM);
    CHECK_EQ(pkb_music_for_boss(boss_named("Lance")), PKB_SONG_CHAMPION);
    CHECK_EQ(pkb_music_for_boss(boss_named("Red")), PKB_SONG_CHAMPION);
    CHECK_EQ(pkb_music_for_boss(boss_named("Meganium")), PKB_SONG_RIVAL);
    CHECK_EQ(pkb_music_for_boss(boss_named("Typhlosion")), PKB_SONG_RIVAL);
    CHECK_EQ(pkb_music_for_boss(boss_named("Feraligatr")), PKB_SONG_RIVAL);
    CHECK_EQ(pkb_music_for_boss(boss_named("Prof. Oak")), PKB_SONG_SUICUNE);
    CHECK_EQ(pkb_music_for_class(0x0A), PKB_SONG_SUICUNE);

    uint8_t red = boss_named("Red");
    CHECK_EQ(pkb_music_resolve(0, false, 0), PKB_SONG_JOHTO_TRAINER);
    CHECK_EQ(pkb_music_resolve(0, true, red), PKB_SONG_CHAMPION);
    CHECK_EQ(pkb_music_resolve(PKB_SONG_ROCKET, true, red), PKB_SONG_ROCKET);
    CHECK_EQ(pkb_music_resolve(PKB_MUSIC_CHAMPION, false, 0), PKB_SONG_CHAMPION);
    CHECK_EQ(pkb_music_for_class(0x1F), PKB_SONG_ROCKET); /* GRUNTM */
    CHECK_EQ(pkb_music_for_class(0x42), PKB_SONG_ROCKET); /* GRUNTF */
    CHECK_EQ(pkb_music_for_class(0x33), PKB_SONG_ROCKET); /* EXECUTIVEM */
    CHECK_EQ(pkb_music_for_class(0x16), PKB_SONG_JOHTO_TRAINER); /* Youngster */
    CHECK_EQ(pkb_music_for_class(0x01), PKB_SONG_JOHTO_GYM);
    CHECK_EQ(pkb_music_for_class(0x11), PKB_SONG_KANTO_GYM);
    CHECK_EQ(pkb_music_for_class(0x3F), PKB_SONG_CHAMPION);
    CHECK_EQ(pkb_music_for_class(0x2A), PKB_SONG_RIVAL);

    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    make_team(&t);
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(!pkb_music_apply_song(block, 0));
    CHECK(!pkb_music_apply_song(block, 0xFE));
    CHECK(pkb_music_apply_song(block, PKB_SONG_ROCKET));
    CHECK_EQ(ots_of(block)[17], PKB_SONG_ROCKET);
    CHECK(!region_has_fe(ots_of(block), 6 * PKB_NAME_LEN));
    round_trip_names(block, patch);
}

static void test_picture_apply(void) {
    PkbTeam t;
    uint8_t block[PKB_PARTY_BLOCK_LEN], plain[PKB_PARTY_BLOCK_LEN], patch[PKB_PATCH_BLOCK_LEN];
    static const uint8_t name[PKB_NAME_LEN] = {
        0x4F,
        0x15,
        0x08,
        0x05,
        0xC9,
        0x00,
        0xCD,
        0xA8,
        0xD3,
        0x50,
        0x50,
    };

    make_team(&t);
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    memcpy(plain, block, sizeof(block));
    CHECK(!pkb_picture_apply(block, 0));
    CHECK(!pkb_picture_apply(block, PKB_TRAINER_CLASS_MAX + 1));
    CHECK(!pkb_picture_apply(block, 0xFE));
    CHECK_MEM_EQ(block, plain, sizeof(block));

    CHECK(pkb_picture_apply(block, 0x01));
    CHECK_MEM_EQ(block + PKB_PARTY_PREAMBLE_LEN, name, PKB_NAME_LEN);
    CHECK_EQ(ots_of(block)[0], 0xD1);
    CHECK_EQ(ots_of(block)[17], 0x01);
    CHECK_EQ(ots_of(block)[65], 0x00);
    CHECK_MEM_EQ(nicks_of(block), nicks_of(plain), 6 * PKB_NAME_LEN);
    CHECK_MEM_EQ(block + 27, plain + 27, 6 * PKB_PARTY_STRUCT_LEN);
    CHECK(!region_has_fe(block + PKB_PARTY_PREAMBLE_LEN, PKB_NAME_LEN));
    CHECK(!region_has_fe(ots_of(block), 6 * PKB_NAME_LEN));
    round_trip_names(block, patch);

    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(pkb_picture_apply(block, 0x3F));
    CHECK_EQ(ots_of(block)[17], 0x3F);
    CHECK(!region_has_fe(ots_of(block), 6 * PKB_NAME_LEN));

    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(pkb_picture_apply_song(block, 0x01, PKB_SONG_ROCKET));
    CHECK_MEM_EQ(block + PKB_PARTY_PREAMBLE_LEN, name, PKB_NAME_LEN);
    CHECK_EQ(ots_of(block)[13], 0x01);
    CHECK_EQ(ots_of(block)[18], PKB_SONG_ROCKET);
    CHECK_MEM_EQ(nicks_of(block), nicks_of(plain), 6 * PKB_NAME_LEN);
    CHECK(!pkb_picture_apply_song(block, 0x01, 0));
    CHECK(!pkb_picture_apply_song(block, 0x01, 0xFE));
    CHECK(pkb_party_block_build(&t, "FLIPPER", block, patch));
    CHECK(pkb_link_overlay_apply(block, 0, PKB_SONG_JOHTO_TRAINER));
    CHECK_MEM_EQ(block, plain, sizeof(block));
    CHECK(pkb_link_overlay_apply(block, 0, PKB_SONG_ROCKET));
    CHECK_MEM_EQ(nicks_of(block), nicks_of(plain), 6 * PKB_NAME_LEN);
    CHECK_EQ(ots_of(block)[17], PKB_SONG_ROCKET);

    CHECK(!pkb_picture_generic_class(0x01));
    CHECK(!pkb_picture_generic_class(0x09));
    CHECK(!pkb_picture_generic_class(0x0B));
    CHECK(!pkb_picture_generic_class(0x10));
    CHECK(!pkb_picture_generic_class(0x2A));
    CHECK(!pkb_picture_generic_class(0x3F));
    CHECK(!pkb_picture_generic_class(0x40));
    CHECK(pkb_picture_generic_class(0x0C)); /* Cal stays available */
    CHECK(pkb_picture_generic_class(0x1F)); /* GRUNTM */
    CHECK(pkb_picture_generic_class(0x16));
    PkbPolicyRng rng;
    pkb_policy_rng_seed(&rng, 1);
    for(int i = 0; i < 80; i++) {
        uint8_t c = pkb_picture_resolve(0, false, 0, &rng);
        CHECK(pkb_picture_generic_class(c));
    }
    CHECK_EQ(pkb_picture_resolve(PKB_PICTURE_LINK, true, 0, &rng), 0);
    CHECK_EQ(pkb_picture_resolve(0, true, 0, &rng), pb_bosses[0].trainer_class);
    CHECK_EQ(pkb_picture_resolve(0x05, false, 0, &rng), 0x05);
    CHECK(pkb_picture_setting_ok(0));
    CHECK(pkb_picture_setting_ok(PKB_PICTURE_LINK));
    CHECK(pkb_picture_setting_ok(0x01));
    CHECK(!pkb_picture_setting_ok(0xFE));
    CHECK_EQ(strcmp(pb_trainer_class_label[0], "Falkner"), 0);
    CHECK_EQ(strcmp(pb_trainer_class_label[0x2A - 1], "Silver"), 0);
    CHECK_EQ(strcmp(pb_trainer_class_label[0x10 - 1], "Lance"), 0);
    CHECK_EQ(strcmp(pb_trainer_class_label[0x3F - 1], "Red"), 0);
    CHECK_EQ(strcmp(pkb_picture_choice_label(0), "Default"), 0);
    CHECK_EQ(strcmp(pkb_picture_choice_label(1), "Link"), 0);
    CHECK_EQ(strcmp(pkb_picture_setting_label(0x01), "Falkner"), 0);
}

static void test_rn_block(void) {
    uint8_t rns[10] = {0, 1, 0x7F, 0xFC, 0xFD, 0xFE, 0xFF, 9, 8, 7};
    uint8_t out[PKB_RN_BLOCK_LEN];
    pkb_rn_block_build(rns, out);
    for(int i = 0; i < 7; i++)
        CHECK_EQ(out[i], 0xFD);
    for(int i = 7; i < 17; i++)
        CHECK(out[i] < 0xFD);

    /* Slave side copies the master's numbers, skipping $FD/$FE anywhere */
    static const uint8_t rx[] = {0xFE, 0xFE, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0xFD, 0,
                                 0x10, 0xFE, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90};
    uint8_t got[10];
    static const uint8_t want[10] = {0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90};
    CHECK(pkb_rn_block_parse(rx, sizeof(rx), got));
    CHECK_MEM_EQ(got, want, 10);
    CHECK(!pkb_rn_block_parse(rx, 12, got));
}

int main(void) {
    RUN(test_block_layout);
    RUN(test_parse_round_trip);
    RUN(test_short_team);
    RUN(test_music_overlay);
    RUN(test_music_choices);
    RUN(test_picture_apply);
    RUN(test_rn_block);
    return TEST_REPORT();
}
