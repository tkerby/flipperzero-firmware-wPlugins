/* A simulated card for testing the engines on a PC. */
#include "mock_backend.h"
#include <string.h>

static MockCard card = MOCK_NONE;
static uint32_t now_ms;
static bool selected; /* ISO4A: applet selected? */

int mock_detect_calls, mock_release_calls, mock_apdu_calls, mock_raw_calls;
size_t mock_last_raw_bits;
bool mock_last_raw_crc_tx, mock_last_raw_crc_rx;
uint8_t mock_mfc[64][16];
uint8_t mock_ul[16][4];

static const uint8_t KEY_A[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
static const uint8_t KEY_B[6] = {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5};
static const uint8_t AID[6] = {0xF0, 0x01, 0x02, 0x03, 0x04, 0x05};

void mock_set_card(MockCard k) {
    card = k;
    selected = false;
}

void mock_set_time(uint32_t ms) {
    now_ms = ms;
}

void mock_reset(MockCard kind) {
    memset(mock_mfc, 0, sizeof(mock_mfc));
    memset(mock_ul, 0, sizeof(mock_ul));
    for(int b = 0; b < 64; b++) {
        if(b % 4 == 3) { /* sector trailer: keyA | access | keyB */
            memcpy(mock_mfc[b], KEY_A, 6);
            mock_mfc[b][6] = 0xFF;
            mock_mfc[b][7] = 0x07;
            mock_mfc[b][8] = 0x80;
            mock_mfc[b][9] = 0x69;
            memcpy(mock_mfc[b] + 10, KEY_B, 6);
        } else {
            for(int i = 0; i < 16; i++)
                mock_mfc[b][i] = (uint8_t)(b * 16 + i);
        }
    }
    mock_mfc[0][0] = 0xDE;
    mock_mfc[0][1] = 0xAD;
    mock_mfc[0][2] = 0xBE;
    mock_mfc[0][3] = 0xEF;
    mock_mfc[0][4] = 0xDE ^ 0xAD ^ 0xBE ^ 0xEF;
    for(int p = 0; p < 16; p++)
        for(int i = 0; i < 4; i++)
            mock_ul[p][i] = (uint8_t)(0x40 + p * 4 + i);
    mock_detect_calls = mock_release_calls = mock_apdu_calls = mock_raw_calls = 0;
    mock_last_raw_bits = 0;
    now_ms = 0;
    mock_set_card(kind);
}

static int m_detect(void* ctx, NbTag* t) {
    (void)ctx;
    mock_detect_calls++;
    memset(t, 0, sizeof(*t));
    switch(card) {
    case MOCK_MFC1K:
        t->atqa[0] = 0x04;
        t->atqa[1] = 0x00;
        t->sak = 0x08;
        memcpy(t->uid, mock_mfc[0], 4);
        t->uid_len = 4;
        return 1;
    case MOCK_UL: {
        static const uint8_t uid[7] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        t->atqa[0] = 0x44;
        t->atqa[1] = 0x00;
        t->sak = 0x00;
        memcpy(t->uid, uid, 7);
        t->uid_len = 7;
        return 1;
    }
    case MOCK_ISO4A: {
        static const uint8_t ats[12] = {
            0x0C, 0x78, 0x77, 0xB1, 0x02, 0x80, 0x31, 0xC0, 0x73, 0xC8, 0x40, 0x00};
        t->atqa[0] = 0x04;
        t->atqa[1] = 0x03;
        t->sak = 0x20;
        t->uid[0] = 0x08;
        t->uid[1] = 0xAA;
        t->uid[2] = 0xBB;
        t->uid[3] = 0xCC;
        t->uid_len = 4;
        memcpy(t->ats, ats, sizeof(ats));
        t->ats_len = sizeof(ats);
        selected = false; /* a fresh activation resets the applet state */
        return 1;
    }
    default:
        return 0;
    }
}

static int m_apdu(void* ctx, const uint8_t* tx, size_t n, uint8_t* rx, size_t max) {
    (void)ctx;
    (void)max;
    mock_apdu_calls++;
    if(card != MOCK_ISO4A) return NB_ERR_TIMEOUT;
    if(n >= 5 + 6 && tx[0] == 0x00 && tx[1] == 0xA4 && tx[2] == 0x04) { /* SELECT by AID */
        if(tx[4] == 6 && memcmp(tx + 5, AID, 6) == 0) {
            selected = true;
            rx[0] = 0x90;
            rx[1] = 0x00;
        } else {
            rx[0] = 0x6A;
            rx[1] = 0x82;
        }
        return 2;
    }
    if(n >= 4 && tx[0] == 0x00 && tx[1] == 0xB0) { /* READ: needs the applet selected first */
        if(!selected) {
            rx[0] = 0x69;
            rx[1] = 0x85;
            return 2;
        }
        memcpy(rx, "HELO", 4);
        rx[4] = 0x90;
        rx[5] = 0x00;
        return 6;
    }
    if(n >= 4 && tx[0] == 0x00 &&
       tx[1] == 0xC0) { /* long answer: forces an extended PN532 frame */
        for(int i = 0; i < 258; i++)
            rx[i] = (uint8_t)i;
        rx[258] = 0x90;
        rx[259] = 0x00;
        return 260;
    }
    rx[0] = 0x6D;
    rx[1] = 0x00;
    return 2;
}

static int m_raw(
    void* ctx,
    const uint8_t* tx,
    size_t bits,
    bool crc_tx,
    bool crc_rx,
    uint8_t* rx,
    size_t max,
    size_t* rbits,
    uint32_t to) {
    (void)ctx;
    (void)to;
    (void)max;
    mock_raw_calls++;
    mock_last_raw_bits = bits;
    mock_last_raw_crc_tx = crc_tx;
    mock_last_raw_crc_rx = crc_rx;
    rx[0] = (uint8_t)(tx[0] ^ 0xFF);
    *rbits = 8;
    return 1;
}

static bool key_ok(uint8_t blk, bool key_b, const uint8_t key[6]) {
    if(card != MOCK_MFC1K || blk >= 64) return false;
    uint8_t* trailer = mock_mfc[(blk / 4) * 4 + 3];
    return memcmp(key, key_b ? trailer + 10 : trailer, 6) == 0;
}

static int m_mfc_auth(void* ctx, uint8_t blk, bool b, const uint8_t key[6]) {
    (void)ctx;
    return key_ok(blk, b, key) ? NB_OK : NB_ERR_AUTH;
}

static int m_mfc_read(void* ctx, uint8_t blk, bool b, const uint8_t key[6], uint8_t out[16]) {
    (void)ctx;
    if(!key_ok(blk, b, key)) return NB_ERR_AUTH;
    memcpy(out, mock_mfc[blk], 16);
    return NB_OK;
}

static int
    m_mfc_write(void* ctx, uint8_t blk, bool b, const uint8_t key[6], const uint8_t in[16]) {
    (void)ctx;
    if(!key_ok(blk, b, key)) return NB_ERR_AUTH;
    if(blk == 0) return NB_ERR_IO; /* like a real card: block 0 is read-only */
    memcpy(mock_mfc[blk], in, 16);
    return NB_OK;
}

static int m_mful_read(void* ctx, uint8_t page, uint8_t out[16]) {
    (void)ctx;
    if(card != MOCK_UL || page >= 16) return NB_ERR_IO;
    for(int i = 0; i < 4; i++)
        memcpy(out + 4 * i, mock_ul[(page + i) % 16], 4); /* reads wrap around */
    return NB_OK;
}

static int m_mful_write(void* ctx, uint8_t page, const uint8_t in[4]) {
    (void)ctx;
    if(card != MOCK_UL || page < 4 || page >= 16) return NB_ERR_IO; /* pages 0-3 locked */
    memcpy(mock_ul[page], in, 4);
    return NB_OK;
}

static void m_release(void* ctx) {
    (void)ctx;
    mock_release_calls++;
    selected = false;
}

static uint32_t m_tick(void* ctx) {
    (void)ctx;
    return now_ms;
}

NbBackend mock_be = {
    .ctx = NULL,
    .detect = m_detect,
    .apdu = m_apdu,
    .raw = m_raw,
    .mfc_auth = m_mfc_auth,
    .mfc_read = m_mfc_read,
    .mfc_write = m_mfc_write,
    .mful_read = m_mful_read,
    .mful_write = m_mful_write,
    .release = m_release,
    .field = NULL,
    .tick_ms = m_tick,
};
