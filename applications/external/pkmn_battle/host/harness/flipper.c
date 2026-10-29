#include "flipper.h"

#include <stdio.h>
#include <string.h>

#include <pkb/engine.h>
#include <pkb/music_test.h>
#include <pkb/picture.h>

#include "host_io.h"

#define SMOKE_TEAM_SIZE 1
#define SMOKE_ACTION    0 /* BATTLEACTION_MOVE1 */

static HostBlob pools_blob;

static void fm_notify(void* ctx, uint32_t events) {
    FlipperModel* fm = ctx;
    fm->events |= events;
}

static bool fm_prepare_team(FlipperModel* fm) {
    if(fm->fixed_blocks) {
        pbl_set_tx(&fm->s, fm->rn, fm->party, fm->patch);
        return true;
    }
    uint8_t rns[PKB_RNS_LEN];
    PkbMoveFilter filter = fm->engine_mode ? pkb_engine_move_filter : NULL;
    if(fm->boss >= 0) {
        if(!pkb_boss_team_build(
               &fm->team, (uint8_t)fm->boss, fm->boss_ace, (uint16_t)pkb_policy_rng_next(&fm->rng)))
            return false;
    } else if(
        pkb_team_generate(
            &fm->team,
            PKB_PARTY_LENGTH,
            fm->level_lo,
            fm->level_hi,
            &fm->pools,
            filter,
            NULL,
            &fm->rng) != PkbTeamOk) {
        return false;
    }
    if(fm->team_size < fm->team.count) fm->team.count = fm->team_size;
    for(unsigned i = 0; i < PKB_RNS_LEN; i++)
        rns[i] = (uint8_t)pkb_policy_rng_next(&fm->rng);
    pkb_rn_block_build(rns, fm->rn);
    if(!pkb_party_block_build(&fm->team, fm->team.name, fm->party, fm->patch)) return false;
    if(fm->music_test == PKB_MUSIC_TRACE || fm->music_test == PKB_MUSIC_RETURN) {
        if(!pkb_music_test_apply(fm->party, fm->music_test)) return false;
    } else {
        uint8_t song = fm->music_test == PKB_MUSIC_CHAMPION ? PKB_SONG_CHAMPION : 0;
        if(!pkb_link_overlay_apply(fm->party, fm->sprite_class, song)) return false;
    }
    pbl_set_tx(&fm->s, fm->rn, fm->party, fm->patch);
    return true;
}

void fm_init(FlipperModel* fm, uint64_t seed, const char* out_dir) {
    memset(fm, 0, sizeof(*fm));
    pkb_policy_rng_seed(&fm->rng, seed);
    fm->pools = host_movepools(&pools_blob);
    fm->team_size = SMOKE_TEAM_SIZE;
    fm->level_lo = fm->level_hi = 50;
    fm->boss = -1;
    fm->out_dir = out_dir;
    fm->s.notify = fm_notify;
    fm->s.notify_ctx = fm;
    fm->s.auto_action = SMOKE_ACTION;
    pbl_start(&fm->s);
    fm->error = !fm_prepare_team(fm);
}

bool fm_set_music(FlipperModel* fm, int mode) {
    fm->music_test = mode;
    return fm_prepare_team(fm);
}

bool fm_set_sprite(FlipperModel* fm, uint8_t trainer_class) {
    fm->sprite_class = trainer_class;
    return fm_prepare_team(fm);
}

void fm_init_engine(FlipperModel* fm, uint64_t seed, const char* out_dir) {
    memset(fm, 0, sizeof(*fm));
    pkb_policy_rng_seed(&fm->rng, seed);
    fm->pools = host_movepools(&pools_blob);
    fm->team_size = PKB_PARTY_LENGTH;
    fm->level_lo = fm->level_hi = 50;
    fm->boss = -1;
    fm->engine_mode = true;
    fm->out_dir = out_dir;
    fm->s.notify = fm_notify;
    fm->s.notify_ctx = fm;
    fm->s.auto_action = PBL_ACTION_NONE;
    pbl_start(&fm->s);
    fm->error = !fm_prepare_team(fm);
}

void fm_set_blocks(
    FlipperModel* fm,
    const uint8_t rn[PKB_RN_BLOCK_LEN],
    const uint8_t party[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch[PKB_PATCH_BLOCK_LEN]) {
    memcpy(fm->rn, rn, sizeof(fm->rn));
    memcpy(fm->party, party, sizeof(fm->party));
    memcpy(fm->patch, patch, sizeof(fm->patch));
    fm->fixed_blocks = true;
    fm_prepare_team(fm);
}

static void fm_log_begin(FlipperModel* fm) {
    pkb_log_init(&fm->log, fm->log_buf, sizeof(fm->log_buf), 0);
    pkb_log_note(&fm->log, fm->engine_mode ? "engine" : "M2 smoke test: 1 mon, always $80");
    pbl_log_blocks(&fm->s, &fm->log);
    fm->logged = 0;
    fm->active = true;
}

static void fm_drain(FlipperModel* fm) {
    uint32_t done = pbl_battle_exchanges_done(&fm->s);
    uint8_t mine, gb;
    while(fm->logged < done) {
        if(pbl_battle_history(&fm->s, fm->logged, &mine, &gb)) {
            uint32_t crc = fm->logged < fm->ncrc ? fm->crc[fm->logged] : 0;
            pkb_log_exchange(&fm->log, (uint16_t)fm->logged, mine, gb, crc);
        } else {
            pkb_log_note(&fm->log, "exchange lost from history");
        }
        fm->logged++;
    }
}

static void fm_log_write(FlipperModel* fm) {
    uint32_t bytes = fm->s.bytes - fm->s.round_start_bytes;
    uint8_t end[5] = {
        fm->result,
        (uint8_t)bytes,
        (uint8_t)(bytes >> 8),
        (uint8_t)(bytes >> 16),
        (uint8_t)(bytes >> 24)};
    pkb_log_record(&fm->log, PkbLogBattleEnd, end, sizeof(end));
    fm->last_log[0] = 0;
    if(!fm->out_dir) return;
    snprintf(fm->last_log, sizeof(fm->last_log), "%s/battle%02d.pbl", fm->out_dir, fm->battles);
    FILE* f = fopen(fm->last_log, "wb");
    if(!f || fwrite(fm->log_buf, 1, fm->log.len, f) != fm->log.len) fm->error = true;
    if(f) fclose(f);
}

void fm_tick(FlipperModel* fm, uint32_t now_ms) {
    uint32_t ev = fm->events;
    fm->events = 0;
    if(fm->active) {
        fm_drain(fm);
        if((ev & PBL_EV_BATTLE_END) || pbl_battle_end_hint(&fm->s, now_ms)) {
            fm_log_write(fm);
            fm->active = false;
            fm->battles++;
            if(!fm_prepare_team(fm)) fm->error = true;
        }
    }
    if(ev & PBL_EV_BLOCKS_DONE) {
        fm_log_begin(fm);
        if(fm->engine_mode) {
            fm->battle_ready = true;
            fm->ncrc = 0;
            fm->result = 0;
        }
    }
    if(ev & PBL_EV_NEED_TX) fm->error = true;
}
