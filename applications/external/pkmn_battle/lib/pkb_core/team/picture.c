/* Colosseum trainer picture.
 *
 * The player name is a mobile-script prefix that calls this cave at
 * wOTPartyMonOTs ($D3A8). A new link writes CAL into wOtherTrainerClass
 * before the participant screen, so that visit snapshots the six happiness
 * bytes to wcd70, stores the chosen class, and sets wOtherTrainerID to 1.
 * hUnusedByte remembers the visit: 1 means the next print (after IsGymLeader,
 * before DoBattle) writes the snapshot back, and 2 means later prints only
 * finish the text. The id stays 1 so GetTrainerName does not walk the table.
 *
 * The player name ends with @. The picture cave saves that address and puts
 * it back in de. The song cave leaves it on the stack until every exit pops
 * it, so PlaceNextChar stops after PlayMusic too.
 */
#include <pkb/picture.h>

#include <pkb/music_test.h>
#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

#include "pb_trainers.h"

#include <stddef.h>
#include <string.h>

/* call wOTPartyMonOTs. The two $50 bytes are the text terminator the cave
 * pops into de. */
static const uint8_t k_name[PKB_NAME_LEN] = {
    0x4F, 0x15, 0x08, 0x05, 0xC9, 0x00, 0xCD, 0xA8, 0xD3, 0x50, 0x50,
};

/* Class immediate in k_cave. A CAL class means a new battle, so hUnusedByte
 * left at 2 by the previous battle does not skip the snapshot. */
#define CLASS_AT 17
/* Class and song immediates in k_cave_song. */
#define SONG_CLASS_AT 13
#define SONG_AT 18

static const uint8_t k_cave[PKB_NAME_LEN * PKB_PARTY_LENGTH] = {
    0xD1, 0xD5, 0xFA, 0x2F, 0xD2, 0xD6, 0x0C, 0x20, 0x2B, 0x4F, 0x3C, 0xEA, 0x31, 0xD2, 0xE0, 0xA1,
    0x3E, 0x01, 0xEA, 0x2F, 0xD2, 0x21, 0xFA, 0xDC, 0x11, 0x70, 0xCD, 0x06, 0x06, 0xCB, 0x41, 0x20,
    0x04, 0x7E, 0x12, 0x18, 0x02, 0x1A, 0x77, 0x13, 0xC5, 0x01, 0x30, 0x00, 0x09, 0xC1, 0x05, 0x20,
    0xEC, 0xD1, 0x37, 0xC9, 0xF0, 0xA1, 0x3D, 0x20, 0xF8, 0x3C, 0x4F, 0x3C, 0xE0, 0xA1, 0x18, 0xD5,
    0x00, 0x00,
};

/* Same jobs plus `ld e, song; call PlayMusic` on the first visit. d is still 0
 * there. Every exit pops the call return, which is the name's @. c is set here:
 * 0 snapshots, and the next visit (hUnusedByte still 0) sets c to 1 and restores.
 * The overworld menu clears that flag before the next link. */
static const uint8_t k_cave_song[PKB_NAME_LEN * PKB_PARTY_LENGTH] = {
    0xFA, 0x2F, 0xD2, 0xD6, 0x0C, 0x20, 0x2E, 0x4F, 0x3C, 0xEA, 0x31, 0xD2, 0x3E, 0x01, 0xEA, 0x2F,
    0xD2, 0x1E, 0x2F, 0xCD, 0x97, 0x3B, 0x21, 0xFA, 0xDC, 0x11, 0x70, 0xCD, 0x06, 0x06, 0xCB, 0x41,
    0x20, 0x04, 0x7E, 0x12, 0x18, 0x02, 0x1A, 0x77, 0x13, 0xC5, 0x01, 0x30, 0x00, 0x09, 0xC1, 0x05,
    0x20, 0xEC, 0xD1, 0x37, 0xC9, 0xF0, 0xA1, 0xB7, 0x20, 0xF8, 0x3C, 0x4F, 0x3C, 0xE0, 0xA1, 0x18,
    0xD5, 0x00,
};

static bool has_fe(const uint8_t* p, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(p[i] == PKB_SERIAL_NO_DATA_BYTE) return true;
    }
    return false;
}

static bool install_cave(uint8_t block[PKB_PARTY_BLOCK_LEN], const uint8_t* name, const uint8_t* cave, int class_at,
                         uint8_t trainer_class, int song_at, uint8_t song) {
    if(trainer_class < 1 || trainer_class > PKB_TRAINER_CLASS_MAX) return false;
    if(song_at >= 0 && (song == 0 || song == PKB_SERIAL_NO_DATA_BYTE)) return false;
    uint8_t buf[PKB_NAME_LEN * PKB_PARTY_LENGTH];
    memcpy(buf, cave, sizeof(buf));
    buf[class_at] = trainer_class;
    if(song_at >= 0) buf[song_at] = song;
    if(has_fe(name, PKB_NAME_LEN) || has_fe(buf, sizeof(buf))) return false;
    if(name[6] != 0xCD || name[7] != 0xA8 || name[8] != 0xD3) return false;
    memcpy(block + PKB_PARTY_PREAMBLE_LEN, name, PKB_NAME_LEN);
    memcpy(block + PKB_PARTY_PREAMBLE_LEN + offsetof(PkbLinkPartyData, ot_names), buf, sizeof(buf));
    return true;
}

