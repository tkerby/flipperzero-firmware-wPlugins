/*
 * sim_log OUT.pbl
 *
 * Runs a simulated Colosseum session (master_sim as the Game Boy, the
 * session in smoke-test mode as the Flipper) and writes the battle log
 * the device would write, for tools/gen/decode_log.py to read.
 */
#include <stdio.h>
#include <string.h>
#include <pkb/log.h>
#include <pkb/party_block.h>
#include <pkb/session.h>
#include "../sim/master_sim.h"
#include "pb_const.h"

static PblSession slave;
static MasterSim gb;
static uint8_t log_buf[8192];

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: sim_log OUT.pbl\n");
        return 2;
    }
    static const uint8_t moves[4] = {TACKLE, GROWL, THUNDERSHOCK, QUICK_ATTACK};
    PkbTeam mine, theirs;
    mine.count = 1;
    mine.trainer_id = 1234;
    pkb_team_mon_init(&mine.mon[0], PIKACHU, 50, moves);
    theirs.count = 3;
    theirs.trainer_id = 0xFE21;
    pkb_team_mon_init(&theirs.mon[0], TYPHLOSION, 50, moves);
    pkb_team_mon_init(&theirs.mon[1], CHANSEY, 50, moves);
    pkb_team_mon_init(&theirs.mon[2], SNORLAX, 50, moves);

    uint8_t rns[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint8_t rn[17], party[450], patch[200];
    pkb_rn_block_build(rns, rn);
    pkb_party_block_build(&mine, "FLIPPER", party, patch);
    slave.auto_action = 0;
    pbl_start(&slave);
    pbl_set_tx(&slave, rn, party, patch);
    ms_init(&gb, &slave);

    uint8_t g_rn[17], g_party[450], g_patch[200], rx_rn[17], rx_party[450], rx_patch[200];
    static const uint8_t gb_rns[10] = {0x55, 0x10, 0xFC, 0, 1, 2, 3, 4, 5, 0x99};
    pkb_rn_block_build(gb_rns, g_rn);
    pkb_party_block_build(&theirs, "GOLD", g_party, g_patch);
    if(!ms_receptionist(&gb, 30) ||
       !ms_console(&gb, g_rn, g_party, g_patch, rx_rn, rx_party, rx_patch)) {
        fprintf(stderr, "session failed: %s\n", gb.error ? gb.error : "?");
        return 1;
    }

    PkbLog log;
    pkb_log_init(&log, log_buf, sizeof(log_buf), 1760000000u);
    pkb_log_note(&log, "host simulation");
    static const uint8_t gb_actions[] = {0, 1, 4, 2};
    for(unsigned i = 0; i < sizeof(gb_actions); i++) {
        ms_battle_exchange(&gb, gb_actions[i]);
        ms_delay_frames(&gb, 30);
        if(i == 0) pbl_log_blocks(&slave, &log); /* patch RX completes on the first exchange */
    }
    for(uint32_t i = 0; i < pbl_battle_exchanges_done(&slave); i++) {
        uint8_t a, b;
        if(pbl_battle_history(&slave, i, &a, &b)) pkb_log_exchange(&log, (uint16_t)i, a, b, 0);
    }
    uint8_t end[5] = {0, (uint8_t)slave.bytes, (uint8_t)(slave.bytes >> 8), 0, 0};
    pkb_log_record(&log, PkbLogBattleEnd, end, sizeof(end));

    FILE* f = fopen(argv[1], "wb");
    if(!f || fwrite(log_buf, 1, log.len, f) != log.len) return 1;
    fclose(f);
    return log.overflow ? 1 : 0;
}
