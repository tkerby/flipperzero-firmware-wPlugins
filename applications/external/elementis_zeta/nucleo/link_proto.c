// IR LINK framing + reliable half-duplex session (contract section 45): see link_proto.h.
#include <string.h>
#include "link_proto.h"

#define TDIFF(a, b) ((int32_t)((uint32_t)(a) - (uint32_t)(b))) // a - b, wrap safe
enum { FL_SYNCED = 1, FL_RESP_DUE = 2, FL_HELLO2_DUE = 4, FL_HAVE_PEER = 8, FL_EVER_RX = 16 };
enum { M_IDLE = 0, M_TX, M_WAIT };
enum { D_LISTEN = 0, D_TX, D_WAIT };

// ---------------------------------------------------------------------------------------------------------------- CRC + frames
uint16_t lk_crc16(const uint8_t* p, uint16_t n) {
    uint16_t c = 0xFFFF;
    while(n--) {
        c ^= (uint16_t)(*p++ << 8);
        for(int i = 0; i < 8; i++) c = (uint16_t)((c & 0x8000) ? (c << 1) ^ 0x1021 : (c << 1));
    }
    return c;
}

uint16_t lk_frame_build(uint8_t* out, uint8_t ctl, uint8_t mt, const uint8_t* payload, uint8_t len) {
    if(len > LK_MAX_PAYLOAD) return 0;
    out[0] = LK_PRE1;
    out[1] = LK_PRE2;
    out[2] = len;
    out[3] = ctl;
    out[4] = mt;
    if(len) memcpy(out + LK_HDR, payload, len);
    const uint16_t c = lk_crc16(out + 2, (uint16_t)(3 + len));
    out[LK_HDR + len] = (uint8_t)(c >> 8);
    out[LK_HDR + len + 1] = (uint8_t)c;
    return (uint16_t)(LK_HDR + len + 2);
}

void lk_rx_reset(LinkRx* r) {
    r->n = 0;
}

static void lkp_shift1(LinkRx* r) {
    if(r->n) {
        memmove(r->b, r->b + 1, (size_t)r->n - 1);
        r->n--;
    }
}

void lk_rx_push(LinkRx* r, uint8_t byte) {
    if(r->n >= LK_FRAME_MAX) lkp_shift1(r); // cannot hold a frame that long: the oldest byte goes
    r->b[r->n++] = byte;
}

bool lk_rx_frame(LinkRx* r, LinkFrame* f) {
    for(;;) {
        while(r->n && r->b[0] != LK_PRE1) lkp_shift1(r);
        if(r->n >= 2 && r->b[1] != LK_PRE2) {
            lkp_shift1(r);
            continue;
        }
        if(r->n >= 3 && r->b[2] > LK_MAX_PAYLOAD) {
            lkp_shift1(r);
            continue;
        }
        if(r->n >= 4 && (r->b[3] & LK_CTL_RESERVED)) {
            lkp_shift1(r);
            continue;
        }
        if(r->n < 3) return false;
        const uint16_t need = (uint16_t)(LK_HDR + r->b[2] + 2);
        if(r->n < need) return false;
        const uint16_t c = lk_crc16(r->b + 2, (uint16_t)(3 + r->b[2]));
        if((uint8_t)(c >> 8) != r->b[need - 2] || (uint8_t)c != r->b[need - 1]) {
            lkp_shift1(r); // a frame may start inside these bytes
            continue;
        }
        f->len = r->b[2];
        f->ctl = r->b[3];
        f->mt = r->b[4];
        memcpy(f->payload, r->b + LK_HDR, f->len);
        r->n = (uint8_t)(r->n - need);
        memmove(r->b, r->b + need, r->n);
        return true;
    }
}

// ---------------------------------------------------------------------------------------------------------------- session
static inline uint32_t lkp_now(const LinkSess* s) {
    return s->ch->now_ms(s->ch->ctx);
}
static uint32_t lkp_rnd(LinkSess* s) {
    return s->ch->rand32(s->ch->ctx);
}
static void lkp_put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t lkp_get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void lk_init(LinkSess* s, const LinkChan* ch) {
    memset(s, 0, sizeof(*s));
    s->ch = ch;
    s->t = (LinkTiming){LK_T_FIRST_MS, LK_T_BYTE_MS, LK_T_RESP_MAX_MS, LK_POLL_MS, LK_DEAD_MS, LK_HELLO_WAIT_MS, LK_RETRY_MAX, LK_DISC_MS};
    s->state = LS_DISC;
    s->nonce = lkp_rnd(s) | 1u;
    s->t_start = s->last_rx = lkp_now(s);
    s->t_next = s->t_start + 80u + lkp_rnd(s) % 260u;
}

