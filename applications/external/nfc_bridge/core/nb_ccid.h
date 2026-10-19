/*
 * nb_ccid.h - USB CCID (smart-card reader class) message engine for a contactless
 * reader, plus the PC/SC part 3 "pseudo-APDUs" (the ACR122 / ACS style commands).
 *
 * The USB layer hands whole CCID Bulk-OUT messages to nb_ccid_process() and sends
 * back what it returns.  No Flipper or USB code in here.
 *
 * What a PC/SC application sees:
 *   - ATR synthesised from the card (PC/SC part 3, section 3.1.3), so cards get the
 *     usual names in pcsc_scan / Windows.
 *   - ISO14443-4 cards: APDUs pass straight through to the card.
 *   - MIFARE Classic / Ultralight / NTAG: FF CA (UID), FF 82 (load key),
 *     FF 86 (authenticate), FF B0 (read), FF D6 (write).
 *   - FF 00 00 00 Lc <D4 ...>: PN532 command pass-through (so libnfc's acr122_pcsc
 *     driver style access works), when a PN532 engine is attached.
 */
#pragma once
#include "nb_backend.h"
#include "nb_pn532.h"

#define NB_CCID_HDR     10
#define NB_CCID_MSG_MAX 280 /* header + up to 261 data + slack */

typedef struct {
    const NbBackend* be;
    Pn532* pn; /* optional: enables the FF 00 00 00 pass-through */

    NbTag tag;
    bool present;
    bool powered;

    uint8_t keys[4][6];
    bool key_set[4];

    bool auth_valid;
    bool auth_key_b;
    uint8_t auth_key[6];
    uint16_t auth_sector;

    bool reported_valid; /* have we told the host about the slot state yet? */
    bool reported_present;
    uint32_t last_poll_ms;
} NbCcid;

void nb_ccid_init(NbCcid* c, const NbBackend* be, Pn532* pn_or_null);

/* One complete Bulk-OUT message in, one Bulk-IN message out.
 * Returns the number of bytes written to out (0 = malformed, send nothing). */
size_t nb_ccid_process(NbCcid* c, const uint8_t* msg, size_t len, uint8_t* out, size_t out_max);

/* Call from time to time while idle. If the slot state changed (or was never
 * reported) fills note[2] with an Interrupt-IN RDR_to_PC_NotifySlotChange message
 * and returns 2, otherwise returns 0. */
int nb_ccid_poll_notify(NbCcid* c, uint8_t note[2]);

/* Build the PC/SC ATR for a card. Returns its length. (exposed for tests) */
size_t nb_ccid_make_atr(const NbTag* t, uint8_t* atr, size_t max);
