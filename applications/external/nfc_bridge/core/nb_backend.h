/*
 * nb_backend.h - the only thing the protocol engines know about the NFC hardware.
 *
 * The PN532 engine and the CCID/PC-SC engine are plain C with no Flipper
 * dependencies.  They talk to a tag through this table of function pointers, so
 * the same code runs on the Flipper (nb_backend_flipper.c) and on a PC against a
 * simulated tag (tests/mock_backend.c).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NB_OK 0
#define NB_ERR_TIMEOUT (-1) /* no answer from the card */
#define NB_ERR_AUTH (-2) /* MIFARE Classic authentication failed */
#define NB_ERR_IO (-3) /* other failure */
#define NB_ERR_UNSUPPORTED (-4) /* backend cannot do this */

#define NB_UID_MAX 10
#define NB_ATS_MAX 32

typedef struct {
    uint8_t atqa[2]; /* exactly as received on the air (LSB first): 04 00 for a 1K */
    uint8_t sak;
    uint8_t uid[NB_UID_MAX];
    uint8_t uid_len;
    uint8_t ats[NB_ATS_MAX]; /* full ATS including the TL byte; ats_len == 0 if none */
    uint8_t ats_len;
} NbTag;

typedef enum {
    NbTagUnknown = 0,
    NbTagMfcMini,
    NbTagMfc1k,
    NbTagMfc4k,
    NbTagUltralight, /* MIFARE Ultralight / NTAG (type 2) */
    NbTagIso4a, /* ISO14443-4 (has an ATS), speaks APDUs */
} NbTagType;

NbTagType nb_tag_type(const NbTag* t);

typedef struct NbBackend {
    void* ctx;

    /* Look for ONE ISO14443-A card in the field and activate it.
     * return 1 = found (out filled), 0 = nothing there, <0 = error. */
    int (*detect)(void* ctx, NbTag* out);

    /* ISO14443-4 pass-through: send an APDU/block and get the answer.
     * Returns the number of bytes in rx, or a negative NB_ERR_*. The card must
     * stay selected between calls (applet selection is stateful). */
    int (*apdu)(void* ctx, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_max);

    /* Raw ISO14443-A frame (optional, may be NULL).  tx_bits is the exact bit
     * count (<= 8*bytes).  crc_tx / crc_rx say whether the hardware should append
     * / check the CRC_A.  Returns received bytes (rx_bits gets the bit count). */
    int (*raw)(
        void* ctx,
        const uint8_t* tx,
        size_t tx_bits,
        bool crc_tx,
        bool crc_rx,
        uint8_t* rx,
        size_t rx_max,
        size_t* rx_bits,
        uint32_t timeout_ms);

    /* MIFARE Classic: block-level operations.  Every call carries the key, the
     * backend re-authenticates as needed.  mfc_auth only verifies the key. */
    int (*mfc_auth)(void* ctx, uint8_t block, bool key_b, const uint8_t key[6]);
    int (*mfc_read)(void* ctx, uint8_t block, bool key_b, const uint8_t key[6], uint8_t out[16]);
    int (*mfc_write)(void* ctx, uint8_t block, bool key_b, const uint8_t key[6], const uint8_t in[16]);

    /* MIFARE Ultralight / NTAG: 4-byte pages. read returns 4 pages (16 bytes). */
    int (*mful_read)(void* ctx, uint8_t page, uint8_t out[16]);
    int (*mful_write)(void* ctx, uint8_t page, const uint8_t in[4]);

    /* Forget / halt the current card (may be NULL). */
    void (*release)(void* ctx);

    /* Switch the RF field (may be NULL). */
    void (*field)(void* ctx, bool on);

    /* Monotonic milliseconds. */
    uint32_t (*tick_ms)(void* ctx);

    /* Optional cheap "is the card still there?" check that must NOT disturb an
     * activated card (unlike detect()). Lets the CCID engine notice a card going
     * away while the host has it powered. NULL = backend cannot tell. */
    bool (*is_present)(void* ctx);
} NbBackend;
