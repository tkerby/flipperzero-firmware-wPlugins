#pragma once
// IR LINK, framing + reliable half-duplex session (contract section 45). No furi.h (host-compilable): the transport is an abstract BYTE CHANNEL (LinkChan); the
// device implements it over the infrared port (link_ir.c + link_phy.c), the host tests over a simulated wire with noise, drops and delays (tools/host_world/link_check.c).
//
// FRAME (one IR burst):   A5 5A | LEN | CTL | MT | payload[LEN] | CRC16-hi CRC16-lo
//   PRE   A5 5A      two-byte preamble (a receiver re-synchronises on it after any garbage)
//   LEN   0..LK_MAX_PAYLOAD   payload bytes (a larger value is not a frame: the scanner moves on)
//   CTL   bit0 TXSEQ  sequence bit of the message in this frame        bit1 ACKSEQ  sequence bit of the last message the sender ACCEPTED from the peer
//         bit2 HAS_MSG  the frame carries a message (MT + payload)      bit3 FROM_MASTER  the sender is the master
//         bit4 CTRL   a discovery frame (message type 1 = HELLO, 2 = HELLO2), outside the session      bits 5..7 zero (reserved: a frame with one set is dropped)
//   MT    message type (0 when HAS_MSG is clear)
//   CRC16 CCITT-FALSE (poly 0x1021, init 0xFFFF) over LEN CTL MT payload, big endian.   Overhead: 7 bytes. A frame without payload: 7 bytes (~52 ms on the air).
//
// SESSION. Half duplex (an IR port cannot hear while it sends), so the traffic is request / response and only the MASTER ever speaks first:
//   DISCOVERY  both Flippers repeat   listen (random 80..340 ms) -> HELLO{nonce} -> listen 140 ms   until one hears the other. The one that hears a HELLO answers HELLO2{own nonce,
//              echo}; the higher nonce is the MASTER (the "coin toss"), the other the SLAVE. The master's first message is SYNC{master nonce, slave nonce} (a lost HELLO2 is
//              repaired by it: a Flipper still searching that receives a SYNC with its own nonce becomes the slave).
//   DATA       the master sends a REQ frame (its pending message, or none = a poll) every LK_POLL_MS (at once while a message is in flight); the slave answers each REQ with ONE RESP frame
//              (its pending message or none). Messages are delivered with an ALTERNATING-BIT protocol: one message in flight per direction, resent in every frame until the
//              peer's ACKSEQ equals its TXSEQ; a duplicate (same TXSEQ as the last accepted) is acknowledged again and not delivered twice; a message is NOT accepted (so not
//              acknowledged) while the previous one has not been read by the application (flow control). A lost REQ or RESP shows as silence: the master resends after T_FIRST.
//   FAILURE    the master gives up after LK_RETRY_MAX silent exchanges in a row, the slave after LK_DEAD_MS without a valid REQ; discovery gives up after its own timeout.
#include <stdint.h>
#include <stdbool.h>

#define LK_PRE1        0xA5
#define LK_PRE2        0x5A
#define LK_MAX_PAYLOAD 96
#define LK_HDR         5
#define LK_OVERHEAD    7
#define LK_FRAME_MAX   (LK_HDR + LK_MAX_PAYLOAD + 2)
#define LK_PROTO_VER   1

#define LK_CTL_TXSEQ    0x01
#define LK_CTL_ACKSEQ   0x02
#define LK_CTL_MSG      0x04
#define LK_CTL_MASTER   0x08
#define LK_CTL_CTRL     0x10
#define LK_CTL_RESERVED 0xE0

enum {
    LK_MT_NONE = 0,
    LK_MT_HELLO = 1,
    LK_MT_HELLO2 = 2,
    LK_MT_SYNC = 3 /* session-internal */,
    LK_MT_APP = 0x10 /* first application message type */
};

// Default timings (milliseconds). A LinkSess holds a copy (tests shorten them).
#define LK_T_FIRST_MS \
    260 // after a REQ ends: the slave has this long to START answering (its poll period ~33 ms + a turn-around), else the REQ is resent
#define LK_T_BYTE_MS \
    70 // a frame in progress is abandoned after this long without a byte (a symbol lasts at most 2.5 ms, a byte 10 ms)
#define LK_T_RESP_MAX_MS \
    1800 // upper bound of one exchange (a 96-byte payload is 0.8 s on the air)
#define LK_POLL_MS   150 // idle poll period
#define LK_RETRY_MAX 15 // consecutive silent exchanges before the master gives up
#define LK_DEAD_MS \
    7000 // the slave gives up after this long without a valid REQ; the master after this long without a valid RESP
