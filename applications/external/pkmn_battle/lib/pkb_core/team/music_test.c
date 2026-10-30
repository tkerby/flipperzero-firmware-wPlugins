/* Colosseum music. Crystal prints the OT player name with PlaceString
 * on the participant screen, which is early enough to share the cave
 * with the trainer picture. Nicknames stay the species names.
 * The bytes below are the English-Crystal mobile-script prefix
 * (home/text.asm, mobile/mobile_5f.asm Function17f2cb):
 *
 *   4F          line: cursor becomes tilemap $C5E1
 *   15 08 05 C9 mobile function 8 prints 5 digits and leaves
 *               E6 C5 05 C9 at wcd52, which is `and $C5 / dec b / ret`
 *   00          the missed index: _RunMobileScript jumps to wcd52
 *
 * That ret continues at the next byte. `call wOTPartyMonOTs` ($D3A8) pushes
 * the address of the following $50. The cave pops it into de and decrements,
 * so PlaceNextChar's inc de lands on the terminator. The first visit stores
 * a flag in the last OT-name byte and calls PlayMusic. Later visits only
 * return, so the song is not restarted when the HUD prints the name again.
 *
 * Nicknames and OT names are outside the patch list, so none of these bytes
 * may be $FE.
 */
#include <pkb/music_test.h>

#include <pkb/charset.h>
#include <pkb/team.h>

#include <stddef.h>
#include <string.h>

/* call wOTPartyMonOTs; two terminators so a one-byte slip still stops. */
static const uint8_t k_nick[PKB_NAME_LEN] = {
    0x4F, 0x15, 0x08, 0x05, 0xC9, 0x00, 0xCD, 0xA8, 0xD3, 0x50, 0x50,
};

/* pop de / dec de / ld a,[flag] / or a / jr nz,.done / ld a,1 / ld [flag],a / scf / ret */
static const uint8_t k_return[] = {
    0xD1, 0x1D, 0xFA, 0xE9, 0xD3, 0xB7, 0x20, 0x05, 0x3E, 0x01, 0xEA, 0xE9, 0xD3, 0x37, 0xC9,
};

/* Same, plus push bc/de/hl, ld de,song, call PlayMusic, pop. Song is byte 17. */
static const uint8_t k_play[] = {
    0xD1, 0x1D, 0xFA, 0xE9, 0xD3, 0xB7, 0x20, 0x11, 0x3E, 0x01, 0xEA, 0xE9, 0xD3, 0xC5, 0xD5, 0xE5,
    0x11, 0x2F, 0x00, 0xCD, 0x97, 0x3B, 0xE1, 0xD1, 0xC1, 0x37, 0xC9,
};

#define SONG_BYTE 17

static const struct {
    uint8_t id;
    const char* label;
} k_choices[] = {
    {0, "Default"},
    {PKB_SONG_CHAMPION, "Champion"},
    {PKB_SONG_JOHTO_GYM, "Johto Gym"},
    {PKB_SONG_KANTO_GYM, "Kanto Gym"},
    {PKB_SONG_RIVAL, "Rival"},
    {PKB_SONG_ROCKET, "Rocket"},
    {PKB_SONG_JOHTO_TRAINER, "Johto Trainer"},
    {PKB_SONG_KANTO_TRAINER, "Kanto Trainer"},
    {PKB_SONG_JOHTO_WILD, "Johto Wild"},
    {PKB_SONG_JOHTO_WILD_NIGHT, "Wild Night"},
    {PKB_SONG_KANTO_WILD, "Kanto Wild"},
    {PKB_SONG_SUICUNE, "Suicune"},
};

static const char* const k_kanto_gyms[] = {
    "Brock", "Misty", "Lt. Surge", "Erika", "Janine", "Sabrina", "Blaine", "Blue",
};

static uint8_t* at(uint8_t* block, size_t member) {
    return block + PKB_PARTY_PREAMBLE_LEN + member;
}

static bool has_fe(const uint8_t* p, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(p[i] == PKB_SERIAL_NO_DATA_BYTE) return true;
    }
    return false;
}

static bool named(uint8_t boss, const char* name) {
    const char* label = pkb_boss_label(boss);
    return label && strcmp(label, name) == 0;
}

static bool kanto_gym(uint8_t boss) {
    for(size_t i = 0; i < sizeof(k_kanto_gyms) / sizeof(k_kanto_gyms[0]); i++) {
        if(named(boss, k_kanto_gyms[i])) return true;
    }
    return false;
}

uint8_t pkb_music_choice_count(void) {
    return (uint8_t)(sizeof(k_choices) / sizeof(k_choices[0]));
}

uint8_t pkb_music_choice_id(uint8_t index) {
    return index < pkb_music_choice_count() ? k_choices[index].id : 0;
}

const char* pkb_music_choice_label(uint8_t index) {
    return index < pkb_music_choice_count() ? k_choices[index].label : "Default";
}

const char* pkb_music_setting_label(uint8_t stored) {
    if(stored == PKB_MUSIC_CHAMPION) return "Champion";
    for(uint8_t i = 0; i < pkb_music_choice_count(); i++) {
        if(k_choices[i].id == stored) return k_choices[i].label;
    }
    return "Default";
}