bool pkb_picture_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class) {
    if(k_cave[CLASS_AT] != 0x01 || k_name[9] != 0x50) return false;
    return install_cave(block, k_name, k_cave, CLASS_AT, trainer_class, -1, 0);
}

bool pkb_picture_apply_song(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class, uint8_t song) {
    if(k_cave_song[SONG_CLASS_AT] != 0x01 || k_cave_song[SONG_AT] != PKB_SONG_CHAMPION) return false;
    if(k_cave_song[SONG_AT - 1] != 0x1E || k_cave_song[SONG_AT + 1] != 0xCD || k_cave_song[SONG_AT + 2] != 0x97)
        return false;
    return install_cave(block, k_name, k_cave_song, SONG_CLASS_AT, trainer_class, SONG_AT, song);
}

uint8_t pkb_picture_choice_count(void) {
    return (uint8_t)(2 + PB_NUM_TRAINER_CLASSES);
}

uint8_t pkb_picture_choice_id(uint8_t index) {
    if(index == 0) return 0;
    if(index == 1) return PKB_PICTURE_LINK;
    if(index >= 2 && index < pkb_picture_choice_count()) return (uint8_t)(index - 1);
    return 0;
}

const char* pkb_picture_choice_label(uint8_t index) {
    if(index == 0) return "Default";
    if(index == 1) return "Link";
    if(index >= 2 && (unsigned)(index - 2) < PB_NUM_TRAINER_CLASSES) return pb_trainer_class_label[index - 2];
    return "Default";
}

const char* pkb_picture_setting_label(uint8_t stored) {
    if(stored == PKB_PICTURE_LINK) return "Link";
    if(stored >= 1 && stored <= PB_NUM_TRAINER_CLASSES) return pb_trainer_class_label[stored - 1];
    return "Default";
}

bool pkb_picture_setting_ok(uint8_t stored) {
    return stored == 0 || stored == PKB_PICTURE_LINK || (stored >= 1 && stored <= PB_NUM_TRAINER_CLASSES);
}

bool pkb_picture_generic_class(uint8_t trainer_class) {
    if(trainer_class < 1 || trainer_class > PKB_TRAINER_CLASS_MAX) return false;
    switch(trainer_class) {
    case 0x01: /* Falkner */
    case 0x02: /* Whitney */
    case 0x03: /* Bugsy */
    case 0x04: /* Morty */
    case 0x05: /* Pryce */
    case 0x06: /* Jasmine */
    case 0x07: /* Chuck */
    case 0x08: /* Clair */
    case 0x09: /* RIVAL1 */
    case 0x0B: /* Will */
    case 0x0D: /* Bruno */
    case 0x0E: /* Karen */
    case 0x0F: /* Koga */
    case 0x10: /* Lance */
    case 0x11: /* Brock */
    case 0x12: /* Misty */
    case 0x13: /* Lt. Surge */
    case 0x15: /* Erika */
    case 0x1A: /* Janine */
    case 0x23: /* Sabrina */
    case 0x2A: /* RIVAL2 */
    case 0x2E: /* Blaine */
    case 0x3F: /* Red */
    case 0x40: /* Blue */
        return false;
    default:
        return true;
    }
}

uint8_t pkb_picture_random_class(PkbPolicyRng* rng) {
    uint8_t pool[PKB_TRAINER_CLASS_MAX];
    uint8_t n = 0;
    for(uint8_t c = 1; c <= PKB_TRAINER_CLASS_MAX; c++) {
        if(pkb_picture_generic_class(c)) pool[n++] = c;
    }
    if(!n || !rng) return 0;
    return pool[pkb_policy_rng_below(rng, n)];
}

uint8_t pkb_picture_resolve(uint8_t choice, bool boss, uint8_t boss_index, PkbPolicyRng* rng) {
    if(choice == PKB_PICTURE_LINK) return 0;
    if(choice == 0) {
        if(boss && boss_index < PB_NUM_BOSSES) return pb_bosses[boss_index].trainer_class;
        return pkb_picture_random_class(rng);
    }
    if(choice <= PB_NUM_TRAINER_CLASSES) return choice;
    return 0;
}

bool pkb_link_overlay_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class, uint8_t song) {
    bool play = song != 0 && song != PKB_SONG_JOHTO_TRAINER;
    if(trainer_class && play) return pkb_picture_apply_song(block, trainer_class, song);
    if(trainer_class) return pkb_picture_apply(block, trainer_class);
    if(play) return pkb_music_apply_song(block, song);
    return true;
}
