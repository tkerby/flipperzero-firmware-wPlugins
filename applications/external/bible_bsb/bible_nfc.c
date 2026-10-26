#include "bible_nfc.h"

#include <furi.h>
#include <string.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/helpers/nfc_data_generator.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/nfc_generic_event.h>

/* ============================================================================
 * NDEF constants
 * ============================================================================ */
#define NDEF_HEADER      0xD1 /* MB=1, ME=1, SR=1, TNF=01 (Well-known) */
#define NDEF_TYPE_LEN    0x01 /* Single-byte type */
#define NDEF_TYPE_URI    0x55 /* 'U' — URI record */
#define URI_PREFIX_HTTPS 0x04 /* https:// */
#define NDEF_TYPE_TEXT   0x54 /* 'T' — Text record */

/* NTAG215 capacity */
#define NTAG215_USER_PAGES  126
#define NTAG215_TOTAL_BYTES (NTAG215_USER_PAGES * MF_ULTRALIGHT_PAGE_SIZE)
#define CC_PAGE             3

/* ============================================================================
 * Emulation state (static — one active emulation at a time)
 * ============================================================================ */
typedef struct {
    Nfc* nfc;
    NfcListener* listener;
    NfcDevice* device;
    bool active;
} EmulationState;

static EmulationState emulation_state = {
    .nfc = NULL,
    .listener = NULL,
    .device = NULL,
    .active = false,
};

/* ============================================================================
 * NDEF builders
 * ============================================================================ */

/**
 * Build raw NDEF URI record bytes.
 *
 * Format: 0xD1 0x01 payload_len 0x55 0x04 {url_without_https://}
 *
 * @param url          full URL starting with "https://"
 * @param out_ndef     output buffer (must be at least 128 bytes)
 * @param out_ndef_len [out] number of bytes written
 * @return true if built successfully
 */
static bool build_ndef_url(const char* url, uint8_t* out_ndef, uint8_t* out_ndef_len) {
    furi_check(url);
    furi_check(out_ndef);
    furi_check(out_ndef_len);

    /* Strip "https://" prefix — the NDEF URI prefix byte already encodes it */
    const char* stripped = url;
    if(strncmp(stripped, "https://", 8) == 0) {
        stripped += 8;
    }

    size_t url_len = strlen(stripped);
    if(url_len > 200) return false; /* sanity limit for NDEF payload */

    uint8_t payload_len = (uint8_t)(1 + url_len);
    if(payload_len > 250) return false;

    uint8_t i = 0;
    out_ndef[i++] = NDEF_HEADER;
    out_ndef[i++] = NDEF_TYPE_LEN;
    out_ndef[i++] = payload_len;
    out_ndef[i++] = NDEF_TYPE_URI;
    out_ndef[i++] = URI_PREFIX_HTTPS;

    for(size_t j = 0; j < url_len; j++) {
        out_ndef[i++] = (uint8_t)stripped[j];
    }

    *out_ndef_len = i;
    return true;
}

/**
 * Build raw NDEF Text record bytes.
 *
 * Format: 0xD1 0x01 payload_len 0x54 0x02 'en' {text_bytes}
 *
 * @param text         plain text payload
 * @param out_ndef     output buffer
 * @param out_ndef_len [out] number of bytes written
 * @return true if built successfully
 */
static bool build_ndef_text(const char* text, uint8_t* out_ndef, uint8_t* out_ndef_len) {
    furi_check(text);
    furi_check(out_ndef);
    furi_check(out_ndef_len);

    size_t text_len = strlen(text);
    if(text_len > 480) return false;

    static const char lang[] = "en";
    uint8_t payload_len = (uint8_t)(1 + sizeof(lang) - 1 + text_len);

    uint8_t i = 0;
    out_ndef[i++] = NDEF_HEADER;
    out_ndef[i++] = NDEF_TYPE_LEN;
    out_ndef[i++] = payload_len;
    out_ndef[i++] = NDEF_TYPE_TEXT;
    out_ndef[i++] = 0x02; /* UTF-8, language code length = 2 */

    for(size_t j = 0; j < (sizeof(lang) - 1); j++) {
        out_ndef[i++] = (uint8_t)lang[j];
    }

    for(size_t j = 0; j < text_len; j++) {
        out_ndef[i++] = (uint8_t)text[j];
    }

    *out_ndef_len = i;
    return true;
}

/* ============================================================================
 * NTAG215 page writer
 * ============================================================================ */

/**
 * Write NDEF bytes into an NTAG215 MfUltralightData page array.
 *
 * Page layout:
 *   Pages 0-2: UID/BCC (set by nfc_data_generator_fill_data)
 *   Page 3:    CC = E1 10 12 00
 *   Pages 4+:  TLV: 0x03 + len + NDEF bytes + 0xFE terminator
 *   Remaining: zeroed
 *
 * @param ndef      NDEF record bytes
 * @param ndef_len  length of NDEF record
 * @param pages     MfUltralightData page array (modified in place)
 * @return number of pages written, or 0 if too large
 */