#define LK_DISC_MS       20000 // default discovery timeout
#define LK_HELLO_WAIT_MS 140 // after our HELLO, listen this long for HELLO2

typedef struct LinkChan {
    void* ctx;
    uint32_t (*now_ms)(void* ctx);
    bool (*tx)(
        void* ctx,
        const uint8_t* buf,
        uint16_t
            n); // starts sending a frame (the channel copies it; non-blocking); false = still busy
    bool (*tx_busy)(void* ctx); // true while a frame is going out (the receiver is deaf then)
    int (*rx)(void* ctx); // next received byte, -1 = none
    uint32_t (*rand32)(void* ctx);
} LinkChan;

// ---- CRC and frames (pure functions, tested alone)
uint16_t lk_crc16(const uint8_t* p, uint16_t n);
// Builds a frame into out (>= LK_FRAME_MAX); returns its length (0 when len > LK_MAX_PAYLOAD).
uint16_t
    lk_frame_build(uint8_t* out, uint8_t ctl, uint8_t mt, const uint8_t* payload, uint8_t len);

typedef struct {
    uint8_t b[LK_FRAME_MAX];
    uint8_t n;
} LinkRx;
typedef struct {
    uint8_t ctl, mt, len;
    uint8_t payload
        [LK_MAX_PAYLOAD]; // a copy: the frame is consumed from the LinkRx buffer when it is returned
} LinkFrame;
void lk_rx_reset(LinkRx* r);
void lk_rx_push(LinkRx* r, uint8_t byte);
// Extracts the next valid frame from the bytes held (false = none yet). A bad CRC / length / reserved bit moves the scanner one byte on, so a frame that starts inside
// the garbage (or inside a corrupted frame) is still found.
bool lk_rx_frame(LinkRx* r, LinkFrame* f);

// ---- session
typedef enum {
    LS_DISC = 0,
    LS_MASTER,
    LS_SLAVE,
    LS_CLOSED,
    LS_FAILED
} LinkState;
typedef enum {
    LF_NONE = 0,
    LF_NOPARTNER,
    LF_TIMEOUT,
    LF_VERSION
} LinkFail;

typedef struct {
    uint16_t t_first, t_byte, t_resp_max, poll, dead, hello_wait;
    uint8_t retry_max;
    uint32_t disc;
} LinkTiming;

typedef struct {
    uint32_t frames_tx, frames_rx, bytes_tx, bytes_rx, crc_drop, resend, dup, msgs_tx, msgs_rx,
        silent;
    uint32_t air_us_tx; // sum of the on-air time the channel reported (filled by the channel, tests)
} LinkStats;

typedef struct LinkSess {
    const LinkChan* ch;
    LinkTiming t;
    LinkStats st;
    uint8_t state, fail, d_phase, m_state, m_fail, flags;
    uint32_t nonce, peer_nonce, t_start, t_next, last_rx, m_deadline, m_hard, hello2_at;
    // framing
    LinkRx rx;
    uint32_t t_lastbyte;
    uint8_t txb[LK_FRAME_MAX];
    uint16_t txn;
    // reliable messages (alternating bit)
    bool tx_pending;
    uint8_t tx_seq, rx_exp, out_type, out_len, in_type, in_len;
    bool in_ready, peer_msg;
    uint8_t out[LK_MAX_PAYLOAD], in[LK_MAX_PAYLOAD];
} LinkSess;

void lk_init(
    LinkSess* s,
    const LinkChan*
        ch); // starts DISCOVERY now (t.disc = LK_DISC_MS; change s->t before the first lk_poll if needed)
void lk_poll(
    LinkSess*
        s); // call often (every game frame, <= 33 ms): reads the channel, runs the protocol, writes frames
LinkState lk_state(const LinkSess* s);
bool lk_is_master(const LinkSess* s);
bool lk_established(
    const LinkSess* s); // the session handshake (SYNC) is complete: messages can flow
bool lk_can_send(const LinkSess* s); // established and no message in flight
bool lk_send(
    LinkSess* s,
    uint8_t type,
    const void* payload,
    uint8_t len); // type >= LK_MT_APP; false = cannot now (try again later) or too long
bool lk_recv(
    LinkSess* s,
    uint8_t* type,
    void* buf,
    uint8_t* len); // pops the received message (buf >= LK_MAX_PAYLOAD)
bool lk_flushed(
    const LinkSess*
        s); // nothing of ours is waiting for an acknowledgement and no frame is going out
void lk_close(LinkSess* s); // ends the session now (state CLOSED)
