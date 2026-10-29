/*
 * Battle action exchanges, answering _LinkBattleSendReceiveAction
 * (mobile/mobile_40.asm) on the master:
 *
 *   .waiting      LinkTransfer every frame until a $8x arrives
 *   .receive      10 more LinkTransfers (still sending its $8x and
 *                 re-reading ours)
 *   .acknowledge  10 LinkDataReceived ($00)
 *
 * LinkTransfer reads hSerialReceive before each transfer and never clears a
 * non-$8x value, so whatever we send last before the master's next exchange
 * is read first there. Rules:
 *   - send $00 until the engine has posted our nibble, then $80|nibble;
 *   - after the master's $8x has been latched, send ours
 *     PBL_REPEAT_AFTER_SEEN more times, then $00 again;
 *   - latch a new master action only after PBL_ARM_QUIET non-$8x bytes in
 *     a row (exchanges are separated by its 10 $00 acks; a single odd byte
 *     inside its $8x stream must not look like a new exchange).
 */
#include <pkb/session.h>
#include "session_i.h"

void pbl_battle_reset(PblSession* s) {
    atomic_store(&s->my_action, PBL_ACTION_NONE);
    atomic_store(&s->my_posted, 0);
    atomic_store(&s->my_done, 0);
    atomic_store(&s->gb_latched, 0);
    atomic_store(&s->end_requested, false);
    s->my_repeat = 0;
    s->my_sending = false;
    s->gb_quiet = PBL_ARM_QUIET;
    s->exit_candidate = false;
}

void pbl_battle_begin(PblSession* s) {
    pbl_battle_reset(s);
    pbl_set_phase(s, PblPhaseBattle);
}

void pbl_battle_post_action(PblSession* s, uint8_t nibble) {
    atomic_store(&s->my_action, nibble & 0x0F);
    atomic_fetch_add(&s->my_posted, 1);
}

bool pbl_battle_exchange_result(PblSession* s, uint32_t index, uint8_t* gb_nibble) {
    if(atomic_load(&s->my_done) <= index) return false;
    *gb_nibble = s->gb_actions[index & 3];
    return true;
}

bool pbl_battle_history(const PblSession* s, uint32_t index, uint8_t* mine, uint8_t* gb) {
    uint32_t done = atomic_load(&s->my_done);
    if(index >= done || done - index > 16) return false;
    *mine = s->history[index & 15][0];
    *gb = s->history[index & 15][1];
    return atomic_load(&s->my_done) - index <= 16; /* not overwritten meanwhile */
}

uint32_t pbl_battle_exchanges_done(const PblSession* s) {
    return atomic_load(&s->my_done);
}

void pbl_battle_request_end(PblSession* s) {
    atomic_store(&s->end_requested, true);
}

bool pbl_battle_end_hint(const PblSession* s, uint32_t now_ms) {
    return atomic_load(&s->phase) == PblPhaseBattle && s->exit_candidate &&
           now_ms - s->last_rx_ms >= PBL_EXIT_GAP_MS;
}

uint8_t pbl_battle_on_byte(PblSession* s, uint8_t rx, uint32_t gap_ms) {
    uint32_t events = 0;
    uint8_t tx = 0x00;

    /* ExitLinkCommunications clocks out a single $00 after the result
     * screen; every exchange also starts with a $00, but its $8x follows
     * one frame later. */
    if(s->exit_candidate) {
        s->exit_candidate = false;
        if(gap_ms >= PBL_EXIT_GAP_MS) return pbl_enter_room_with(s, rx);
    }
    if(atomic_load(&s->end_requested) && gap_ms >= PBL_EXIT_GAP_MS) {
        return pbl_enter_room_with(s, rx);
    }
    if(rx == 0x00 && gap_ms >= PBL_EXIT_GAP_MS) s->exit_candidate = true;

    bool armed = s->gb_quiet >= PBL_ARM_QUIET;
    if((rx & 0xF0) == SERIAL_BATTLE_NIBBLE) {
        s->gb_quiet = 0;
        if(armed) {
            uint32_t n = atomic_load(&s->gb_latched);
            s->gb_actions[n & 3] = rx & 0x0F;
            atomic_store(&s->gb_latched, n + 1);
            events |= PBL_EV_GB_ACTION;
            if(s->auto_action != PBL_ACTION_NONE && atomic_load(&s->my_posted) <= n) {
                pbl_battle_post_action(s, s->auto_action);
            }
        }
    } else if(s->gb_quiet < PBL_ARM_QUIET) {
        s->gb_quiet++;
    }

    uint32_t done = atomic_load(&s->my_done);
    if(done < atomic_load(&s->my_posted)) {
        uint8_t nibble = (uint8_t)atomic_load(&s->my_action);
        if(atomic_load(&s->gb_latched) > done) {
            /* master's action for this exchange is in: finish our repeats */
            if(!s->my_sending) {
                s->my_sending = true;
                s->my_repeat = PBL_REPEAT_AFTER_SEEN;
            }
            if(s->my_repeat == 0) {
                s->my_sending = false;
                s->history[done & 15][0] = nibble;
                s->history[done & 15][1] = s->gb_actions[done & 3];
                atomic_store(&s->my_done, done + 1);
                events |= PBL_EV_EXCHANGE_DONE;
            } else {
                s->my_repeat--;
                tx = SERIAL_BATTLE_NIBBLE | nibble;
            }
        } else if(s->gb_quiet >= PBL_ARM_QUIET) {
            /* The master's previous exchange has reached its $00 acks (or
             * this is the first one): anything we send now is first read by
             * its next exchange, which is the one this action belongs to.
             * While it still sends its old $8x it is in .receive and would
             * take ours as a replacement for the previous action. */
            tx = SERIAL_BATTLE_NIBBLE | nibble;
        }
    }

    if(events) pbl_notify(s, events);
    return tx;
}