static uint16_t
    write_ndef_to_ntag215_pages(const uint8_t* ndef, uint8_t ndef_len, MfUltralightPage* pages) {
    furi_check(ndef);
    furi_check(pages);

    /* NDEF + TLV overhead (0x03 tag, 1-byte len, 0xFE terminator) = ndef_len + 3
     * Must fit after system pages 0-3 (16 bytes) within NTAG215 total */
    if((uint32_t)ndef_len + 3U + 16U > (uint32_t)NTAG215_TOTAL_BYTES) {
        return 0;
    }

    /* Capability Container at page 3 */
    pages[CC_PAGE].data[0] = 0xE1;
    pages[CC_PAGE].data[1] = 0x10;
    pages[CC_PAGE].data[2] = 0x12;
    pages[CC_PAGE].data[3] = 0x00;

    /* Write TLV starting at byte offset 16 (page 4, byte 0) */
    uint16_t offset = 16;

    /* TLV tag: NDEF Message */
    pages[offset / 4].data[offset % 4] = 0x03;
    offset++;

    /* TLV length */
    pages[offset / 4].data[offset % 4] = ndef_len;
    offset++;

    /* NDEF payload */
    for(uint8_t j = 0; j < ndef_len; j++) {
        pages[offset / 4].data[offset % 4] = ndef[j];
        offset++;
    }

    /* TLV terminator */
    pages[offset / 4].data[offset % 4] = 0xFE;
    offset++;

    /* Zero-fill remaining user pages */
    while(offset < NTAG215_TOTAL_BYTES) {
        pages[offset / 4].data[offset % 4] = 0x00;
        offset++;
    }

    return (uint16_t)((offset + 3) / 4);
}

/* ============================================================================
 * NFC listener callback
 * ============================================================================ */
static NfcCommand bible_nfc_listener_callback(NfcGenericEvent event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return NfcCommandContinue;
}

/* ============================================================================
 * Shared emulation start for any NDEF payload
 * ============================================================================ */
static bool start_ndef_emulation(const uint8_t* ndef, uint8_t ndef_len) {
    furi_check(ndef);

    /* Stop any existing emulation first */
    if(emulation_state.active) {
        bible_nfc_stop(NULL);
    }

    /* 1. Allocate NFC HAL */
    Nfc* nfc = nfc_alloc();
    if(nfc == NULL) {
        return false;
    }
    emulation_state.nfc = nfc;

    /* 2. Allocate NFC device and fill with NTAG215 data (UID, BCC, etc.) */
    NfcDevice* device = nfc_device_alloc();
    if(device == NULL) {
        nfc_free(nfc);
        emulation_state.nfc = NULL;
        return false;
    }
    emulation_state.device = device;

    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, device);

    /* 3. Get protocol-specific data pointer and write NDEF into pages.
     *    nfc_device_get_data returns const. The page buffer is mutated in place. */
    const NfcDeviceData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);
    if(data == NULL) {
        nfc_device_free(device);
        emulation_state.device = NULL;
        nfc_free(nfc);
        emulation_state.nfc = NULL;
        return false;
    }

    MfUltralightData* mf_data = (MfUltralightData*)data;
    uint16_t pages_written = write_ndef_to_ntag215_pages(ndef, ndef_len, mf_data->page);
    if(pages_written == 0) {
        nfc_device_free(device);
        emulation_state.device = NULL;
        nfc_free(nfc);
        emulation_state.nfc = NULL;
        return false;
    }

    /* 4. Create NFC protocol listener */
    NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolMfUltralight, data);
    if(listener == NULL) {
        nfc_device_free(device);
        emulation_state.device = NULL;
        nfc_free(nfc);
        emulation_state.nfc = NULL;
        return false;
    }
    emulation_state.listener = listener;

    /* 5. Start listener — this internally starts NFC HAL emulation */
    nfc_listener_start(listener, bible_nfc_listener_callback, NULL);

    emulation_state.active = true;
    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

bool bible_nfc_start_url(BibleAppState* state) {
    furi_check(state);

    if(state->nfc_url[0] == '\0') {
        return false;
    }

    uint8_t ndef[256];
    uint8_t ndef_len = 0;

    if(!build_ndef_url(state->nfc_url, ndef, &ndef_len)) {
        return false;
    }

    bool ok = start_ndef_emulation(ndef, ndef_len);
    if(ok) {
        state->nfc_emitting = true;
        state->nfc_is_export = false;
    }
    return ok;
}

bool bible_nfc_start_text(BibleAppState* state, const char* text) {
    furi_check(state);
    furi_check(text);

    /* NTAG215 payload capacity is ~500 bytes after system pages and TLV overhead */
    if(strlen(text) > 480) {
        return false;
    }

    uint8_t ndef[512];
    uint8_t ndef_len = 0;

    if(!build_ndef_text(text, ndef, &ndef_len)) {
        return false;
    }

    bool ok = start_ndef_emulation(ndef, ndef_len);
    if(ok) {
        strlcpy(state->nfc_url, "Export JSON", sizeof(state->nfc_url));
        state->nfc_emitting = true;
        state->nfc_is_export = true;
    }
    return ok;
}

void bible_nfc_stop(BibleAppState* state) {
    /* state may be NULL when called internally from start_ndef_emulation */

    /* Cleanup in exact reverse of allocation order:
     *   listener_stop → listener_free → device_free → nfc_free */

    if(emulation_state.listener != NULL) {
        nfc_listener_stop(emulation_state.listener);
        nfc_listener_free(emulation_state.listener);
        emulation_state.listener = NULL;
    }

    if(emulation_state.device != NULL) {
        nfc_device_free(emulation_state.device);
        emulation_state.device = NULL;
    }

    if(emulation_state.nfc != NULL) {
        nfc_free(emulation_state.nfc);
        emulation_state.nfc = NULL;
    }

    emulation_state.active = false;

    if(state != NULL) {
        state->nfc_emitting = false;
        state->nfc_url[0] = '\0';
        state->nfc_is_export = false;
    }
}
