#include "nb_ccid.h"
#include <string.h>

/* CCID message types */
#define PC_ICC_POWER_ON     0x62
#define PC_ICC_POWER_OFF    0x63
#define PC_GET_SLOT_STATUS  0x65
#define PC_XFR_BLOCK        0x6F
#define PC_GET_PARAMETERS   0x6C
#define PC_RESET_PARAMETERS 0x6D
#define PC_SET_PARAMETERS   0x61
#define PC_ESCAPE           0x6B
#define PC_ICC_CLOCK        0x6E
#define PC_ABORT            0x72

#define RDR_DATA_BLOCK  0x80
#define RDR_SLOT_STATUS 0x81
#define RDR_PARAMETERS  0x82
#define RDR_ESCAPE      0x83

#define ICC_ACTIVE   0x00
#define ICC_INACTIVE 0x01
#define ICC_ABSENT   0x02
#define CMD_FAILED   0x40

#define ERR_CMD_NOT_SUPPORTED 0x00
#define ERR_BAD_SLOT          0x05
#define ERR_ICC_MUTE          0xFE
#define ERR_BAD_LENGTH        0x01
#define ERR_BUSY_WITH_AUTO    0xF2

void nb_ccid_init(NbCcid* c, const NbBackend* be, Pn532* pn) {
    memset(c, 0, sizeof(*c));
    c->be = be;
    c->pn = pn;
}

/* --------------------------------------------------------------------- ATR */

size_t nb_ccid_make_atr(const NbTag* t, uint8_t* atr, size_t max) {
    if(max < 24) return 0;
    size_t n = 0;
    atr[n++] = 0x3B;

    if(t->ats_len >= 1 && nb_tag_type(t) == NbTagIso4a) {
        /* ISO14443-4A: historical bytes of the ATS become the ATR historical bytes */
        const uint8_t* ats = t->ats;
        size_t idx = 2;
        if(t->ats_len >= 2) {
            if(ats[1] & 0x10) idx++; /* TA(1) */
            if(ats[1] & 0x20) idx++; /* TB(1) */
            if(ats[1] & 0x40) idx++; /* TC(1) */
        }
        size_t k = (t->ats_len > idx) ? (size_t)(t->ats_len - idx) : 0;
        if(k > 15) k = 15;
        atr[n++] = (uint8_t)(0x80 | k); /* T0: TD1 follows, k historical bytes */
        atr[n++] = 0x80; /* TD1: TD2 follows, protocol T=0 */
        atr[n++] = 0x01; /* TD2: protocol T=1 */
        if(k) memcpy(atr + n, ats + idx, k);
        n += k;
    } else {
        /* ISO14443-3 card: synthesise the PC/SC part 3 contactless ATR */
        static const uint8_t head[] = {
            0x8F, 0x80, 0x01, 0x80, 0x4F, 0x0C, 0xA0, 0x00, 0x00, 0x03, 0x06};
        memcpy(atr + n, head, sizeof(head));
        n += sizeof(head);
        uint16_t nn = 0;
        switch(nb_tag_type(t)) {
        case NbTagMfc1k:
            nn = 0x0001;
            break;
        case NbTagMfc4k:
            nn = 0x0002;
            break;
        case NbTagUltralight:
            nn = 0x0003;
            break;
        case NbTagMfcMini:
            nn = 0x0026;
            break;
        default:
            break;
        }
        atr[n++] = 0x03; /* standard: ISO 14443 A, part 3 */
        atr[n++] = (uint8_t)(nn >> 8); /* card name (2 bytes, high first) */
        atr[n++] = (uint8_t)nn;
        atr[n++] = 0x00; /* RFU */
        atr[n++] = 0x00;
        atr[n++] = 0x00;
        atr[n++] = 0x00;
    }
    uint8_t tck = 0;
    for(size_t i = 1; i < n; i++)
        tck ^= atr[i];
    atr[n++] = tck;
    return n;
}

/* --------------------------------------------------------- response helpers */

static size_t put_hdr(
    uint8_t* out,
    uint8_t type,
    uint32_t dlen,
    uint8_t slot,
    uint8_t seq,
    uint8_t status,
    uint8_t err,
    uint8_t spec) {
    out[0] = type;
    out[1] = (uint8_t)dlen;
    out[2] = (uint8_t)(dlen >> 8);
    out[3] = (uint8_t)(dlen >> 16);
    out[4] = (uint8_t)(dlen >> 24);
    out[5] = slot;
    out[6] = seq;
    out[7] = status;
    out[8] = err;
    out[9] = spec;
    return NB_CCID_HDR;
}

static uint8_t icc_state(const NbCcid* c) {
    if(!c->present) return ICC_ABSENT;
    return c->powered ? ICC_ACTIVE : ICC_INACTIVE;
}

