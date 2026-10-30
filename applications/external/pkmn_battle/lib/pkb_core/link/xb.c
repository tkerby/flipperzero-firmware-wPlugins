/*
 * Slave side of Serial_ExchangeBytes (home/serial.asm) for the three
 * Colosseum blocks: RNs (17), party (450), patch lists (200).
 *
 * Like the Game Boy routine: keep sending the block's first byte ($FD)
 * until a $FD arrives (that byte is not stored), then send byte k while
 * receiving byte k. The master runs the same loop one transfer ahead, so
 * its last byte of each block reaches us in its next block's first transfer
 * and ours is never stored by it; the preambles and the 3 padding bytes of
 * the party block absorb that shift.
 */
#include <pkb/session.h>
#include "session_i.h"

uint8_t pbl_xb_on_byte(PblSession* s, uint8_t rx) {
    PblPhase phase = atomic_load(&s->phase);
    const uint8_t* tx;
    uint8_t* buf;
    uint16_t len;

    switch(phase) {
    case PblPhaseXbRn:
        tx = s->rn_tx;
        buf = s->rn_rx;
        len = PKB_RN_BLOCK_LEN;
        break;
    case PblPhaseXbParty:
        tx = s->party_tx;
        buf = s->party_rx;
        len = PKB_PARTY_BLOCK_LEN;
        break;
    default:
        tx = s->patch_tx;
        buf = s->patch_rx;
        len = PKB_PATCH_BLOCK_LEN;
        break;
    }

    if(!s->xb_data) {
        if(rx == PKB_SERIAL_PREAMBLE_BYTE) {
            s->xb_data = true;
            s->xb_count = 0;
            return tx[0];
        }
        /* Before the first block starts the master may still retry the
         * console check, or restart the link from scratch. */
        if(phase == PblPhaseXbRn && rx == CONSOLE_CHECK_1) {
            pbl_set_phase(s, PblPhaseRoom);
            return pbl_nibble_on_byte(s, rx);
        }
        if(rx == ESTABLISH_INTERNAL_PROBE && phase == PblPhaseXbRn) {
            pbl_set_phase(s, PblPhaseConn);
            return ESTABLISH_EXTERNAL;
        }
        return PKB_SERIAL_PREAMBLE_BYTE;
    }

    buf[s->xb_count++] = rx;
    if(s->xb_count < len) return tx[s->xb_count];

    switch(phase) {
    case PblPhaseXbRn:
        pbl_set_phase(s, PblPhaseXbParty);
        return PKB_SERIAL_PREAMBLE_BYTE;
    case PblPhaseXbParty:
        pbl_set_phase(s, PblPhaseXbPatch);
        return PKB_SERIAL_PREAMBLE_BYTE;
    default:
        atomic_store(&s->tx_ready, false); /* consumed: next battle needs a fresh team */
        pbl_battle_begin(s);
        pbl_notify(s, PBL_EV_BLOCKS_DONE);
        return 0x00;
    }
}