bool pkb_music_setting_ok(uint8_t stored) {
    if(stored == PKB_MUSIC_CHAMPION) return true;
    for(uint8_t i = 0; i < pkb_music_choice_count(); i++) {
        if(k_choices[i].id == stored) return true;
    }
    return false;
}

uint8_t pkb_music_for_boss(uint8_t boss) {
    switch(pkb_boss_category(boss)) {
    case PkbBossRed:
    case PkbBossChampion:
        return PKB_SONG_CHAMPION;
    case PkbBossOak:
        return PKB_SONG_SUICUNE;
    case PkbBossRival:
        return PKB_SONG_RIVAL;
    case PkbBossEliteFour:
        return PKB_SONG_JOHTO_GYM;
    case PkbBossGym:
        return kanto_gym(boss) ? PKB_SONG_KANTO_GYM : PKB_SONG_JOHTO_GYM;
    default:
        return PKB_SONG_JOHTO_TRAINER;
    }
}

uint8_t pkb_music_for_class(uint8_t trainer_class) {
    switch(trainer_class) {
    case 0x10: /* Lance */
    case 0x3F: /* Red */
        return PKB_SONG_CHAMPION;
    case 0x1F: /* GRUNTM */
    case 0x33: /* EXECUTIVEM */
    case 0x37: /* EXECUTIVEF */
    case 0x42: /* GRUNTF */
        return PKB_SONG_ROCKET;
    case 0x11: /* Brock */
    case 0x12: /* Misty */
    case 0x13: /* Lt. Surge */
    case 0x15: /* Erika */
    case 0x1A: /* Janine */
    case 0x23: /* Sabrina */
    case 0x2E: /* Blaine */
    case 0x40: /* Blue */
        return PKB_SONG_KANTO_GYM;
    case 0x01: /* Falkner */
    case 0x02: /* Whitney */
    case 0x03: /* Bugsy */
    case 0x04: /* Morty */
    case 0x05: /* Pryce */
    case 0x06: /* Jasmine */
    case 0x07: /* Chuck */
    case 0x08: /* Clair */
    case 0x0B: /* Will */
    case 0x0D: /* Bruno */
    case 0x0E: /* Karen */
    case 0x0F: /* Koga */
        return PKB_SONG_JOHTO_GYM;
    case 0x09: /* RIVAL1 */
    case 0x2A: /* RIVAL2 */
        return PKB_SONG_RIVAL;
    case 0x0A: /* POKEMON_PROF */
        return PKB_SONG_SUICUNE;
    default:
        return PKB_SONG_JOHTO_TRAINER;
    }
}

uint8_t pkb_music_resolve(uint8_t choice, bool boss, uint8_t boss_index) {
    if(choice == PKB_MUSIC_CHAMPION) return PKB_SONG_CHAMPION;
    if(choice == 0) return boss ? pkb_music_for_boss(boss_index) : PKB_SONG_JOHTO_TRAINER;
    return choice;
}

static void write_cave(uint8_t* ots, const uint8_t* cave, size_t n) {
    memset(ots, 0, PKB_NAME_LEN * PKB_PARTY_LENGTH);
    memcpy(ots, cave, n);
}

static bool install(uint8_t* block, const uint8_t* cave, size_t cave_n) {
    if(cave_n >= PKB_NAME_LEN * PKB_PARTY_LENGTH) return false;
    if(has_fe(k_nick, sizeof(k_nick)) || has_fe(cave, cave_n)) return false;
    /* The trigger is the OT player name, so species nicknames stay readable.
     * The picture cave uses the same two fields; see pkb_picture_apply_song
     * when a class and a song are both requested. */
    memcpy(block + PKB_PARTY_PREAMBLE_LEN, k_nick, PKB_NAME_LEN);
    write_cave(at(block, offsetof(PkbLinkPartyData, ot_names)), cave, cave_n);
    return true;
}

bool pkb_music_apply_song(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t song) {
    if(song == 0 || song == PKB_SERIAL_NO_DATA_BYTE || song == PKB_SERIAL_PREAMBLE_BYTE) return false;
    uint8_t cave[sizeof(k_play)];
    memcpy(cave, k_play, sizeof(cave));
    cave[SONG_BYTE] = song;
    if(k_play[SONG_BYTE + 1] != 0 || k_play[SONG_BYTE + 3] != (PKB_PLAY_MUSIC_ADDR & 0xFF) ||
       k_play[SONG_BYTE + 4] != (PKB_PLAY_MUSIC_ADDR >> 8))
        return false;
    return install(block, cave, sizeof(cave));
}

bool pkb_music_test_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], int mode) {
    if(mode == PKB_MUSIC_OFF) return true;
    if(mode == PKB_MUSIC_CHAMPION) return pkb_music_apply_song(block, PKB_SONG_CHAMPION);
    uint8_t* nicks = at(block, offsetof(PkbLinkPartyData, nicknames));
    if(mode == PKB_MUSIC_TRACE) {
        uint8_t mark[PKB_NAME_LEN];
        pkb_gb_encode("MUSICTEST", mark, sizeof(mark));
        if(has_fe(mark, sizeof(mark))) return false;
        memcpy(nicks, mark, PKB_NAME_LEN);
        return true;
    }
    if(mode != PKB_MUSIC_RETURN) return false;
    return install(block, k_return, sizeof(k_return));
}