LinkState lk_state(const LinkSess* s) {
    return (LinkState)s->state;
}
bool lk_is_master(const LinkSess* s) {
    return s->state == LS_MASTER;
}
bool lk_established(const LinkSess* s) {
    return (s->state == LS_MASTER || s->state == LS_SLAVE) && (s->flags & FL_SYNCED);
}
bool lk_can_send(const LinkSess* s) {
    return lk_established(s) && !s->tx_pending;
}
bool lk_send(LinkSess* s, uint8_t type, const void* payload, uint8_t len) {
    if(!lk_can_send(s) || type < LK_MT_APP || len > LK_MAX_PAYLOAD) return false;
    s->out_type = type;
    s->out_len = len;
    if(len) memcpy(s->out, payload, len);
    s->tx_pending = true;
    return true;
}
bool lk_recv(LinkSess* s, uint8_t* type, void* buf, uint8_t* len) {
    if(!s->in_ready) return false;
    *type = s->in_type;
    *len = s->in_len;
    if(s->in_len) memcpy(buf, s->in, s->in_len);
    s->in_ready = false;
    return true;
}
bool lk_flushed(const LinkSess* s) {
    return !s->tx_pending && !(s->flags & FL_RESP_DUE) && !s->ch->tx_busy(s->ch->ctx);
}
void lk_close(LinkSess* s) {
    s->state = LS_CLOSED;
}

static void lkp_fail(LinkSess* s, uint8_t why) {
    s->state = LS_FAILED;
    s->fail = why;
}

// Sends the frame in txb (built by the callers); counts it. False when the channel is busy (the caller tries again later).
static bool lkp_send_built(LinkSess* s) {
    if(!s->ch->tx(s->ch->ctx, s->txb, s->txn)) return false;
    s->st.frames_tx++;
    s->st.bytes_tx += s->txn;
    return true;
}

// A session frame (REQ from the master / RESP from the slave) carrying our pending message, if any, and our acknowledgement.
static bool lkp_send_data(LinkSess* s) {
    uint8_t ctl = (uint8_t)(((s->rx_exp & 1) ^ 1) ? LK_CTL_ACKSEQ : 0);
    if(s->state == LS_MASTER) ctl |= LK_CTL_MASTER;
    uint8_t mt = LK_MT_NONE, len = 0;
    if(s->tx_pending) {
        ctl |= LK_CTL_MSG | (s->tx_seq & 1 ? LK_CTL_TXSEQ : 0);
        mt = s->out_type;
        len = s->out_len;
    }
    s->txn = lk_frame_build(s->txb, ctl, mt, s->out, len);
    return lkp_send_built(s);
}

static bool lkp_send_hello(LinkSess* s) {
    uint8_t p[5];
    lkp_put32(p, s->nonce);
    p[4] = LK_PROTO_VER;
    s->txn = lk_frame_build(s->txb, LK_CTL_CTRL, LK_MT_HELLO, p, 5);
    return lkp_send_built(s);
}

static bool lkp_send_hello2(LinkSess* s) {
    uint8_t p[9];
    lkp_put32(p, s->nonce);
    lkp_put32(p + 4, s->peer_nonce);
    p[8] = LK_PROTO_VER;
    s->txn = lk_frame_build(s->txb, LK_CTL_CTRL, LK_MT_HELLO2, p, 9);
    return lkp_send_built(s);
}

// Both nonces are known: the higher one is the master (the same answer on both sides, whoever heard whom first).
static void lkp_settle(LinkSess* s) {
    s->flags |= FL_HAVE_PEER;
    if(s->state != LS_DISC) return;
    if(s->nonce > s->peer_nonce) {
        s->state = LS_MASTER;
        s->m_state = M_IDLE;
        s->t_next = lkp_now(s);
        s->last_rx = lkp_now(s);
        uint8_t p[8];
        lkp_put32(p, s->nonce);
        lkp_put32(p + 4, s->peer_nonce);
        s->out_type = LK_MT_SYNC; // the master's first message: the slave learns the role from it even if every HELLO2 was lost
        s->out_len = 8;
        memcpy(s->out, p, 8);
        s->tx_pending = true;
    } else {
        s->state = LS_SLAVE;
        s->last_rx = lkp_now(s);
    }
}

