/*
 * nb_backend_flipper.c
 *
 * - detect()      : ISO14443-3A anticollision (UID/ATQA/SAK). If the card is ISO14443-4
 *                   (SAK bit 5) a persistent poller "session" is opened so the card
 *                   stays selected between APDUs, and the ATS is captured.
 * - apdu()        : runs one block exchange inside that session.
 * - mfc_* / mful_*: the firmware's synchronous pollers. Each call activates the card
 *                   itself, so there is no state to keep here.
 * - raw()         : not implemented (InCommunicateThru answers "timeout").
 *
 * All calls must come from ONE thread (the bridge worker).
 */
#include "nb_backend_flipper.h"

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <toolbox/bit_buffer.h>
#include <stdlib.h>
#include <string.h>

#define SESSION_BUF 300

struct FbBackend {
    Nfc* nfc;
    NbBackend be;

    /* ISO14443-4A session */
    NfcPoller* poller;
    bool session_open;
    volatile bool session_stop;
    volatile bool session_ok;
    FuriSemaphore* sem_ready;
    FuriSemaphore* sem_req;
    FuriSemaphore* sem_resp;
    BitBuffer* tx;
    BitBuffer* rx;
    volatile int req_result;
    uint8_t ats[NB_ATS_MAX];
    uint8_t ats_len;
};

/* ------------------------------------------------------------ 14443-4 session */

static void capture_ats(FbBackend* f, const Iso14443_4aData* d) {
    const Iso14443_4aAtsData* a = &d->ats_data;
    size_t n = 0;
    f->ats[n++] = 0; /* TL, filled in below */
    f->ats[n++] = a->t0;
    if(a->t0 & 0x10) f->ats[n++] = a->ta_1;
    if(a->t0 & 0x20) f->ats[n++] = a->tb_1;
    if(a->t0 & 0x40) f->ats[n++] = a->tc_1;
    uint32_t cnt = 0;
    const uint8_t* hist = iso14443_4a_get_historical_bytes(d, &cnt);
    for(uint32_t i = 0; hist && i < cnt && n < NB_ATS_MAX; i++)
        f->ats[n++] = hist[i];
    f->ats[0] = (uint8_t)n;
    f->ats_len = (uint8_t)n;
}

static NfcCommand session_callback(NfcGenericEvent event, void* context) {
    FbBackend* f = context;
    const Iso14443_4aPollerEvent* e = event.event_data;

    if(e->type == Iso14443_4aPollerEventTypeError) {
        f->session_ok = false;
        furi_semaphore_release(f->sem_ready);
        return NfcCommandStop;
    }
    if(e->type == Iso14443_4aPollerEventTypeReady) {
        capture_ats(f, nfc_poller_get_data(f->poller));
        f->session_ok = true;
        furi_semaphore_release(f->sem_ready);

        /* keep the card selected and serve APDU requests until told to stop */
        while(!f->session_stop) {
            if(furi_semaphore_acquire(f->sem_req, 100) != FuriStatusOk) continue;
            if(f->session_stop) break;
            Iso14443_4aError err = iso14443_4a_poller_send_block(event.instance, f->tx, f->rx);
            f->req_result = (err == Iso14443_4aErrorNone) ? (int)bit_buffer_get_size_bytes(f->rx) :
                                                            NB_ERR_TIMEOUT;
            furi_semaphore_release(f->sem_resp);
        }
        return NfcCommandStop;
    }
    return NfcCommandContinue;
}

static void session_close(FbBackend* f) {
    if(f->poller) {
        f->session_stop = true;
        furi_semaphore_release(
            f->sem_req); /* wake the callback; fails harmlessly if already signalled */
        nfc_poller_stop(f->poller);
        nfc_poller_free(f->poller);
        f->poller = NULL;
    }
    f->session_open = false;
    f->ats_len = 0;
}