static size_t reply_status(
    NbCcid* c,
    uint8_t type,
    uint8_t slot,
    uint8_t seq,
    bool ok,
    uint8_t err,
    uint8_t* out) {
    return put_hdr(
        out, type, 0, slot, seq, (uint8_t)(icc_state(c) | (ok ? 0 : CMD_FAILED)), ok ? 0 : err, 0);
}

/* ---------------------------------------------------------------- the APDUs */

static uint16_t sector_of(uint8_t block) {
    return block < 128 ? (uint16_t)(block / 4) : (uint16_t)(32 + (block - 128) / 16);
}

static int sw(uint8_t* r, uint8_t sw1, uint8_t sw2, size_t data_len) {
    r[data_len] = sw1;
    r[data_len + 1] = sw2;
    return (int)data_len + 2;
}

/* ATS historical bytes, for FF CA 01 */
static size_t ats_hist(const NbTag* t, const uint8_t** p) {
    if(t->ats_len < 2) return 0;
    size_t idx = 2;
    if(t->ats[1] & 0x10) idx++;
    if(t->ats[1] & 0x20) idx++;
    if(t->ats[1] & 0x40) idx++;
    if(t->ats_len <= idx) return 0;
    *p = t->ats + idx;
    return t->ats_len - idx;
}

/* Returns response length (data + SW1 SW2), or negative NB_ERR_* if the card did not answer. */
static int do_apdu(NbCcid* c, const uint8_t* a, size_t n, uint8_t* r, size_t max) {
    const NbBackend* be = c->be;
    if(n < 4 || max < 20) return sw(r, 0x67, 0x00, 0);
    NbTagType ty = nb_tag_type(&c->tag);

    if(a[0] != 0xFF) {
        if(ty == NbTagIso4a) {
            int k = be->apdu(be->ctx, a, n, r, max);
            return k;
        }
        return sw(r, 0x6E, 0x00, 0); /* class not supported: not an ISO7816 card */
    }

    uint8_t ins = a[1], p1 = a[2], p2 = a[3];
    size_t le = (n == 5) ? (a[4] ? a[4] : 256) : 0; /* case 2: Le */

    switch(ins) {
    case 0xCA: { /* get data */
        const uint8_t* d = NULL;
        size_t dl = 0;
        if(p1 == 0x00) {
            d = c->tag.uid;
            dl = c->tag.uid_len;
        } else if(p1 == 0x01) {
            dl = ats_hist(&c->tag, &d);
        } else {
            return sw(r, 0x6A, 0x81, 0);
        }
        if(le && le < dl) return sw(r, 0x6C, (uint8_t)dl, 0);
        if(dl) memcpy(r, d, dl);
        return sw(r, 0x90, 0x00, dl);
    }

    case 0x82: { /* load authentication key */
        if(n != 11 || a[4] != 6 || p2 > 3) return sw(r, 0x67, 0x00, 0);
        memcpy(c->keys[p2], a + 5, 6);
        c->key_set[p2] = true;
        return sw(r, 0x90, 0x00, 0);
    }

    case 0x86: { /* general authenticate: 01 00 block keytype keynum */
        if(n != 10 || a[4] != 5 || a[5] != 0x01) return sw(r, 0x67, 0x00, 0);
        uint8_t blk = a[7], kt = a[8], kn = a[9];
        if((kt != 0x60 && kt != 0x61) || kn > 3 || !c->key_set[kn]) return sw(r, 0x69, 0x82, 0);
        if(ty != NbTagMfc1k && ty != NbTagMfc4k && ty != NbTagMfcMini) return sw(r, 0x6A, 0x81, 0);
        int k = be->mfc_auth(be->ctx, blk, kt == 0x61, c->keys[kn]);
        if(k != NB_OK) {
            c->auth_valid = false;
            return sw(r, 0x63, 0x00, 0);
        }
        c->auth_valid = true;
        c->auth_key_b = (kt == 0x61);
        memcpy(c->auth_key, c->keys[kn], 6);
        c->auth_sector = sector_of(blk);
        return sw(r, 0x90, 0x00, 0);
    }

    case 0xB0: { /* read binary: P2 = block / page */
        if(p1 != 0x00 || n != 5) return sw(r, 0x6A, 0x81, 0);
        size_t want = a[4] ? a[4] : 16;
        if(want > 16) return sw(r, 0x67, 0x00, 0);
        uint8_t buf[16];
        if(ty == NbTagUltralight) {
            if(be->mful_read(be->ctx, p2, buf) != NB_OK) return sw(r, 0x63, 0x00, 0);
        } else if(ty == NbTagMfc1k || ty == NbTagMfc4k || ty == NbTagMfcMini) {
            if(!c->auth_valid || c->auth_sector != sector_of(p2)) return sw(r, 0x69, 0x82, 0);
            if(be->mfc_read(be->ctx, p2, c->auth_key_b, c->auth_key, buf) != NB_OK)
                return sw(r, 0x63, 0x00, 0);
        } else {
            return sw(r, 0x6A, 0x81, 0);
        }
        memcpy(r, buf, want);
        return sw(r, 0x90, 0x00, want);
    }

    case 0xD6: { /* update binary */
        if(p1 != 0x00 || n < 6) return sw(r, 0x6A, 0x81, 0);
        size_t lc = a[4];
        if(n != 5 + lc) return sw(r, 0x67, 0x00, 0);
        const uint8_t* d = a + 5;
        if(ty == NbTagUltralight) {
            if(lc == 0 || lc % 4 != 0 || lc > 16) return sw(r, 0x67, 0x00, 0);
            for(size_t i = 0; i < lc / 4; i++)
                if(be->mful_write(be->ctx, (uint8_t)(p2 + i), d + 4 * i) != NB_OK)
                    return sw(r, 0x63, 0x00, 0);
            return sw(r, 0x90, 0x00, 0);
        }
        if(ty == NbTagMfc1k || ty == NbTagMfc4k || ty == NbTagMfcMini) {
            if(lc != 16) return sw(r, 0x67, 0x00, 0);
            if(!c->auth_valid || c->auth_sector != sector_of(p2)) return sw(r, 0x69, 0x82, 0);
            if(be->mfc_write(be->ctx, p2, c->auth_key_b, c->auth_key, d) != NB_OK)
                return sw(r, 0x63, 0x00, 0);
            return sw(r, 0x90, 0x00, 0);
        }
        return sw(r, 0x6A, 0x81, 0);
    }

    case 0x00: { /* ACR122-style pass-through of a PN532 command: FF 00 00 00 Lc D4 ... */
        if(!c->pn || p1 != 0 || p2 != 0 || n < 6 || a[5] != 0xD4 || n != 5 + (size_t)a[4])
            return sw(r, 0x6A, 0x81, 0);
        uint8_t rsp[NB_PN532_RSP_MAX];
        size_t k = pn532_exec(c->pn, a + 6, n - 6, rsp, sizeof(rsp));
        if(k == 0 || k + 3 > max) return sw(r, 0x63, 0x00, 0);
        r[0] = 0xD5;
        memcpy(r + 1, rsp, k);
        return sw(r, 0x90, 0x00, k + 1);
    }

    default:
        return sw(r, 0x6A, 0x81, 0);
    }
}