// An acknowledgement / message carried by a session frame.
static void lkp_on_data(LinkSess* s, const LinkFrame* f) {
    s->last_rx = lkp_now(s);
    s->flags |= FL_EVER_RX;
    if(s->tx_pending && (((f->ctl & LK_CTL_ACKSEQ) != 0) == ((s->tx_seq & 1) != 0))) {
        s->tx_pending = false;
        s->tx_seq ^= 1;
        s->st.msgs_tx++;
        if(s->out_type == LK_MT_SYNC) s->flags |= FL_SYNCED;
    }
    s->peer_msg = (f->ctl & LK_CTL_MSG) != 0;
    if(!s->peer_msg) return;
    if(((f->ctl & LK_CTL_TXSEQ) != 0) != ((s->rx_exp & 1) != 0)) { // not the sequence bit we expect: the peer did not get our ACK; it is acknowledged again, not delivered
        s->st.dup++;
        return;
    }
    if(f->mt == LK_MT_SYNC) {
        s->rx_exp ^= 1;
        s->flags |= FL_SYNCED;
    } else if(!s->in_ready && (s->flags & FL_SYNCED)) {
        s->in_type = f->mt;
        s->in_len = f->len;
        memcpy(s->in, f->payload, f->len);
        s->in_ready = true;
        s->rx_exp ^= 1;
        s->st.msgs_rx++;
    } // else: the application has not read the previous message yet: not accepted, not acknowledged (the peer repeats it)
}

static void lkp_on_frame(LinkSess* s, const LinkFrame* f) {
    s->st.frames_rx++;
    if(f->ctl & LK_CTL_CTRL) { // discovery
        if(f->mt == LK_MT_HELLO && f->len == 5 && s->state != LS_MASTER && !(s->flags & FL_SYNCED)) {
            if(f->payload[4] != LK_PROTO_VER) return lkp_fail(s, LF_VERSION);
            const uint32_t n2 = lkp_get32(f->payload);
            if(n2 == s->nonce) { // a tie (1 in 4 billion): draw again
                s->nonce = lkp_rnd(s) | 1u;
                return;
            }
            s->peer_nonce = n2;
            s->flags |= FL_HELLO2_DUE;
            lkp_settle(s);
        } else if(f->mt == LK_MT_HELLO2 && f->len == 9 && s->state == LS_DISC) {
            if(f->payload[8] != LK_PROTO_VER) return lkp_fail(s, LF_VERSION);
            if(lkp_get32(f->payload + 4) != s->nonce || lkp_get32(f->payload) == s->nonce) return; // a reply to somebody else's HELLO
            s->peer_nonce = lkp_get32(f->payload);
            lkp_settle(s);
        }
        return;
    }
    const bool from_master = (f->ctl & LK_CTL_MASTER) != 0;
    if(s->state == LS_DISC && from_master && (f->ctl & LK_CTL_MSG) && f->mt == LK_MT_SYNC && f->len == 8 && lkp_get32(f->payload + 4) == s->nonce && lkp_get32(f->payload) > s->nonce) {
        s->peer_nonce = lkp_get32(f->payload); // the master's SYNC reached a Flipper that never heard the HELLO2
        s->flags |= FL_HAVE_PEER;
        s->state = LS_SLAVE;
        s->last_rx = lkp_now(s);
    }
    if(s->state == LS_SLAVE && from_master) {
        lkp_on_data(s, f);
        s->flags |= FL_RESP_DUE;
    } else if(s->state == LS_MASTER && !from_master) {
        lkp_on_data(s, f);
        if(s->m_state == M_WAIT) {
            s->m_state = M_IDLE;
            s->m_fail = 0;
            s->t_next = lkp_now(s) + ((s->peer_msg || s->tx_pending || s->in_ready) ? 0u : s->t.poll);
        }
    }
}

