#include "hub_transport.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>
#include "hub_mem.h"

/* CDC channel 0 stays the normal CLI, so ufbt and qFlipper keep working. */
#define HUB_CDC_CH 1

/* Packets shorter than the 64-byte endpoint end the host's read at once. */
#define HUB_USB_PACKET 63

#define HUB_FLAG_RX (1UL << 0)

#define HUB_TX_TIMEOUT_MS     1000
#define HUB_HEADER_TIMEOUT_MS 500

struct HubTransport {
    FuriThreadId thread;
    FuriSemaphore* tx_ready;
    FuriHalUsbInterface* usb_prev;

    uint8_t rx_buf[CDC_DATA_SZ];
    size_t rx_len;
    size_t rx_pos;

    uint16_t data_left; /* bytes left in the current DATA frame */
    bool peer_closed;

    uint8_t tx_buf[4 + HUB_FRAME_MAX_PAYLOAD];
};

static void hub_cdc_tx(void* ctx) {
    HubTransport* t = ctx;
    furi_semaphore_release(t->tx_ready);
}

static void hub_cdc_rx(void* ctx) {
    HubTransport* t = ctx;
    furi_thread_flags_set(t->thread, HUB_FLAG_RX);
}

static const CdcCallbacks hub_cdc_callbacks = {
    .tx_ep_callback = hub_cdc_tx,
    .rx_ep_callback = hub_cdc_rx,
    .state_callback = NULL,
    .ctrl_line_callback = NULL,
    .config_callback = NULL,
};

HubTransport* hub_transport_alloc(void) {
    HubTransport* t = malloc(sizeof(HubTransport));
    furi_check(t); /* at app start; nothing to clean up yet */
    memset(t, 0, sizeof(HubTransport));
    t->thread = furi_thread_get_current_id();
    t->tx_ready = furi_semaphore_alloc(1, 1);

    /* Switch USB to two serial ports: CLI on channel 0, hub on channel 1.
     * The CLI stays enabled; its callbacks on channel 0 survive the switch. */
    t->usb_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_cdc_dual, NULL));
    furi_hal_cdc_set_callbacks(HUB_CDC_CH, (CdcCallbacks*)&hub_cdc_callbacks, t);

    return t;
}

void hub_transport_free(HubTransport* t) {
    furi_hal_cdc_set_callbacks(HUB_CDC_CH, NULL, NULL);
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(t->usb_prev, NULL));

    furi_semaphore_free(t->tx_ready);
    free(t);
}

static bool hub_rx_byte(HubTransport* t, uint8_t* out, uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    while(t->rx_pos >= t->rx_len) {
        int32_t n = furi_hal_cdc_receive(HUB_CDC_CH, t->rx_buf, sizeof(t->rx_buf));
        if(n > 0) {
            t->rx_len = n;
            t->rx_pos = 0;
            break;
        }
        uint32_t elapsed = furi_get_tick() - start;
        if(elapsed >= timeout_ms) return false;
        /* Poll as well as wait, so a missed flag costs at most 20 ms. */
        furi_thread_flags_wait(HUB_FLAG_RX, FuriFlagWaitAny, MIN(20UL, timeout_ms - elapsed));
    }
    *out = t->rx_buf[t->rx_pos++];
    return true;
}

static bool hub_tx_raw(HubTransport* t, const uint8_t* data, size_t len) {
    while(len > 0) {
        size_t n = MIN(len, (size_t)HUB_USB_PACKET);
        if(furi_semaphore_acquire(t->tx_ready, HUB_TX_TIMEOUT_MS) != FuriStatusOk) {
            return false;
        }
        furi_hal_cdc_send(HUB_CDC_CH, (uint8_t*)data, n);
        data += n;
        len -= n;
    }
    return true;
}

static bool hub_tx_frame(HubTransport* t, uint8_t type, const void* payload, size_t len) {
    furi_check(len <= HUB_FRAME_MAX_PAYLOAD);
    t->tx_buf[0] = HUB_FRAME_SYNC;
    t->tx_buf[1] = type;
    t->tx_buf[2] = len & 0xFF;
    t->tx_buf[3] = (len >> 8) & 0xFF;
    if(len) memcpy(t->tx_buf + 4, payload, len);
    return hub_tx_raw(t, t->tx_buf, 4 + len);
}