static bool session_open_4a(FbBackend* f) {
    /* make sure no stale tokens are left in the semaphores */
    while(furi_semaphore_acquire(f->sem_ready, 0) == FuriStatusOk) {
    }
    while(furi_semaphore_acquire(f->sem_req, 0) == FuriStatusOk) {
    }
    while(furi_semaphore_acquire(f->sem_resp, 0) == FuriStatusOk) {
    }
    f->session_stop = false;
    f->session_ok = false;
    f->poller = nfc_poller_alloc(f->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(f->poller, session_callback, f);
    if(furi_semaphore_acquire(f->sem_ready, 1500) != FuriStatusOk || !f->session_ok) {
        session_close(f);
        return false;
    }
    f->session_open = true;
    return true;
}

/* ----------------------------------------------------------------- backend fns */

static int fb_detect(void* ctx, NbTag* out) {
    FbBackend* f = ctx;
    session_close(f);
    memset(out, 0, sizeof(*out));

    Iso14443_3aData* d = iso14443_3a_alloc();
    int rc = 0;
    if(iso14443_3a_poller_sync_read(f->nfc, d) == Iso14443_3aErrorNone) {
        out->uid_len = d->uid_len > NB_UID_MAX ? NB_UID_MAX : d->uid_len;
        memcpy(out->uid, d->uid, out->uid_len);
        out->atqa[0] = d->atqa[0];
        out->atqa[1] = d->atqa[1];
        out->sak = d->sak;
        rc = 1;
        if(d->sak & 0x20) { /* ISO-DEP card: select it for good and read its ATS */
            if(session_open_4a(f)) {
                memcpy(out->ats, f->ats, f->ats_len);
                out->ats_len = f->ats_len;
            }
        }
    }
    iso14443_3a_free(d);
    return rc;
}

static int fb_apdu(void* ctx, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_max) {
    FbBackend* f = ctx;
    if(!f->session_open || tx_len == 0 || tx_len > SESSION_BUF) return NB_ERR_IO;
    bit_buffer_copy_bytes(f->tx, tx, tx_len);
    bit_buffer_reset(f->rx);
    furi_semaphore_release(f->sem_req);
    if(furi_semaphore_acquire(f->sem_resp, 3000) != FuriStatusOk) {
        session_close(f);
        return NB_ERR_TIMEOUT;
    }
    if(f->req_result < 0) return f->req_result;
    size_t n = bit_buffer_get_size_bytes(f->rx);
    if(n > rx_max) n = rx_max;
    memcpy(rx, bit_buffer_get_data(f->rx), n);
    return (int)n;
}

static int map_mfc(MfClassicError e) {
    switch(e) {
    case MfClassicErrorNone:
        return NB_OK;
    case MfClassicErrorAuth:
        return NB_ERR_AUTH;
    case MfClassicErrorNotPresent:
    case MfClassicErrorTimeout:
        return NB_ERR_TIMEOUT;
    default:
        return NB_ERR_IO;
    }
}

static int fb_mfc_auth(void* ctx, uint8_t block, bool key_b, const uint8_t key[6]) {
    FbBackend* f = ctx;
    session_close(f);
    MfClassicKey k;
    memcpy(k.data, key, 6);
    MfClassicAuthContext ac;
    return map_mfc(mf_classic_poller_sync_auth(
        f->nfc, block, &k, key_b ? MfClassicKeyTypeB : MfClassicKeyTypeA, &ac));
}

static int
    fb_mfc_read(void* ctx, uint8_t block, bool key_b, const uint8_t key[6], uint8_t out[16]) {
    FbBackend* f = ctx;
    session_close(f);
    MfClassicKey k;
    memcpy(k.data, key, 6);
    MfClassicBlock b;
    int r = map_mfc(mf_classic_poller_sync_read_block(
        f->nfc, block, &k, key_b ? MfClassicKeyTypeB : MfClassicKeyTypeA, &b));
    if(r == NB_OK) memcpy(out, b.data, 16);
    return r;
}

static int
    fb_mfc_write(void* ctx, uint8_t block, bool key_b, const uint8_t key[6], const uint8_t in[16]) {
    FbBackend* f = ctx;
    session_close(f);
    MfClassicKey k;
    memcpy(k.data, key, 6);
    MfClassicBlock b;
    memcpy(b.data, in, 16);
    return map_mfc(mf_classic_poller_sync_write_block(
        f->nfc, block, &k, key_b ? MfClassicKeyTypeB : MfClassicKeyTypeA, &b));
}

static int map_mful(MfUltralightError e) {
    switch(e) {
    case MfUltralightErrorNone:
        return NB_OK;
    case MfUltralightErrorAuth:
        return NB_ERR_AUTH;
    case MfUltralightErrorNotPresent:
    case MfUltralightErrorTimeout:
        return NB_ERR_TIMEOUT;
    default:
        return NB_ERR_IO;
    }
}

/* One READ on the wire returns 4 pages; the firmware API reads page by page. */
static int fb_mful_read(void* ctx, uint8_t page, uint8_t out[16]) {
    FbBackend* f = ctx;
    session_close(f);
    for(uint8_t i = 0; i < 4; i++) {
        MfUltralightPage p;
        MfUltralightError e =
            mf_ultralight_poller_sync_read_page(f->nfc, (uint16_t)(page + i), &p);
        if(e != MfUltralightErrorNone) {
            if(i == 0) return map_mful(e);
            memset(out + 4 * i, 0, (size_t)(16 - 4 * i)); /* ran off the end of the card */
            return NB_OK;
        }
        memcpy(out + 4 * i, p.data, 4);
    }
    return NB_OK;
}

static int fb_mful_write(void* ctx, uint8_t page, const uint8_t in[4]) {
    FbBackend* f = ctx;
    session_close(f);
    MfUltralightPage p;
    memcpy(p.data, in, 4);
    return map_mful(mf_ultralight_poller_sync_write_page(f->nfc, page, &p));
}

static void fb_release(void* ctx) {
    session_close((FbBackend*)ctx);
}

static uint32_t fb_tick(void* ctx) {
    UNUSED(ctx);
    return furi_get_tick();
}

/* ------------------------------------------------------------------ lifecycle */

FbBackend* fb_backend_alloc(void) {
    FbBackend* f = malloc(sizeof(FbBackend));
    memset(f, 0, sizeof(*f));
    f->nfc = nfc_alloc();
    f->sem_ready = furi_semaphore_alloc(1, 0);
    f->sem_req = furi_semaphore_alloc(1, 0);
    f->sem_resp = furi_semaphore_alloc(1, 0);
    f->tx = bit_buffer_alloc(SESSION_BUF);
    f->rx = bit_buffer_alloc(SESSION_BUF);

    f->be.ctx = f;
    f->be.detect = fb_detect;
    f->be.apdu = fb_apdu;
    f->be.raw = NULL;
    f->be.mfc_auth = fb_mfc_auth;
    f->be.mfc_read = fb_mfc_read;
    f->be.mfc_write = fb_mfc_write;
    f->be.mful_read = fb_mful_read;
    f->be.mful_write = fb_mful_write;
    f->be.release = fb_release;
    f->be.field = NULL;
    f->be.tick_ms = fb_tick;
    return f;
}

void fb_backend_free(FbBackend* f) {
    session_close(f);
    bit_buffer_free(f->tx);
    bit_buffer_free(f->rx);
    furi_semaphore_free(f->sem_ready);
    furi_semaphore_free(f->sem_req);
    furi_semaphore_free(f->sem_resp);
    nfc_free(f->nfc);
    free(f);
}

const NbBackend* fb_backend_get(FbBackend* f) {
    return &f->be;
}