void lk_poll(LinkSess* s) {
    if(s->state >= LS_CLOSED) return;
    const uint32_t now = lkp_now(s);
    int b;
    while((b = s->ch->rx(s->ch->ctx)) >= 0) {
        lk_rx_push(&s->rx, (uint8_t)b);
        s->t_lastbyte = now;
        s->st.bytes_rx++;
        LinkFrame f;
        while(s->state < LS_CLOSED && lk_rx_frame(&s->rx, &f)) lkp_on_frame(s, &f);
    }
    if(s->state >= LS_CLOSED) return;
    // bytes that keep not completing a frame are dropped after a quiet moment (a stuck partial frame would hide the start of the next one)
    const bool partial = s->rx.n > 0;
    if(partial && TDIFF(now, s->t_lastbyte) > s->t.t_byte) lk_rx_reset(&s->rx);
    const bool busy = s->ch->tx_busy(s->ch->ctx);

    switch(s->state) {
    case LS_DISC:
        if(TDIFF(now, s->t_start) > (int32_t)s->t.disc) return lkp_fail(s, LF_NOPARTNER);
        if(s->flags & FL_HELLO2_DUE) { // a HELLO was heard: answer at once (before our own HELLO)
            if(!busy && lkp_send_hello2(s)) s->flags &= (uint8_t)~FL_HELLO2_DUE;
            break;
        }
        if(s->d_phase == D_LISTEN && TDIFF(now, s->t_next) >= 0 && !busy) {
            if(lkp_send_hello(s)) s->d_phase = D_TX;
        } else if(s->d_phase == D_TX && !busy) {
            s->d_phase = D_WAIT;
            s->t_next = now + s->t.hello_wait;
        } else if(s->d_phase == D_WAIT && TDIFF(now, s->t_next) >= 0) {
            s->d_phase = D_LISTEN;
            s->t_next = now + 80u + lkp_rnd(s) % 260u;
        }
        break;
    case LS_SLAVE:
        if(s->flags & FL_HELLO2_DUE) { // the master is still searching: it needs our nonce
            if(!busy && lkp_send_hello2(s)) s->flags &= (uint8_t)~FL_HELLO2_DUE;
            break;
        }
        if((s->flags & FL_RESP_DUE) && !busy && lkp_send_data(s)) s->flags &= (uint8_t)~FL_RESP_DUE;
        if(TDIFF(now, s->last_rx) > (int32_t)s->t.dead) lkp_fail(s, (s->flags & FL_SYNCED) ? LF_TIMEOUT : LF_NOPARTNER);
        break;
    case LS_MASTER:
        if((s->flags & FL_HELLO2_DUE) && s->m_state == M_IDLE) { // the HELLO that made us master is answered first (the slave learns our nonce; the SYNC follows)
            if(!busy && lkp_send_hello2(s)) s->flags &= (uint8_t)~FL_HELLO2_DUE;
            break;
        }
        if(s->m_state == M_IDLE) {
            if(TDIFF(now, s->t_next) >= 0 && !busy) {
                if(s->m_fail) s->st.resend++;
                if(lkp_send_data(s)) s->m_state = M_TX;
            }
        } else if(s->m_state == M_TX) {
            if(!busy) {
                s->m_state = M_WAIT;
                s->m_deadline = now + s->t.t_first;
                s->m_hard = now + s->t.t_resp_max;
            }
        } else if(s->m_state == M_WAIT) {
            const bool receiving = s->rx.n > 0 && TDIFF(now, s->t_lastbyte) <= (int32_t)s->t.t_byte;
            if(TDIFF(now, s->m_deadline) >= 0 && (!receiving || TDIFF(now, s->m_hard) >= 0)) { // silence (or a response that never completed)
                s->m_fail++;
                s->st.silent++;
                lk_rx_reset(&s->rx);
                if(s->m_fail > s->t.retry_max || TDIFF(now, s->last_rx) > (int32_t)s->t.dead) return lkp_fail(s, (s->flags & FL_SYNCED) ? LF_TIMEOUT : LF_NOPARTNER);
                s->m_state = M_IDLE;
                s->t_next = now + 10u + lkp_rnd(s) % 50u; // a little jitter so two retries do not keep colliding with the same interference
            }
        }
        break;
    default: break;
    }
}
