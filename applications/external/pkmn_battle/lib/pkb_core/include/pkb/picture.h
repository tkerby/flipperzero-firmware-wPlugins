#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Menu value that forces the normal link picture, even for a boss. */
#define PKB_PICTURE_LINK 0xFFu
/* Crystal trainer class of the normal Colosseum picture (CalPic). */
#define PKB_TRAINER_CLASS_CAL 0x0Cu
/* MYSTICALMAN, the last entry in TrainerPicPointers. */
#define PKB_TRAINER_CLASS_MAX 0x43u

/*
 * Install the OT player-name script that sets wOtherTrainerClass to `trainer_class`
 * (and wOtherTrainerID to 1) on the participant screen, before GetTrainerPic.
 * The next print of that name, after IsGymLeader and before DoBattle, writes the
 * party happiness bytes back. Nicknames are left alone.
 *
 * The visible trainer name is blank: reprinting it does not fit in the 66-byte
 * OT-name cave once the happiness copy is in there. Item restore does not fit
 * either; those bytes are outside the battle CRC.
 *
 * hUnusedByte counts the visits inside one battle. A later link writes CAL
 * again, and that class is what arms the next snapshot, so a flag left at 2
 * does not skip the rematch. Returns false if `trainer_class` is not 1..MAX
 * or the script would put a $FE in the party block.
 */
bool pkb_picture_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class);

/* Picture cave that also calls PlayMusic with `song` on the first visit.
 * `song` must be a real song id (not 0, not $FE). Nicknames are not modified.
 * The name still ends in @. This cave pops that address on every exit. */
bool pkb_picture_apply_song(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class, uint8_t song);

/* class 0 keeps Cal. song 0 or the Johto trainer theme does not call PlayMusic.
 * When both are set, one cave does both jobs. Nicknames are never written. */
bool pkb_link_overlay_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t trainer_class, uint8_t song);

/* Menu rows: 0 Default, 1 Link, then one row per trainer class. */
uint8_t pkb_picture_choice_count(void);
uint8_t pkb_picture_choice_id(uint8_t index);
const char* pkb_picture_choice_label(uint8_t index);
const char* pkb_picture_setting_label(uint8_t stored);
bool pkb_picture_setting_ok(uint8_t stored);
/* True for a class a random battle may show. Gym leaders, the Elite Four,
 * Lance, both rivals, and Red are left out. */
bool pkb_picture_generic_class(uint8_t trainer_class);
/* One class from that pool. rng is only used for a random battle's Default. */
uint8_t pkb_picture_random_class(PkbPolicyRng* rng);
/* 0 and Link install nothing (the Game Boy keeps Cal). Default on a boss
 * uses that boss's class. Default on a random battle draws a generic class.
 * Any other stored value is the class id. */
uint8_t pkb_picture_resolve(uint8_t choice, bool boss, uint8_t boss_index, PkbPolicyRng* rng);

#ifdef __cplusplus
}
#endif
