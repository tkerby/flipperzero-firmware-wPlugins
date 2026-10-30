#pragma once

/*
 * Serial links between emulated Game Boys and our session.
 *
 * SameBoy calls bit_start with each bit the clock master shifts out and
 * bit_end when the bit completes, where the far side's bit is returned.
 *
 * Session bridge: the far side is PblSession behind a software shift
 * register, the same preload model as gblink on the Flipper: when a byte
 * completes, pbl_on_byte() gets it and its return value is what we shift
 * out during the next transfer.
 *
 * Twin bridge: the far side is another SameBoy GB (external clock), as in
 * SameBoy's own link cable: either GB may be the master at any time.
 */

#include <pkb/session.h>

#include "gbh.h"
#include "trace.h"

typedef struct {
    Gbh* gb;
    PblSession* s;
    Trace* trace;
    uint8_t out_sr; /* our shift register */
    uint8_t out_byte; /* the byte being shifted out (for the trace) */
    uint8_t in_sr;
    int bits;
    bool master_bit;
} SessionBridge;

/* first_tx: what the Flipper preloads before the first transfer ($02). */
void bridge_session_attach(SessionBridge* b, Gbh* gb, PblSession* s, Trace* trace, uint8_t first_tx);

typedef struct {
    Gbh* gb[2];
    Trace* trace; /* entries are (master, other) for whichever GB clocked */
    bool bit[2];
    uint8_t master_sr[2];
    uint8_t slave_sr[2];
    int bits[2];
    uint64_t bytes_by[2]; /* transfers clocked by each GB */
    /* gb[1]'s clock relative to gb[0]'s. Real GBs drift apart, which is what
     * lets WaitForLinkedFriend on one side catch the other listening; two
     * emulated GBs from one state never would. */
    double rate1;
    /* an external-clock byte completed and its Serial handler has not run */
    bool irq_pending[2];
    unsigned lost_irqs[2]; /* serial interrupts the game's VBlank lost (bridge.c) */
} TwinBridge;

void bridge_twin_attach(TwinBridge* b, Gbh* a, Gbh* c, Trace* trace);
/* Run both GBs in lockstep until `gb[lead]` has finished a frame. */
void bridge_twin_run_frame(TwinBridge* b, int lead);
