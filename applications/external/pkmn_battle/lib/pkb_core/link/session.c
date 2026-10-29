/*
 * Slave-side Cable Club session state machine. See pkb/session.h for the
 * phase overview; the master's behaviour being answered is quoted per phase.
 */
#include <string.h>
#include <pkb/log.h>
#include <pkb/session.h>
#include "session_i.h"

static const char* const phase_names[PblPhaseCount] = {
    "IDLE", "CONN", "RECEPT", "ROOM", "CONSOLE", "XB_RN", "XB_PARTY", "XB_PATCH", "BATTLE",
};

const char* pbl_phase_name(PblPhase phase) {
    return phase < PblPhaseCount ? phase_names[phase] : "?";
}

void pbl_notify(PblSession* s, uint32_t events) {
    if(s->notify) s->notify(s->notify_ctx, events);
}

void pbl_set_phase(PblSession* s, PblPhase phase) {
    atomic_store(&s->phase, phase);
    s->ex_value = 0;
    s->ex_repeat = 0;
    s->ex_armed = true;
    switch(phase) {
    case PblPhaseConn:
        s->default_tx = ESTABLISH_EXTERNAL;
        break;
    case PblPhaseRoom:
        /* Pre-load the room sync byte: Link_EnsureSync only needs one of the
         * two bytes it keeps to be $Dx, and $Dx is ignored everywhere else. */
        s->default_tx = SYNC_COLOSSEUM;
        break;
    case PblPhaseConsole:
        s->round_start_bytes = s->bytes;
        s->default_tx = 0x00;
        break;
    case PblPhaseXbRn:
    case PblPhaseXbParty:
    case PblPhaseXbPatch:
        s->xb_data = false;
        s->xb_count = 0;
        s->default_tx = PKB_SERIAL_PREAMBLE_BYTE;
        break;
    default:
        s->default_tx = 0x00;
        break;
    }
    pbl_notify(s, PBL_EV_PHASE);
}

void pbl_start(PblSession* s) {
    s->last_rx = 0;
    s->last_rx_ms = 0;
    s->bytes = 0;
    s->round_start_bytes = 0;
    pbl_battle_reset(s);
    pbl_set_phase(s, PblPhaseConn);
}

void pbl_set_tx(PblSession* s, const uint8_t rn[PKB_RN_BLOCK_LEN],
                const uint8_t party[PKB_PARTY_BLOCK_LEN], const uint8_t patch[PKB_PATCH_BLOCK_LEN]) {
    atomic_store(&s->tx_ready, false);
    memcpy(s->rn_tx, rn, PKB_RN_BLOCK_LEN);
    memcpy(s->party_tx, party, PKB_PARTY_BLOCK_LEN);
    memcpy(s->patch_tx, patch, PKB_PATCH_BLOCK_LEN);
    atomic_store(&s->tx_ready, true);
}

bool pbl_master_rns(const PblSession* s, uint8_t rns[PKB_RNS_LEN]) {
    return pkb_rn_block_parse(s->rn_rx, PKB_RN_BLOCK_LEN, rns);
}

/*
 * Answer a WaitLinkTransfer-style exchange: once the master's value is seen
 * (and we were armed by a different byte since the last answer), send the
 * same value back PBL_REPEAT_AFTER_SEEN times, then fall back to the phase's
 * default byte so no stale value is left for the master's next exchange.
 */
static void answer_begin(PblSession* s, uint8_t value) {
    s->ex_value = value;
    s->ex_repeat = PBL_REPEAT_AFTER_SEEN;
    s->ex_armed = false;
}

/* Returns the byte to send if an answer is in progress, else -1. Sets *done
 * when the last repeat has just been scheduled. */
static int answer_step(PblSession* s, bool* done) {
    *done = false;
    if(!s->ex_value) return -1;
    uint8_t v = s->ex_value;
    if(s->ex_repeat == 0) {
        s->ex_value = 0;
        *done = true;
        return -1;
    }
    s->ex_repeat--;
    return v;
}

static uint8_t on_conn(PblSession* s, uint8_t rx) {
    /* WaitForLinkedFriend: the master clocks out $01 (USING_EXTERNAL_CLOCK)
     * and becomes the clock master when it reads back $02. */
    if(rx == ESTABLISH_INTERNAL_PROBE) {
        pbl_set_phase(s, PblPhaseRecept);
        return s->default_tx;
    }
    return ESTABLISH_EXTERNAL;
}

