/*
 * C model of the clock-master Game Boy's side of a Crystal Cable Club
 * session (home/serial.asm, engine/link/link.asm, mobile/mobile_40.asm),
 * wired to a PblSession slave. Each routine mirrors its asm namesake.
 *
 * Hardware model: the master transmits rSB, which its Serial interrupt
 * reloads from hSerialSend after every transfer (so what it "sends" goes out
 * one transfer later), then sets hSerialSend = $FE. A transfer started by
 * LinkTransfer completes during the following DelayFrame, after the second
 * .Receive, as on hardware (8 bits at 8 kHz take ~1 ms).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pkb/session.h>

#define MS_FRAME_US 16743u

typedef struct MasterSim MasterSim;
typedef void (*MsFrameHook)(MasterSim* m, void* ctx);

struct MasterSim {
    /* GB registers / HRAM / WRAM that the routines use */
    uint8_t rSB;
    uint8_t hSerialSend;
    uint8_t hSerialReceive;
    bool hSerialReceivedNewData;
    uint8_t hSerialConnectionStatus;
    bool hSerialIgnoringInitialData;
    bool ie_serial_only; /* rIE == IE_SERIAL (during the byte blocks) */
    uint8_t wLinkMode;
    uint8_t wPlayerLinkAction;
    uint8_t wOtherPlayerLinkAction;
    uint8_t wOtherPlayerLinkMode;
    uint16_t wLinkTimeoutFrames;
    bool transfer_pending;

    /* slave */
    PblSession* slave;
    uint8_t slave_preload; /* byte the slave transmits in the next transfer */

    /* time */
    uint64_t now_us;
    uint32_t frames;
    uint32_t transfers;
    MsFrameHook on_frame;
    void* hook_ctx;

    /* optional trace of every transfer: master byte, slave byte */
    uint8_t* trace;
    uint32_t trace_cap;
    uint32_t trace_len;

    /* failure reporting */
    const char* error;
};

void ms_init(MasterSim* m, PblSession* slave);
void ms_delay_frames(MasterSim* m, uint32_t n);

/* Pokecenter2F LinkReceptionistScript_Battle up to "Please come in". */
bool ms_receptionist(MasterSim* m, uint32_t save_prompt_frames);

/* Colosseum console: LinkCommunications through the three byte blocks.
 * master_*_tx are the master's blocks; the received slave blocks are
 * written to *_rx (raw, as stored by Serial_ExchangeBytes). */
bool ms_console(
    MasterSim* m,
    const uint8_t rn_tx[PKB_RN_BLOCK_LEN],
    const uint8_t party_tx[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch_tx[PKB_PATCH_BLOCK_LEN],
    uint8_t rn_rx[PKB_RN_BLOCK_LEN],
    uint8_t party_rx[PKB_PARTY_BLOCK_LEN],
    uint8_t patch_rx[PKB_PATCH_BLOCK_LEN]);

/* _LinkBattleSendReceiveAction: returns the slave's nibble (wBattleAction). */
uint8_t ms_battle_exchange(MasterSim* m, uint8_t my_nibble);

/* ExitLinkCommunications' single $00 transfer. */
void ms_exit_link_communications(MasterSim* m);

/* WaitForOtherPlayerToExit when leaving the Colosseum. */
void ms_wait_for_other_player_to_exit(MasterSim* m);
