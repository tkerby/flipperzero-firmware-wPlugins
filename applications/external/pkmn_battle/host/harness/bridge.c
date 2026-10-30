#include "bridge.h"

#include <Core/gb.h>
#include <string.h>

/* ---- GB (master) <-> PblSession ---- */

static void session_bit_start(GB_gameboy_t* gb, bool bit) {
    Gbh* h = GB_get_user_data(gb);
    SessionBridge* b = h->link;
    b->master_bit = bit;
}

static bool session_bit_end(GB_gameboy_t* gb) {
    Gbh* h = GB_get_user_data(gb);
    SessionBridge* b = h->link;
    bool out = b->out_sr & 0x80;
    b->out_sr = (uint8_t)(b->out_sr << 1);
    b->in_sr = (uint8_t)(b->in_sr << 1 | b->master_bit);
    if(++b->bits == 8) {
        b->bits = 0;
        if(b->trace) trace_add(b->trace, h->time, b->in_sr, b->out_byte, 0);
        b->out_byte = b->out_sr = pbl_on_byte(b->s, b->in_sr, gbh_ms(h));
    }
    return out;
}

void bridge_session_attach(
    SessionBridge* b,
    Gbh* gb,
    PblSession* s,
    Trace* trace,
    uint8_t first_tx) {
    memset(b, 0, sizeof(*b));
    b->gb = gb;
    b->s = s;
    b->trace = trace;
    b->out_sr = b->out_byte = first_tx;
    gb->link = b;
    GB_set_serial_transfer_bit_start_callback(gb->gb, session_bit_start);
    GB_set_serial_transfer_bit_end_callback(gb->gb, session_bit_end);
}

/* ---- GB <-> GB ---- */

static int twin_index(TwinBridge* b, GB_gameboy_t* gb) {
    return b->gb[0]->gb == gb ? 0 : 1;
}

static void twin_bit_start(GB_gameboy_t* gb, bool bit) {
    Gbh* h = GB_get_user_data(gb);
    TwinBridge* b = h->link;
    b->bit[twin_index(b, gb)] = bit;
}

static bool twin_bit_end(GB_gameboy_t* gb) {
    Gbh* h = GB_get_user_data(gb);
    TwinBridge* b = h->link;
    int me = twin_index(b, gb);
    GB_gameboy_t* other = b->gb[!me]->gb;
    bool in = GB_serial_get_data_bit(other);
    bool armed = gbh_peek(b->gb[!me], 0, 0xFF02) & 0x80;
    GB_serial_set_data_bit(other, b->bit[me]);
    if(armed && !(gbh_peek(b->gb[!me], 0, 0xFF02) & 0x80))
        b->irq_pending[!me] = true; /* its byte completed */
    b->master_sr[me] = (uint8_t)(b->master_sr[me] << 1 | b->bit[me]);
    b->slave_sr[me] = (uint8_t)(b->slave_sr[me] << 1 | in);
    if(++b->bits[me] == 8) {
        b->bits[me] = 0;
        b->bytes_by[me]++;
        if(b->trace) trace_add(b->trace, h->time, b->master_sr[me], b->slave_sr[me], (uint8_t)me);
    }
    return in;
}

/* The game's VBlank handlers read rIF, then clear it and write back what
 * they read (VBlank_CutsceneCGB, around _UpdateSound). An external-clock
 * byte completing in between loses its serial interrupt, so the Serial
 * handler never re-arms the transfer and that GB waits forever ("Waiting...!").
 * This race exists in the game itself; a master restarts its transfer every
 * frame and recovers, a slave does not. Two emulated GBs drifting slowly
 * through each other's frames hit it in about one battle in 70. We request
 * the lost interrupt again once VBlank is done (GameTimer, still with
 * interrupts off), as if the byte had completed a few cycles later. */
static void twin_hook_serial(Gbh* h, void* ctx) {
    TwinBridge* b = ctx;
    b->irq_pending[b->gb[0] == h ? 0 : 1] = false;
}

static void twin_hook_vblank_done(Gbh* h, void* ctx) {
    TwinBridge* b = ctx;
    int i = b->gb[0] == h ? 0 : 1;
    uint8_t iflag = gbh_peek(h, 0, 0xFF0F);
    if(!b->irq_pending[i] || (iflag & 0x08)) return;
    gbh_poke(h, 0, 0xFF0F, (uint8_t)(iflag | 0x08));
    b->lost_irqs[i]++;
}

void bridge_twin_attach(TwinBridge* b, Gbh* a, Gbh* c, Trace* trace) {
    memset(b, 0, sizeof(*b));
    b->gb[0] = a;
    b->gb[1] = c;
    b->trace = trace;
    b->rate1 = 1.0;
    for(int i = 0; i < 2; i++) {
        b->gb[i]->link = b;
        GB_set_serial_transfer_bit_start_callback(b->gb[i]->gb, twin_bit_start);
        GB_set_serial_transfer_bit_end_callback(b->gb[i]->gb, twin_bit_end);
        gbh_hook(b->gb[i], "Serial", twin_hook_serial, b);
        gbh_hook(b->gb[i], "GameTimer", twin_hook_vblank_done, b);
    }
}

void bridge_twin_run_frame(TwinBridge* b, int lead) {
    uint64_t f = b->gb[lead]->frames;
    while(b->gb[lead]->frames == f) {
        /* gb[1] covers time1 ticks of its own in time1 * rate1 of ours */
        Gbh* next = (double)b->gb[0]->time <= (double)b->gb[1]->time * b->rate1 ? b->gb[0] :
                                                                                  b->gb[1];
        gbh_step(next);
    }
}