/* PPS request (ISO 7816-3 9.2): FF PPS0 [PPS1] [PPS2] [PPS3] PCK.  libccid sends one right after
 * power-on to pick T=0/T=1 unless the reader says it does PPS itself.  A contactless card has no
 * such step, so we just echo the request back, which is the "PPS accepted" answer. */
static bool is_pps_request(const uint8_t* d, size_t n) {
    if(n < 3 || n > 6 || d[0] != 0xFF || (d[1] & 0x80) || (d[1] & 0x0F) > 1) return false;
    size_t want = 3;
    for(int bit = 4; bit <= 6; bit++)
        if(d[1] & (1u << bit)) want++;
    if(n != want) return false;
    uint8_t x = 0;
    for(size_t i = 0; i < n; i++)
        x ^= d[i];
    return x == 0;
}

/* -------------------------------------------------------------- main entry */

size_t nb_ccid_process(NbCcid* c, const uint8_t* m, size_t len, uint8_t* out, size_t out_max) {
    if(len < NB_CCID_HDR || out_max < NB_CCID_HDR + 32) return 0;
    uint8_t type = m[0];
    uint32_t dlen = (uint32_t)m[1] | ((uint32_t)m[2] << 8) | ((uint32_t)m[3] << 16) |
                    ((uint32_t)m[4] << 24);
    uint8_t slot = m[5], seq = m[6];
    if(dlen > NB_CCID_MSG_MAX || len < NB_CCID_HDR + dlen) return 0;
    const uint8_t* data = m + NB_CCID_HDR;

    uint8_t rtype;
    switch(type) {
    case PC_ICC_POWER_ON:
    case PC_XFR_BLOCK:
        rtype = RDR_DATA_BLOCK;
        break;
    case PC_ICC_POWER_OFF:
    case PC_GET_SLOT_STATUS:
    case PC_ICC_CLOCK:
    case PC_ABORT:
        rtype = RDR_SLOT_STATUS;
        break;
    case PC_GET_PARAMETERS:
    case PC_RESET_PARAMETERS:
    case PC_SET_PARAMETERS:
        rtype = RDR_PARAMETERS;
        break;
    case PC_ESCAPE:
        rtype = RDR_ESCAPE;
        break;
    default:
        return reply_status(c, RDR_SLOT_STATUS, slot, seq, false, ERR_CMD_NOT_SUPPORTED, out);
    }
    if(slot != 0) return reply_status(c, rtype, slot, seq, false, ERR_BAD_SLOT, out);

    switch(type) {
    case PC_ICC_POWER_ON: {
        NbTag t;
        int r = c->be->detect(c->be->ctx, &t);
        if(r != 1) {
            c->present = false;
            c->powered = false;
            return reply_status(c, rtype, slot, seq, false, ERR_ICC_MUTE, out);
        }
        c->tag = t;
        c->present = true;
        c->powered = true;
        c->auth_valid = false;
        uint8_t atr[32];
        size_t n = nb_ccid_make_atr(&t, atr, sizeof(atr));
        size_t h = put_hdr(out, rtype, (uint32_t)n, slot, seq, ICC_ACTIVE, 0, 0);
        memcpy(out + h, atr, n);
        return h + n;
    }

    case PC_ICC_POWER_OFF:
        if(c->powered && c->be->release) c->be->release(c->be->ctx);
        c->powered = false;
        c->auth_valid = false;
        return reply_status(c, rtype, slot, seq, true, 0, out);

    case PC_GET_SLOT_STATUS:
        return reply_status(c, rtype, slot, seq, true, 0, out);

    case PC_ICC_CLOCK:
    case PC_ABORT:
        return reply_status(c, rtype, slot, seq, true, 0, out);

    case PC_XFR_BLOCK: {
        if(!c->present || !c->powered)
            return reply_status(c, rtype, slot, seq, false, ERR_ICC_MUTE, out);
        if(is_pps_request(data, dlen)) {
            put_hdr(out, rtype, dlen, slot, seq, ICC_ACTIVE, 0, 0);
            memcpy(out + NB_CCID_HDR, data, dlen);
            return NB_CCID_HDR + dlen;
        }
        if(dlen < 4) return reply_status(c, rtype, slot, seq, false, ERR_BAD_LENGTH, out);
        size_t room = out_max - NB_CCID_HDR;
        uint8_t* r = out + NB_CCID_HDR;
        int k = do_apdu(c, data, dlen, r, room > 270 ? 270 : room);
        if(k < 0) {
            c->powered = false; /* card gone or stopped answering: host must power on again */
            c->present = false;
            return reply_status(c, rtype, slot, seq, false, ERR_ICC_MUTE, out);
        }
        put_hdr(out, rtype, (uint32_t)k, slot, seq, ICC_ACTIVE, 0, 0);
        return NB_CCID_HDR + (size_t)k;
    }

    case PC_GET_PARAMETERS:
    case PC_RESET_PARAMETERS:
    case PC_SET_PARAMETERS: {
        /* T=1 parameter block; the card layer is handled here, so values are nominal */
        static const uint8_t params[7] = {0x11, 0x10, 0x00, 0x15, 0x00, 0xFE, 0x00};
        size_t h = put_hdr(out, rtype, 7, slot, seq, icc_state(c), 0, 0x01);
        memcpy(out + h, params, 7);
        return h + 7;
    }

    case PC_ESCAPE:
    default:
        return reply_status(c, rtype, slot, seq, false, ERR_CMD_NOT_SUPPORTED, out);
    }
}

int nb_ccid_poll_notify(NbCcid* c, uint8_t note[2]) {
    uint32_t now = c->be->tick_ms ? c->be->tick_ms(c->be->ctx) : 0;
    /* A card the host has powered gets no detect() polling (that would disturb it), so
     * ask the backend, if it can tell, whether the card is still there. */
    if(c->powered && c->be->is_present && !c->be->is_present(c->be->ctx)) {
        c->powered = false;
        c->present = false;
        c->auth_valid = false;
    }
    if(!c->powered) {
        if(!c->reported_valid || (uint32_t)(now - c->last_poll_ms) >= 250) {
            c->last_poll_ms = now;
            NbTag t;
            int r = c->be->detect(c->be->ctx, &t);
            if(r == 1) {
                c->tag = t;
                c->present = true;
            } else {
                c->present = false;
            }
        }
    }
    if(!c->reported_valid || c->reported_present != c->present) {
        c->reported_valid = true;
        c->reported_present = c->present;
        note[0] = 0x50;
        note[1] = c->present ? 0x03 : 0x02; /* bit0 present, bit1 changed */
        return 2;
    }
    return 0;
}
