// IR LINK device transport (contract section 45): the LinkChan over the Flipper's infrared port, through the firmware's raw HAL (furi_hal_infrared_async_*). The HAL has ONE state
// machine: while the port transmits it does not receive (the infrared worker of the firmware does the same), so the channel is HALF DUPLEX by construction and the protocol
// (link_proto.c) is request / response. RX: the capture ISR hands every (level, duration) to the codec (link_phy.c), which pushes finished bytes into a small ring buffer that
// lk_poll() drains; the silence ISR resets the codec at the end of a burst. TX: the data ISR asks the codec for the next mark / space element; the last one ends the transmission.
// Device only (link plugin).
#include <furi.h>
#include <furi_hal.h>
#ifdef __arm__ // short_calls.h (forced include) made our own calls short; this SDK header declares FIRMWARE functions, which must stay long calls
_Pragma("long_calls")
#endif
#include <furi_hal_infrared.h>
#ifdef __arm__
_Pragma("no_long_calls")
#endif
#include "link_host.h"
#include "link_phy.h"
#include "link_proto.h"

#define LKIR_TAG "NucleoLink"
#define LKIR_RING 160u

typedef struct {
    LinkChan ch;
    PhyRx prx;
    PhyTx ptx;
    uint8_t ring[LKIR_RING];
    volatile uint8_t rh, rt;
    uint8_t txb[LK_FRAME_MAX];
    volatile bool tx_last;
    bool tx_on, rx_on;
} LkIr;

static LkIr* g_lkir;

// ---- interrupt side (nothing here may block, allocate or log)
static void lkir_rx_capture(void* ctx, bool level, uint32_t duration) {
    LkIr* d = ctx;
    const int b = phy_rx_edge(&d->prx, level, duration);
    if(b >= 0) {
        const uint8_t n = (uint8_t)((d->rh + 1u) % LKIR_RING);
        if(n != d->rt) { // a full ring drops the byte (the frame fails its CRC and is repeated)
            d->ring[d->rh] = (uint8_t)b;
            d->rh = n;
        }
    }
}

static void lkir_rx_silence(void* ctx) {
    phy_rx_silence(&((LkIr*)ctx)->prx);
}

static FuriHalInfraredTxGetDataState lkir_tx_data(void* ctx, uint32_t* duration, bool* level) {
    LkIr* d = ctx;
    if(phy_tx_next(&d->ptx, duration, level) == PHY_TX_LAST) {
        d->tx_last = true;
        return FuriHalInfraredTxGetDataStateLastDone;
    }
    return FuriHalInfraredTxGetDataStateOk;
}

// ---- thread side
static void lkir_rx_start(LkIr* d) {
    phy_rx_init(&d->prx);
    furi_hal_infrared_async_rx_set_capture_isr_callback(lkir_rx_capture, d);
    furi_hal_infrared_async_rx_set_timeout_isr_callback(lkir_rx_silence, d);
    furi_hal_infrared_async_rx_start();
    furi_hal_infrared_async_rx_set_timeout(LK_RX_SILENCE_US);
    d->rx_on = true;
}

static void lkir_rx_stop(LkIr* d) {
    if(d->rx_on) {
        furi_hal_infrared_async_rx_stop();
        d->rx_on = false;
    }
}

static uint32_t lkir_now(void* ctx) {
    (void)ctx;
    return furi_get_tick();
}

static uint32_t lkir_rand(void* ctx) {
    (void)ctx;
    return furi_hal_random_get();
}

static bool lkir_busy(void* ctx) {
    LkIr* d = ctx;
    if(!d->tx_on) return false;
    if(!d->tx_last) return true;
    furi_hal_infrared_async_tx_wait_termination(); // the last element is queued: the DMA drains in a few ms
    d->tx_on = false;
    lkir_rx_start(d); // back to listening
    return false;
}

static bool lkir_tx(void* ctx, const uint8_t* buf, uint16_t n) {
    LkIr* d = ctx;
    if(lkir_busy(d) || n == 0 || n > LK_FRAME_MAX) return false;
    memcpy(d->txb, buf, n);
    lkir_rx_stop(d);
    if(furi_hal_infrared_is_busy()) { // the port is used by something else (the infrared app in the background): try again later
        lkir_rx_start(d);
        return false;
    }
    d->tx_last = false;
    d->rh = d->rt = 0; // whatever was received before our own frame is stale (a half duplex exchange)
    phy_tx_init(&d->ptx, d->txb, n);
    furi_hal_infrared_async_tx_set_data_isr_callback(lkir_tx_data, d);
    furi_hal_infrared_async_tx_start(LK_CARRIER_HZ, LK_DUTY);
    d->tx_on = true;
    return true;
}

static int lkir_rx(void* ctx) {
    LkIr* d = ctx;
    if(d->rt == d->rh) return -1;
    const uint8_t b = d->ring[d->rt];
    d->rt = (uint8_t)((d->rt + 1u) % LKIR_RING);
    return b;
}

const LinkChan* link_chan_open(void) {
    if(g_lkir) return &g_lkir->ch;
    if(furi_hal_infrared_is_busy()) {
        FURI_LOG_E(LKIR_TAG, "the infrared port is busy");
        return NULL;
    }
    LkIr* d = calloc(1, sizeof(LkIr));
    if(!d) return NULL;
    d->ch = (LinkChan){.ctx = d, .now_ms = lkir_now, .tx = lkir_tx, .tx_busy = lkir_busy, .rx = lkir_rx, .rand32 = lkir_rand};
    g_lkir = d;
    lkir_rx_start(d);
    return &d->ch;
}

void link_chan_close(void) {
    LkIr* d = g_lkir;
    if(!d) return;
    g_lkir = NULL;
    if(d->tx_on) {
        furi_hal_infrared_async_tx_stop();
        d->tx_on = false;
    }
    lkir_rx_stop(d);
    free(d);
}

// The self-test (dev): one real frame goes out of the LED and its duration is measured against the codec's own airtime figure. A reception loop-back is NOT attempted: the HAL
// stops the receiver while it transmits, and the Flipper cannot be shown to hear its own LED.
bool link_ir_selftest(uint32_t* bytes_per_s) {
    const LinkChan* ch = link_chan_open();
    if(!ch) return false;
    uint8_t payload[33], frame[LK_FRAME_MAX];
    for(unsigned i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i * 37u + 11u);
    const uint16_t n = lk_frame_build(frame, 0, 0, payload, sizeof(payload));
    const uint32_t want_us = phy_airtime_us(frame, n);
    const uint32_t t0 = furi_get_tick();
    bool ok = ch->tx(ch->ctx, frame, n);
    while(ok && ch->tx_busy(ch->ctx) && furi_get_tick() - t0 < 2000u) furi_delay_ms(1);
    const uint32_t ms = furi_get_tick() - t0;
    FURI_LOG_I(LKIR_TAG, "selftest: %u bytes took %u ms on the air, the codec says %u us", (unsigned)n, (unsigned)ms, (unsigned)want_us);
    link_chan_close();
    if(ok && ms) *bytes_per_s = (uint32_t)n * 1000u / ms;
    return ok && ms * 1000u > want_us * 85u / 100u && ms * 1000u < want_us * 115u / 100u + 20000u;
}
