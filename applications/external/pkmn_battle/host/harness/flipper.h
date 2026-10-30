#pragma once

/*
 * Host model of the Flipper app's Start Battle scene
 * (app/scenes/pkb_scene_battle.c): owns the PblSession, draws a team and TX
 * blocks before every battle and writes the battle log the device would.
 * Smoke-test mode (no engine): one Pokemon that always answers move 1.
 */

#include <pkb/log.h>
#include <pkb/party_block.h>
#include <pkb/session.h>
#include <pkb/team.h>

#define FM_LOG_CAP 8192

typedef struct {
    PblSession s;
    PkbPolicyRng rng;
    PkbBlobReader pools;
    uint8_t team_size;

    PkbTeam team;
    uint8_t rn[PKB_RN_BLOCK_LEN];
    uint8_t party[PKB_PARTY_BLOCK_LEN];
    uint8_t patch[PKB_PATCH_BLOCK_LEN];
    /* replay: send these blocks instead of drawing a team */
    bool fixed_blocks;

    PkbLog log;
    uint8_t log_buf[FM_LOG_CAP];
    bool active;
    uint32_t logged;
    int battles;
    uint32_t events;

    /* engine mode: a 6-Pokemon team of supported moves, actions posted by
     * the engine (the harness runs it when battle_ready is set) */
    bool engine_mode;
    bool battle_ready;
    /* team choice: random teams at level_lo-level_hi, or boss team
     * `boss` (>= 0) scaled to boss_ace (0 = the game's levels) */
    uint8_t level_lo, level_hi;
    int boss;
    uint8_t boss_ace;
    int music_test; /* PKB_MUSIC_*: overlay applied in fm_prepare_team */
    uint8_t sprite_class; /* 0 = Cal, else Crystal trainer class for the picture */
    uint32_t crc[1024]; /* engine state CRC at each exchange of this battle */
    uint32_t ncrc;
    uint8_t result; /* log code of the engine's result (1 win, 2 lose, 3 draw) */

    const char* out_dir; /* battle logs go here (NULL: not written) */
    char last_log[512];
    bool error;
} FlipperModel;

void fm_init(FlipperModel* fm, uint64_t seed, const char* out_dir);
void fm_init_engine(FlipperModel* fm, uint64_t seed, const char* out_dir);
/* Rebuild the outgoing blocks with this music overlay (PKB_MUSIC_*). */
bool fm_set_music(FlipperModel* fm, int mode);
/* Rebuild the outgoing blocks with this trainer class picture (0 clears it). */
bool fm_set_sprite(FlipperModel* fm, uint8_t trainer_class);
/* Replay: use these blocks for the next battle (and keep using them). */
void fm_set_blocks(
    FlipperModel* fm,
    const uint8_t rn[PKB_RN_BLOCK_LEN],
    const uint8_t party[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch[PKB_PATCH_BLOCK_LEN]);
/* Main-loop tick, as battle_tick() on the device. */
void fm_tick(FlipperModel* fm, uint32_t now_ms);
