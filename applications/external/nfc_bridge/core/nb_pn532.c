#include "nb_pn532.h"
#include <string.h>

#define TFI_HOST  0xD4
#define TFI_PN532 0xD5

/* PN532 status codes used in InDataExchange / InCommunicateThru answers */
#define ST_OK          0x00
#define ST_TIMEOUT     0x01
#define ST_AUTH        0x14
#define ST_WRONG_STATE 0x27

static const uint8_t ACK_FRAME[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
static const uint8_t NACK_FRAME[6] = {0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
static const uint8_t ERR_FRAME[8] = {0x00, 0x00, 0xFF, 0x01, 0xFF, 0x7F, 0x81, 0x00};

/* ---------------------------------------------------------------- registers */

static uint8_t reg_get(Pn532* p, uint16_t addr) {
    for(size_t i = 0; i < p->nregs; i++)
        if(p->regs[i].addr == addr) return p->regs[i].val;
    return 0;
}

static void reg_set(Pn532* p, uint16_t addr, uint8_t val) {
    for(size_t i = 0; i < p->nregs; i++) {
        if(p->regs[i].addr == addr) {
            p->regs[i].val = val;
            return;
        }
    }
    if(p->nregs < NB_PN532_REGS_MAX) {
        p->regs[p->nregs].addr = addr;
        p->regs[p->nregs].val = val;
        p->nregs++;
    }
}

static void regs_reset(Pn532* p) {
    p->nregs = 0;
    reg_set(p, 0x6302, 0x80); /* CIU_TxMode: TxCRCEn */
    reg_set(p, 0x6303, 0x80); /* CIU_RxMode: RxCRCEn */
}

void pn532_init(Pn532* p, const NbBackend* be, Pn532WriteFn write, void* io) {
    memset(p, 0, sizeof(*p));
    p->be = be;
    p->write = write;
    p->io = io;
    regs_reset(p);
}

/* ------------------------------------------------------------------ helpers */

static void forget_card(Pn532* p) {
    if(p->tag_valid && p->be->release) p->be->release(p->be->ctx);
    p->tag_valid = false;
    p->auth_valid = false;
}

static uint16_t mfc_sector(uint8_t block) {
    return block < 128 ? (uint16_t)(block / 4) : (uint16_t)(32 + (block - 128) / 16);
}

static int status_from_err(int err) {
    return err == NB_ERR_AUTH ? ST_AUTH : ST_TIMEOUT;
}

/* ----------------------------------------------------------------- commands */

/* Handlers fill out[] with the payload that follows "D5 cmd+1" and return its
 * length, or -1 for "not supported". */

static int cmd_list_passive(Pn532* p, const uint8_t* d, size_t n, uint8_t* out) {
    /* d = MaxTg, BrTy, [InitiatorData...] */
    if(n < 2) return -1;
    uint8_t br_ty = d[1];
    const uint8_t* want = d + 2;
    size_t want_len = n - 2;

    forget_card(p);
    out[0] = 0; /* NbTg */
    if(br_ty != 0x00) return 1; /* only 106 kbit/s type A */

    NbTag t;
    int r = p->be->detect(p->be->ctx, &t);
    if(r != 1) return 1;
    if(want_len > 0) {
        /* libnfc passes (part of) a UID to pick one card */
        size_t cmp = want_len < t.uid_len ? want_len : t.uid_len;
        if(want_len > t.uid_len || memcmp(want, t.uid, cmp) != 0) return 1;
    }
    p->tag = t;
    p->tag_valid = true;

    size_t i = 0;
    out[i++] = 1; /* NbTg */
    out[i++] = 1; /* Tg */
    out[i++] = t.atqa[1]; /* the PN532 reports SENS_RES MSB first */
    out[i++] = t.atqa[0];
    out[i++] = t.sak;
    out[i++] = t.uid_len;
    memcpy(out + i, t.uid, t.uid_len);
    i += t.uid_len;
    if(t.ats_len > 0) {
        memcpy(out + i, t.ats, t.ats_len); /* ats[0] is TL, i.e. the length byte */
        i += t.ats_len;
    }
    return (int)i;
}

static int cmd_data_exchange(Pn532* p, const uint8_t* d, size_t n, uint8_t* out, size_t max) {
    if(n < 1) return -1;
    if(!p->tag_valid || d[0] != 1) {
        out[0] = ST_WRONG_STATE;
        return 1;
    }
    const uint8_t* x = d + 1;
    size_t xn = n - 1;
    const NbBackend* be = p->be;

    switch(nb_tag_type(&p->tag)) {
    case NbTagIso4a: {
        int r = be->apdu(be->ctx, x, xn, out + 1, max - 1);
        if(r < 0) {
            out[0] = ST_TIMEOUT;
            return 1;
        }
        out[0] = ST_OK;
        return 1 + r;
    }

    case NbTagMfcMini:
    case NbTagMfc1k:
    case NbTagMfc4k: {
        if(xn < 2) return -1;
        uint8_t cmd = x[0], blk = x[1];
        if(cmd == 0x60 || cmd == 0x61) { /* auth: cmd blk key[6] uid[4] */
            if(xn < 8) return -1;
            int r = be->mfc_auth(be->ctx, blk, cmd == 0x61, x + 2);
            if(r == NB_OK) {
                p->auth_valid = true;
                p->auth_key_b = (cmd == 0x61);
                memcpy(p->auth_key, x + 2, 6);
                p->auth_sector = mfc_sector(blk);
                out[0] = ST_OK;
            } else {
                p->auth_valid = false;
                out[0] = (uint8_t)status_from_err(r);
            }
            return 1;
        }
        if(cmd == 0x30) { /* read 16 */
            if(!p->auth_valid || p->auth_sector != mfc_sector(blk)) {
                out[0] = ST_AUTH;
                return 1;
            }
            int r = be->mfc_read(be->ctx, blk, p->auth_key_b, p->auth_key, out + 1);
            if(r != NB_OK) {
                out[0] = (uint8_t)status_from_err(r);
                return 1;
            }
            out[0] = ST_OK;
            return 17;
        }
        if(cmd == 0xA0) { /* write 16 */
            if(xn < 18) return -1;
            if(!p->auth_valid || p->auth_sector != mfc_sector(blk)) {
                out[0] = ST_AUTH;
                return 1;
            }
            int r = be->mfc_write(be->ctx, blk, p->auth_key_b, p->auth_key, x + 2);
            out[0] = (r == NB_OK) ? ST_OK : (uint8_t)status_from_err(r);
            return 1;
        }
        out[0] = ST_TIMEOUT; /* value blocks (increment/decrement/...) not implemented */
        return 1;
    }

    case NbTagUltralight: {
        if(xn < 2) return -1;
        uint8_t cmd = x[0], page = x[1];
        if(cmd == 0x30) { /* read: 4 pages */
            int r = be->mful_read(be->ctx, page, out + 1);
            if(r != NB_OK) {
                out[0] = ST_TIMEOUT;
                return 1;
            }
            out[0] = ST_OK;
            return 17;
        }
        if(cmd == 0xA2 && xn >= 6) { /* write one page */
            int r = be->mful_write(be->ctx, page, x + 2);
            out[0] = (r == NB_OK) ? ST_OK : ST_TIMEOUT;
            return 1;
        }
        if(cmd == 0xA0 && xn >= 18) { /* compatibility write: first 4 bytes are the page */
            int r = be->mful_write(be->ctx, page, x + 2);
            out[0] = (r == NB_OK) ? ST_OK : ST_TIMEOUT;
            return 1;
        }
        out[0] = ST_TIMEOUT;
        return 1;
    }

    default:
        out[0] = ST_TIMEOUT;
        return 1;
    }
}

static int cmd_comm_thru(Pn532* p, const uint8_t* d, size_t n, uint8_t* out, size_t max) {
    if(!p->be->raw) {
        out[0] = ST_TIMEOUT;
        return 1;
    }
    size_t tx_bits = n * 8;
    uint8_t last = reg_get(p, 0x633D) & 0x07; /* BitFraming.TxLastBits */
    if(last && n > 0) tx_bits = (n - 1) * 8 + last;
    bool crc_tx = reg_get(p, 0x6302) & 0x80;
    bool crc_rx = reg_get(p, 0x6303) & 0x80;
    size_t rx_bits = 0;
    int r = p->be->raw(p->be->ctx, d, tx_bits, crc_tx, crc_rx, out + 1, max - 1, &rx_bits, 50);
    if(r < 0) {
        out[0] = ST_TIMEOUT;
        return 1;
    }
    out[0] = ST_OK;
    return 1 + r;
}

static int handle(Pn532* p, uint8_t cmd, const uint8_t* d, size_t n, uint8_t* out, size_t max) {
    switch(cmd) {
    case 0x00: { /* Diagnose */
        if(n < 1) return -1;
        if(d[0] == 0x00) { /* communication line test: echo everything back */
            memcpy(out, d, n);
            return (int)n;
        }
        out[0] = 0x00; /* every other self-test: "OK" */
        return 1;
    }
    case 0x02: /* GetFirmwareVersion: PN532 v1.6 */
        out[0] = 0x32;
        out[1] = 0x01;
        out[2] = 0x06;
        out[3] = 0x07;
        return 4;
    case 0x04: { /* GetGeneralStatus */
        size_t i = 0;
        out[i++] = 0x00; /* last error */
        out[i++] = 0x00; /* field */
        out[i++] = p->tag_valid ? 1 : 0;
        if(p->tag_valid) {
            out[i++] = 1;
            out[i++] = 0;
            out[i++] = 0;
            out[i++] = 0;
        }
        out[i++] = 0x80; /* SAM status */
        return (int)i;
    }
    case 0x06: { /* ReadRegister: (hi,lo)* -> val* */
        size_t cnt = n / 2;
        for(size_t i = 0; i < cnt; i++)
            out[i] = reg_get(p, (uint16_t)((d[2 * i] << 8) | d[2 * i + 1]));
        return (int)cnt;
    }
    case 0x08: { /* WriteRegister: (hi,lo,val)* */
        for(size_t i = 0; i + 2 < n; i += 3)
            reg_set(p, (uint16_t)((d[i] << 8) | d[i + 1]), d[i + 2]);
        return 0;
    }
    case 0x0C: /* ReadGPIO */
        out[0] = out[1] = out[2] = 0;
        return 3;
    case 0x0E: /* WriteGPIO */
    case 0x10: /* SetSerialBaudRate (answered, then ignored: the link is USB) */
    case 0x12: /* SetParameters */
    case 0x14: /* SAMConfiguration */
        return 0;
    case 0x16: /* PowerDown */
        out[0] = 0x00;
        return 1;
    case 0x32: /* RFConfiguration */
        if(n >= 2 && d[0] == 0x01 && p->be->field) p->be->field(p->be->ctx, d[1] & 1);
        return 0;
    case 0x4A:
        return cmd_list_passive(p, d, n, out);
    case 0x40:
        return cmd_data_exchange(p, d, n, out, max);
    case 0x42:
        return cmd_comm_thru(p, d, n, out, max);
    case 0x44: /* InDeselect */
    case 0x52: /* InRelease */
        forget_card(p);
        out[0] = ST_OK;
        return 1;
    case 0x54: /* InSelect */
        out[0] = p->tag_valid ? ST_OK : ST_WRONG_STATE;
        return 1;
    default:
        return -1;
    }
}

size_t pn532_exec(Pn532* p, const uint8_t* req, size_t req_len, uint8_t* rsp, size_t rsp_max) {
    if(req_len < 1 || rsp_max < 2) return 0;
    uint8_t body[NB_PN532_RSP_MAX];
    int n = handle(p, req[0], req + 1, req_len - 1, body, sizeof(body));
    if(n < 0) return 0;
    if((size_t)n + 1 > rsp_max) return 0;
    rsp[0] = (uint8_t)(req[0] + 1);
    memcpy(rsp + 1, body, (size_t)n);
    return (size_t)n + 1;
}

/* ------------------------------------------------------------------ framing */

static void send_raw(Pn532* p, const uint8_t* d, size_t n) {
    if(p->write) p->write(p->io, d, n);
}

static void send_response(Pn532* p, const uint8_t* rsp, size_t rsp_len) {
    /* rsp = {cmd+1, data...}; frame payload = TFI + rsp */
    uint8_t* f = p->last_frame;
    size_t len = rsp_len + 1;
    size_t i = 0;
    f[i++] = 0x00;
    f[i++] = 0x00;
    f[i++] = 0xFF;
    if(len < 256) {
        f[i++] = (uint8_t)len;
        f[i++] = (uint8_t)(0x100 - len);
    } else {
        f[i++] = 0xFF;
        f[i++] = 0xFF;
        uint8_t lm = (uint8_t)(len >> 8), ll = (uint8_t)len;
        f[i++] = lm;
        f[i++] = ll;
        f[i++] = (uint8_t)(0x100 - ((lm + ll) & 0xFF));
    }
    uint8_t sum = TFI_PN532;
    f[i++] = TFI_PN532;
    for(size_t k = 0; k < rsp_len; k++) {
        f[i++] = rsp[k];
        sum = (uint8_t)(sum + rsp[k]);
    }
    f[i++] = (uint8_t)(0x100 - sum);
    f[i++] = 0x00;
    p->last_frame_len = i;
    send_raw(p, f, i);
}

static void run_frame(Pn532* p, const uint8_t* body, size_t len) {
    /* body = TFI, cmd, data... (already validated) */
    send_raw(p, ACK_FRAME, sizeof(ACK_FRAME));
    if(len < 2 || body[0] != TFI_HOST) return;
    uint8_t rsp[NB_PN532_RSP_MAX];
    size_t n = pn532_exec(p, body + 1, len - 1, rsp, sizeof(rsp));
    if(n == 0) {
        p->last_frame_len = 0;
        send_raw(p, ERR_FRAME, sizeof(ERR_FRAME));
        return;
    }
    send_response(p, rsp, n);
}

static void drop(Pn532* p, size_t n) {
    if(n >= p->rx_len) {
        p->rx_len = 0;
        return;
    }
    memmove(p->rx, p->rx + n, p->rx_len - n);
    p->rx_len -= n;
}

static void process(Pn532* p) {
    for(;;) {
        /* find the 00 FF start code */
        size_t i = 0;
        while(i + 1 < p->rx_len && !(p->rx[i] == 0x00 && p->rx[i + 1] == 0xFF))
            i++;
        if(i + 1 >= p->rx_len) { /* none yet: keep a trailing 00, it may be the start */
            if(p->rx_len > 0 && p->rx[p->rx_len - 1] == 0x00) {
                p->rx[0] = 0x00;
                p->rx_len = 1;
            } else {
                p->rx_len = 0;
            }
            return;
        }
        drop(p, i);
        if(p->rx_len < 4) return;

        uint8_t len = p->rx[2], lcs = p->rx[3];
        if(len == 0x00 && lcs == 0xFF) { /* host ACK */
            if(p->rx_len < 5) return;
            drop(p, 5);
            continue;
        }
        if(len == 0xFF && lcs == 0x00) { /* host NACK: repeat the last frame */
            if(p->rx_len < 5) return;
            drop(p, 5);
            if(p->last_frame_len) send_raw(p, p->last_frame, p->last_frame_len);
            continue;
        }

        size_t hdr, flen;
        if(len == 0xFF && lcs == 0xFF) { /* extended frame */
            if(p->rx_len < 7) return;
            uint8_t lm = p->rx[4], ll = p->rx[5], lcs2 = p->rx[6];
            if((uint8_t)(lm + ll + lcs2) != 0) {
                drop(p, 2);
                continue;
            }
            hdr = 7;
            flen = ((size_t)lm << 8) | ll;
        } else {
            if((uint8_t)(len + lcs) != 0) { /* bad length checksum: resync */
                drop(p, 2);
                continue;
            }
            hdr = 4;
            flen = len;
        }
        if(flen + hdr + 2 > NB_PN532_RX_MAX) { /* too big for us */
            drop(p, 2);
            send_raw(p, NACK_FRAME, sizeof(NACK_FRAME));
            continue;
        }
        if(p->rx_len < hdr + flen + 2) return; /* wait for the rest */

        uint8_t sum = 0;
        for(size_t k = 0; k < flen; k++)
            sum = (uint8_t)(sum + p->rx[hdr + k]);
        uint8_t dcs = p->rx[hdr + flen];
        if((uint8_t)(sum + dcs) != 0) {
            drop(p, hdr + flen + 2);
            send_raw(p, NACK_FRAME, sizeof(NACK_FRAME));
            continue;
        }
        uint8_t body[NB_PN532_RX_MAX];
        memcpy(body, p->rx + hdr, flen);
        drop(p, hdr + flen + 2);
        run_frame(p, body, flen);
    }
}

void pn532_feed(Pn532* p, const uint8_t* data, size_t len) {
    while(len > 0) {
        size_t room = NB_PN532_RX_MAX - p->rx_len;
        if(room == 0) { /* garbage filled the buffer: start over */
            p->rx_len = 0;
            room = NB_PN532_RX_MAX;
        }
        size_t take = len < room ? len : room;
        memcpy(p->rx + p->rx_len, data, take);
        p->rx_len += take;
        data += take;
        len -= take;
        process(p);
    }
}
