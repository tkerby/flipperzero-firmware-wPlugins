#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pkb/party_block.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Harness-only overlays. The device stores 0 (follow the opponent) or a
 * song id from the choice list. A settings value of CHAMPION is the old
 * "Music: Champion" toggle and is treated as PKB_SONG_CHAMPION. */
enum {
    PKB_MUSIC_OFF = 0,
    PKB_MUSIC_TRACE = 1,
    PKB_MUSIC_RETURN = 2,
    PKB_MUSIC_CHAMPION = 3,
};

/* home/audio.asm PlayMusic on Pokemon Crystal (UE) 1.0
 * (sha1 f4cd194bdee0d04ca4eac29e09b8e4e9d818c133). */
#define PKB_PLAY_MUSIC_ADDR 0x3B97u
/* constants/music_constants.asm. These are the songs PlayBattleMusic can start. */
#define PKB_SONG_KANTO_GYM 0x06u
#define PKB_SONG_KANTO_TRAINER 0x07u
#define PKB_SONG_KANTO_WILD 0x08u
#define PKB_SONG_JOHTO_WILD 0x29u
#define PKB_SONG_JOHTO_TRAINER 0x2Au
#define PKB_SONG_JOHTO_GYM 0x2Eu
#define PKB_SONG_CHAMPION 0x2Fu
#define PKB_SONG_RIVAL 0x30u
#define PKB_SONG_ROCKET 0x31u
#define PKB_SONG_JOHTO_WILD_NIGHT 0x4Au
#define PKB_SONG_SUICUNE 0x64u

/* Menu rows: index 0 is Default (id 0). */
uint8_t pkb_music_choice_count(void);
uint8_t pkb_music_choice_id(uint8_t index);
const char* pkb_music_choice_label(uint8_t index);
/* Label for a stored setting (0 or a song id, or the old CHAMPION value). */
const char* pkb_music_setting_label(uint8_t stored);
bool pkb_music_setting_ok(uint8_t stored);

/* What Crystal's PlayBattleMusic would start for this boss, ignoring the
 * link-battle fallback (a Colosseum fight forces the Johto trainer theme).
 * Red and Lance use the champion theme. The rival uses the rival theme. Kanto gym leaders
 * use the Kanto gym theme. Johto gym leaders and Will, Koga, Bruno and Karen
 * use the Johto gym theme. */
uint8_t pkb_music_for_boss(uint8_t boss);
/* The theme Crystal would play for this trainer class outside a link
 * battle. Grunts and Rocket executives use the Rocket theme. Everyone
 * Crystal leaves on the Johto trainer theme stays there. */
uint8_t pkb_music_for_class(uint8_t trainer_class);
/* choice 0 follows the opponent. A random battle is the Johto trainer theme,
 * which the game already plays, so the caller can skip the overlay. */
uint8_t pkb_music_resolve(uint8_t choice, bool boss, uint8_t boss_index);

/* Harness overlays. OFF leaves the block alone. */
bool pkb_music_test_apply(uint8_t block[PKB_PARTY_BLOCK_LEN], int mode);
/* Install the player-name trigger that calls PlayMusic with `song` once.
 * Nicknames are not modified. Returns false if `song` is 0 or would put
 * a $FE in the party block. A picture class uses pkb_picture_apply_song
 * instead, so the two features share one cave. */
bool pkb_music_apply_song(uint8_t block[PKB_PARTY_BLOCK_LEN], uint8_t song);

#ifdef __cplusplus
}
#endif
