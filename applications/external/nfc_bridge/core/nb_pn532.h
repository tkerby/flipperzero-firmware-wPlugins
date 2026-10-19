/*
 * nb_pn532.h - PN532 host-protocol engine ("pretend to be a PN532 on a serial port").
 *
 * Feed it the bytes that arrive from the PC; it parses PN532 frames, runs the
 * command against the NbBackend and writes the reply frames through `write`.
 * Aimed at what libnfc's pn532_uart driver (nfc-list, nfc-poll, nfc-mfclassic,
 * nfc-mfultralight, libfreefare...) and nfcpy/pn532 python libraries send.
 *
 * Not supported (answers with a PN532 "syntax error" frame): target/emulation
 * commands (TgInitAsTarget...), InJumpForDEP/InATR (NFC-DEP), InAutoPoll, and
 * anything but 106 kbit/s ISO14443-A.
 */
#pragma once
#include "nb_backend.h"

#define NB_PN532_RX_MAX   320
#define NB_PN532_RSP_MAX  320
#define NB_PN532_REGS_MAX 48

typedef void (*Pn532WriteFn)(void* io, const uint8_t* data, size_t len);

typedef struct {
    const NbBackend* be;
    Pn532WriteFn write;
    void* io;

    uint8_t rx[NB_PN532_RX_MAX];
    size_t rx_len;

    uint8_t last_frame[NB_PN532_RSP_MAX + 16];
    size_t last_frame_len;

    struct {
        uint16_t addr;
        uint8_t val;
    } regs[NB_PN532_REGS_MAX];
    size_t nregs;

    NbTag tag;
    bool tag_valid;

    bool auth_valid;
    bool auth_key_b;
    uint8_t auth_key[6];
    uint16_t auth_sector;
} Pn532;

void pn532_init(Pn532* p, const NbBackend* be, Pn532WriteFn write, void* io);

/* Push received bytes in.  May call be->* and write() (blocking) before returning. */
void pn532_feed(Pn532* p, const uint8_t* data, size_t len);

/* Run one command without framing. req = {cmd, data...}.  On success writes
 * {cmd+1, data...} to rsp and returns its length; returns 0 if unsupported. */
size_t pn532_exec(Pn532* p, const uint8_t* req, size_t req_len, uint8_t* rsp, size_t rsp_max);