static bool handshake_restart(PblSession* s, uint8_t rx) {
    if(rx != ESTABLISH_INTERNAL_PROBE) return false;
    pbl_set_phase(s, PblPhaseConn);
    return true;
}

uint8_t pbl_nibble_on_byte(PblSession* s, uint8_t rx) {
    PblPhase phase = atomic_load(&s->phase);
    bool done;

    if(handshake_restart(s, rx)) return ESTABLISH_EXTERNAL;
    if(rx != s->ex_last_seen) s->ex_armed = true;

    if(!s->ex_value && s->ex_armed) {
        switch(rx) {
        case RECEPT_CHECK: /* CheckLinkTimeout_Receptionist (also retried) */
            if(phase == PblPhaseRecept || phase == PblPhaseRoom) answer_begin(s, rx);
            break;
        case CONSOLE_CHECK_1: /* CheckLinkTimeout_Gen2 */
            if(phase == PblPhaseRoom) pbl_set_phase(s, PblPhaseConsole);
            answer_begin(s, rx);
            break;
        case CONSOLE_CHECK_2:
            if(phase == PblPhaseConsole) answer_begin(s, rx);
            break;
        default:
            break;
        }
    }
    s->ex_last_seen = rx;

    uint8_t answering = s->ex_value;
    int tx = answer_step(s, &done);
    if(done) {
        if(answering == RECEPT_CHECK) {
            pbl_set_phase(s, PblPhaseRoom);
        } else if(answering == CONSOLE_CHECK_2) {
            if(!atomic_load(&s->tx_ready)) pbl_notify(s, PBL_EV_NEED_TX);
            pbl_set_phase(s, PblPhaseXbRn);
        }
        s->ex_last_seen = rx;
        s->ex_armed = false;
    }
    return tx >= 0 ? (uint8_t)tx : s->default_tx;
}

uint8_t pbl_on_byte(PblSession* s, uint8_t rx, uint32_t now_ms) {
    PblPhase phase = atomic_load(&s->phase);
    uint32_t gap = now_ms - s->last_rx_ms;
    uint8_t tx;

    s->bytes++;
    switch(phase) {
    case PblPhaseConn:
        tx = on_conn(s, rx);
        break;
    case PblPhaseRecept:
    case PblPhaseRoom:
    case PblPhaseConsole:
        tx = pbl_nibble_on_byte(s, rx);
        break;
    case PblPhaseXbRn:
    case PblPhaseXbParty:
    case PblPhaseXbPatch:
        tx = pbl_xb_on_byte(s, rx);
        break;
    case PblPhaseBattle:
        tx = pbl_battle_on_byte(s, rx, gap);
        break;
    default:
        tx = 0x00;
        break;
    }
    s->last_rx = rx;
    s->last_rx_ms = now_ms;
    return tx;
}

/* Called by the battle code when the end of battle was detected: continue
 * with this byte as if in the room. */
uint8_t pbl_enter_room_with(PblSession* s, uint8_t rx) {
    atomic_fetch_add(&s->battles, 1);
    pbl_set_phase(s, PblPhaseRoom);
    pbl_notify(s, PBL_EV_BATTLE_END);
    s->ex_last_seen = 0;
    return pbl_nibble_on_byte(s, rx);
}

void pbl_log_blocks(const PblSession* s, PkbLog* log) {
    pkb_log_record(log, PkbLogRnTx, s->rn_tx, PKB_RN_BLOCK_LEN);
    pkb_log_record(log, PkbLogRnRx, s->rn_rx, PKB_RN_BLOCK_LEN);
    pkb_log_record(log, PkbLogPartyTx, s->party_tx, PKB_PARTY_BLOCK_LEN);
    pkb_log_record(log, PkbLogPartyRx, s->party_rx, PKB_PARTY_BLOCK_LEN);
    pkb_log_record(log, PkbLogPatchTx, s->patch_tx, PKB_PATCH_BLOCK_LEN);
    pkb_log_record(log, PkbLogPatchRx, s->patch_rx, PKB_PATCH_BLOCK_LEN);
}