/* Read the next valid frame header. Skips noise until a sync byte. */
static bool hub_rx_header(HubTransport* t, uint8_t* type, uint16_t* len, uint32_t timeout_ms) {
    uint8_t b;
    do {
        if(!hub_rx_byte(t, &b, timeout_ms)) return false;
    } while(b != HUB_FRAME_SYNC);

    uint8_t lo, hi;
    if(!hub_rx_byte(t, type, HUB_HEADER_TIMEOUT_MS)) return false;
    if(!hub_rx_byte(t, &lo, HUB_HEADER_TIMEOUT_MS)) return false;
    if(!hub_rx_byte(t, &hi, HUB_HEADER_TIMEOUT_MS)) return false;
    *len = lo | (hi << 8);

    bool known = *type == HUB_FRAME_OPEN || *type == HUB_FRAME_DATA ||
                 *type == HUB_FRAME_CLOSE || *type == HUB_FRAME_PING;
    return known && *len <= HUB_FRAME_MAX_PAYLOAD;
}

static void hub_rx_skip(HubTransport* t, uint16_t len) {
    uint8_t b;
    while(len-- > 0) {
        if(!hub_rx_byte(t, &b, HUB_HEADER_TIMEOUT_MS)) return;
    }
}

bool hub_transport_accept(HubTransport* t, uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < timeout_ms) {
        uint8_t type;
        uint16_t len;
        if(!hub_rx_header(t, &type, &len, timeout_ms)) continue;
        hub_rx_skip(t, len);

        if(type == HUB_FRAME_PING) {
            hub_tx_frame(t, HUB_FRAME_PONG, NULL, 0);
        } else if(type == HUB_FRAME_OPEN) {
            t->data_left = 0;
            t->peer_closed = false;
            return true;
        }
        /* DATA or CLOSE left over from an old connection: drop it. */
    }
    return false;
}

size_t hub_conn_read(HubTransport* t, uint8_t* buf, size_t len, uint32_t timeout_ms) {
    if(t->peer_closed) return 0;

    while(t->data_left == 0) {
        uint8_t type;
        uint16_t flen;
        if(!hub_rx_header(t, &type, &flen, timeout_ms)) return 0;
        if(type == HUB_FRAME_DATA) {
            t->data_left = flen;
        } else {
            hub_rx_skip(t, flen);
            if(type == HUB_FRAME_PING) {
                hub_tx_frame(t, HUB_FRAME_PONG, NULL, 0);
            } else {
                t->peer_closed = true;
                return 0;
            }
        }
    }

    size_t n = 0;
    size_t want = MIN(len, (size_t)t->data_left);
    while(n < want) {
        if(!hub_rx_byte(t, &buf[n], timeout_ms)) break;
        n++;
        /* Hand back what is already buffered, do not wait for more. */
        if(t->rx_pos >= t->rx_len) break;
    }
    t->data_left -= n;
    return n;
}

bool hub_conn_write(HubTransport* t, const void* data, size_t len) {
    const uint8_t* p = data;
    while(len > 0) {
        size_t n = MIN(len, (size_t)HUB_FRAME_MAX_PAYLOAD);
        if(!hub_tx_frame(t, HUB_FRAME_DATA, p, n)) return false;
        p += n;
        len -= n;
    }
    return true;
}

void hub_conn_close(HubTransport* t) {
    hub_tx_frame(t, HUB_FRAME_CLOSE, NULL, 0);
}

void hub_transport_service(HubTransport* t) {
    uint8_t type;
    uint16_t len;
    while(hub_rx_header(t, &type, &len, 0)) {
        hub_rx_skip(t, len);
        if(type == HUB_FRAME_PING) {
            hub_tx_frame(t, HUB_FRAME_PONG, NULL, 0);
        } else if(type == HUB_FRAME_CLOSE) {
            t->peer_closed = true;
        }
    }
}
