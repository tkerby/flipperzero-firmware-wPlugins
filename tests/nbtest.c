/* Thin C API over both engines + the mock card, loaded from Python with ctypes. */
#include "../core/nb_ccid.h"
#include "../core/nb_pn532.h"
#include "../core/nb_virtual.h"
#include "mock_backend.h"
#include <string.h>

static Pn532 pn;
static NbCcid cc;
static uint8_t outq[4096];
static size_t outq_len;

static void sink(void* io, const uint8_t* d, size_t n) {
    (void)io;
    if(outq_len + n <= sizeof(outq)) {
        memcpy(outq + outq_len, d, n);
        outq_len += n;
    }
}

void t_reset(int kind) {
    mock_reset((MockCard)kind);
    outq_len = 0;
    pn532_init(&pn, &mock_be, sink, NULL);
    nb_ccid_init(&cc, &mock_be, &pn);
}

void t_set_card(int kind) {
    mock_set_card((MockCard)kind);
}
void t_set_time(uint32_t ms) {
    mock_set_time(ms);
}

void t_pn_feed(const uint8_t* d, size_t n) {
    pn532_feed(&pn, d, n);
}

size_t t_pn_drain(uint8_t* buf, size_t max) {
    size_t n = outq_len < max ? outq_len : max;
    memcpy(buf, outq, n);
    outq_len = 0;
    return n;
}

size_t t_ccid(const uint8_t* m, size_t n, uint8_t* out, size_t max) {
    return nb_ccid_process(&cc, m, n, out, max);
}
int t_ccid_notify(uint8_t* note) {
    return nb_ccid_poll_notify(&cc, note);
}
size_t t_atr(int kind, uint8_t* out, size_t max) {
    NbTag t;
    mock_set_card((MockCard)kind);
    mock_be.detect(NULL, &t);
    return nb_ccid_make_atr(&t, out, max);
}

int t_stat(int what) {
    switch(what) {
    case 0:
        return mock_detect_calls;
    case 1:
        return mock_release_calls;
    case 2:
        return mock_apdu_calls;
    case 3:
        return mock_raw_calls;
    case 4:
        return (int)mock_last_raw_bits;
    case 5:
        return mock_last_raw_crc_tx;
    case 6:
        return mock_last_raw_crc_rx;
    }
    return -1;
}
void t_mfc_block(int b, uint8_t* out) {
    memcpy(out, mock_mfc[b], 16);
}
void t_ul_page(int p, uint8_t* out) {
    memcpy(out, mock_ul[p], 4);
}

/* ---- virtual card (loaded from a file image) -------------------------------- */
static NbVirtual* vc;
static uint32_t vclock_ms;
static uint32_t vclock(void) {
    return vclock_ms;
}

/* chunk == 0: whole buffer at once, else feed `chunk` bytes at a time. 1 = loaded. */
int tv_load(const uint8_t* d, size_t n, size_t chunk) {
    if(vc) nbv_free(vc);
    vc = nbv_alloc();
    nbv_set_clock(vc, vclock);
    vclock_ms = 0;
    if(chunk == 0) return nbv_load_buffer(vc, d, n) ? 1 : 0;
    nbv_load_begin(vc);
    for(size_t i = 0; i < n; i += chunk) {
        size_t k = (n - i) < chunk ? (n - i) : chunk;
        if(!nbv_load_feed(vc, d + i, k)) return 0;
    }
    return nbv_load_end(vc) ? 1 : 0;
}

const char* tv_error(void) {
    return vc ? nbv_error(vc) : "";
}
const char* tv_name(void) {
    return vc ? nbv_name(vc) : "";
}

/* Point both engines at the virtual card (call after tv_load). */
void tv_attach(void) {
    outq_len = 0;
    pn532_init(&pn, nbv_backend(vc), sink, NULL);
    nb_ccid_init(&cc, nbv_backend(vc), &pn);
}

void tv_set_present(int p) {
    nbv_set_present(vc, p != 0);
}
void tv_set_time(uint32_t ms) {
    vclock_ms = ms;
}
