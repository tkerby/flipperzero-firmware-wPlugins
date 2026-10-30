#pragma once

/*
 * Slave side of a Pokemon Crystal Cable Club Colosseum session.
 *
 * The Game Boy is always the clock master; the Flipper answers one byte per
 * transfer. pbl_on_byte() is called once per received byte (from the gblink
 * ISR on the device, from master_sim on the host) and returns the byte to
 * transmit in the NEXT transfer. It is O(1) and never blocks.
 *
 * Phases follow what the master does (engine/link/link.asm, home/serial.asm):
 *   CONN     answer the $01 handshake with $02 (WaitForLinkedFriend)
 *   RECEPT   $61 exchange (CheckLinkTimeout_Receptionist)
 *   ROOM     $D2 room sync (CheckBothSelectedSameRoom); also idle in the
 *            Colosseum between battles
 *   CONSOLE  $85 then $86 exchanges (CheckLinkTimeout_Gen2)
 *   XB_*     the three Serial_ExchangeBytes blocks (RNs, party, patch list)
 *   BATTLE   $8x action exchanges (_LinkBattleSendReceiveAction)
 * Then back to ROOM. Every exchange of nibbles uses the same rules as
 * WaitLinkTransfer: the master keeps sending until it has our nibble, sends
 * 10 more, then 10 $00 acks, and never clears a stale value, so we stop
 * repeating ours shortly after seeing theirs.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include <pkb/party_block.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PblPhaseIdle,
    PblPhaseConn,
    PblPhaseRecept,
    PblPhaseRoom,
    PblPhaseConsole,
    PblPhaseXbRn,
    PblPhaseXbParty,
    PblPhaseXbPatch,
    PblPhaseBattle,
    PblPhaseCount,
} PblPhase;

/* Event bits passed to PblSession.notify (from ISR context on the device). */
#define PBL_EV_PHASE (1u << 0) /* phase changed */
#define PBL_EV_GB_ACTION (1u << 1) /* a new GB battle action was latched */
#define PBL_EV_EXCHANGE_DONE (1u << 2) /* our side of an action exchange completed */
#define PBL_EV_BLOCKS_DONE (1u << 3) /* RN/party/patch received; battle starting */
#define PBL_EV_BATTLE_END (1u << 4) /* end of battle detected (ExitLinkCommunications) */
#define PBL_EV_NEED_TX (1u << 5) /* blocks are about to start but TX buffers are not ready */

/* After seeing the master's nibble we repeat ours this many more times. */
#define PBL_REPEAT_AFTER_SEEN 5
/* Consecutive non-$8x bytes that separate two master exchanges (it always
 * sends 10 $00 acks); fewer is treated as line noise inside one exchange. */
#define PBL_ARM_QUIET 3
/* A lone $00 after this long a gap, not followed by $8x, is the end of battle. */
#define PBL_EXIT_GAP_MS 300
#define PBL_ACTION_NONE 0xFF

typedef struct {
    /* --- configuration (set before pbl_start, read-only afterwards) --- */
    void (*notify)(void* ctx, uint32_t events);
    void* notify_ctx;
    /* Without an engine, answer every battle exchange with this nibble
     * (smoke test). PBL_ACTION_NONE = wait for pbl_battle_post_action. */
    uint8_t auto_action;

    /* --- TX buffers: written by the main thread while tx_ready is false --- */
    uint8_t rn_tx[PKB_RN_BLOCK_LEN];
    uint8_t party_tx[PKB_PARTY_BLOCK_LEN];
    uint8_t patch_tx[PKB_PATCH_BLOCK_LEN];
    atomic_bool tx_ready;

    /* --- RX buffers: valid once the corresponding block completed --- */
    uint8_t rn_rx[PKB_RN_BLOCK_LEN];
    uint8_t party_rx[PKB_PARTY_BLOCK_LEN];
    uint8_t patch_rx[PKB_PATCH_BLOCK_LEN];

    /* --- state (ISR-owned unless noted) --- */
    atomic_uint_fast8_t phase;
    uint8_t last_rx;
    uint32_t last_rx_ms;
    uint32_t bytes;
    uint32_t round_start_bytes; /* `bytes` when the current console round began */

    /* nibble exchanges before the battle */
    uint8_t ex_value; /* full byte we answer with, 0 = none */
    uint8_t ex_repeat; /* repeats left after the master's value was seen */
    uint8_t ex_last_seen;
    bool ex_armed; /* saw a different byte since the last answer */
    uint8_t default_tx;

    /* block exchange */
    bool xb_data; /* false: sending $FD, waiting for the master's $FD */
    uint16_t xb_count;

    /* battle exchanges: engine thread posts, ISR completes */
    atomic_uint_fast8_t my_action; /* nibble to send, PBL_ACTION_NONE if none */
    atomic_uint_fast32_t my_posted; /* number of actions posted by the engine */
    atomic_uint_fast32_t my_done; /* number of our exchanges completed */
    uint8_t my_repeat; /* $8x left to send after latch */
    bool my_sending;
    atomic_uint_fast32_t gb_latched; /* number of GB actions latched */
    uint8_t gb_actions[4]; /* ring indexed by latch number */
    uint8_t history[16][2]; /* completed exchanges: [ours, master's], by index */
    uint8_t gb_quiet; /* consecutive non-$8x bytes since the last $8x */
    bool exit_candidate;
    atomic_bool end_requested; /* engine knows the battle is over */
    atomic_uint_fast32_t battles;
} PblSession;

/* Reset to CONN (waiting for the handshake). TX buffers are kept. */
void pbl_start(PblSession* s);

/* ISR entry: rx = byte received in the transfer that just finished,
 * now_ms = monotonic milliseconds. Returns the byte for the next transfer. */
uint8_t pbl_on_byte(PblSession* s, uint8_t rx, uint32_t now_ms);

/* Main thread: fill the TX buffers for the next comms round, then mark ready.
 * Must be called while the session is not inside XB_* phases. */
void pbl_set_tx(PblSession* s, const uint8_t rn[PKB_RN_BLOCK_LEN],
                const uint8_t party[PKB_PARTY_BLOCK_LEN], const uint8_t patch[PKB_PATCH_BLOCK_LEN]);

/* Engine thread: post our nibble for the next exchange. */
void pbl_battle_post_action(PblSession* s, uint8_t nibble);
/* Engine thread: true once exchange `index` (0-based count of posted
 * actions) has completed on both sides; *gb_nibble receives the master's. */
bool pbl_battle_exchange_result(PblSession* s, uint32_t index, uint8_t* gb_nibble);

/* Main thread: nibbles of completed exchange `index` while it is still in
 * the 16-entry history; false if not completed yet or already overwritten. */
bool pbl_battle_history(const PblSession* s, uint32_t index, uint8_t* mine, uint8_t* gb);
uint32_t pbl_battle_exchanges_done(const PblSession* s);

/* Engine thread: the battle is over; the next byte returns to ROOM. */
void pbl_battle_request_end(PblSession* s);
/* Main thread: true if the end of battle looks reached (a lone $00 after a
 * gap and nothing since), before the session has confirmed it. */
bool pbl_battle_end_hint(const PblSession* s, uint32_t now_ms);

/* The master's 10 shared random numbers (valid after XB_RN). */
bool pbl_master_rns(const PblSession* s, uint8_t rns[PKB_RNS_LEN]);

const char* pbl_phase_name(PblPhase phase);

struct PkbLog;
/* Append the six block records (RN/party/patch, sent and received). */
void pbl_log_blocks(const PblSession* s, struct PkbLog* log);

#ifdef __cplusplus
}
#endif
