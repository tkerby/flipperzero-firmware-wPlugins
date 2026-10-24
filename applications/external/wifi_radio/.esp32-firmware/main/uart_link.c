#include "uart_link.h"

#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "soc/uart_pins.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "radio_proto.h"
#include "ssid_name.h"
#include "stream_hub.h"
#include "wifi_ap.h"

static const char* TAG = "uart_link";

// UART0 is the only port the devboard brings out to the Flipper's header, and
// this link owns it outright: the console lives on the S2's native USB instead
// (CONFIG_ESP_CONSOLE_USB_CDC), so no log line can ever land inside a frame.
#define LINK_UART UART_NUM_0
#define LINK_BAUD 921600

// The driver fills this behind our back. At 921600 the line delivers ~90 KB/s,
// so 8 KB is ~90 ms of audio — comfortably more than the 20 ms the reader ever
// blocks for, which keeps a busy WiFi moment from costing us bytes.
#define LINK_RX_BUF 8192
#define LINK_TX_BUF 2048

#define LINK_READ_CHUNK      512
#define LINK_READ_TIMEOUT_MS 20

// Credit is the overflow backstop, not the clock — the Flipper paces audio at
// the file's CBR rate. 250 ms is ~4 KB of a 128 kbps stream against a 64 KB
// ring, so the window refills long before it can run dry.
#define CREDIT_PERIOD_MS 250
#define STATS_PERIOD_MS  1000

// The decoder carries a 512-byte payload plus counters, which is more than
// belongs on a task stack.
static RadioDecoder rx_decoder;

static SemaphoreHandle_t tx_lock;
static uint64_t audio_bytes;
static bool ever_received;

// ---------------------------------------------------------------- transmit

static void link_send(uint8_t type, const void* payload, uint16_t len) {
    uint8_t frame[RADIO_MAX_FRAME];
    const size_t n = radio_frame_encode(type, payload, len, frame, sizeof(frame));
    if(n == 0) return;

    // The RX task answers PINGs while the stats task sends CREDIT/STATS, so
    // writes have to be serialized or two frames interleave on the wire.
    xSemaphoreTake(tx_lock, portMAX_DELAY);
    uart_write_bytes(LINK_UART, (const char*)frame, n);
    xSemaphoreGive(tx_lock);
}

// ---------------------------------------------------------------- receive

// Copy a wire payload out as a NUL-terminated C string. Payloads arrive as raw
// bytes with no terminator, so nothing downstream may treat them as strings
// until they have been through here.
static void payload_to_cstr(const RadioDecoder* d, char* out, size_t out_cap) {
    size_t n = d->len;
    if(n > out_cap - 1) n = out_cap - 1;
    memcpy(out, d->payload, n);
    out[n] = '\0';
}

static void link_handle_frame(const RadioDecoder* d) {
    ever_received = true;

    switch(d->type) {
    case RADIO_MSG_AUDIO_DATA:
        stream_hub_write(d->payload, d->len);
        audio_bytes += d->len;
        break;

    case RADIO_MSG_TRACK_META: {
        char json[STREAM_META_MAX];
        payload_to_cstr(d, json, sizeof(json));
        stream_hub_set_meta(json);
        break;
    }

    case RADIO_MSG_FLUSH:
        stream_hub_flush();
        break;

    case RADIO_MSG_SET_SSID: {
        char suffix[SSID_MAX];
        payload_to_cstr(d, suffix, sizeof(suffix));
        // This is only ever a suffix: wifi_ap_set_name_suffix() runs it through
        // ssid_build(), which always keeps the "Flipper Radio" prefix. Nothing
        // arriving over the wire can rename the station to impersonate another
        // network, which matters for an open AP with a captive portal.
        wifi_ap_set_name_suffix(suffix);
        break;
    }

    case RADIO_MSG_PING:
        link_send(RADIO_MSG_PONG, NULL, 0);
        break;

    default:
        // Unknown type with a good CRC: a newer Flipper build talking to older
        // firmware. Ignoring it is correct; the frame was well formed.
        break;
    }
}

static void uart_link_rx_task(void* arg) {
    (void)arg;
    uint8_t chunk[LINK_READ_CHUNK];
    int64_t last_rx_us = esp_timer_get_time();

    for(;;) {
        const int n =
            uart_read_bytes(LINK_UART, chunk, sizeof(chunk), pdMS_TO_TICKS(LINK_READ_TIMEOUT_MS));
        const int64_t now = esp_timer_get_time();

        if(n > 0) {
            last_rx_us = now;
            for(int i = 0; i < n; i++) {
                if(radio_decoder_feed(&rx_decoder, chunk[i])) link_handle_frame(&rx_decoder);
            }
            continue;
        }

        // Nothing arrived. If a frame is half-received, the sender stopped
        // mid-frame and it will never complete. We cannot recover by hunting
        // for the next 0xAA, because 0xAA occurs naturally inside MP3 data and
        // a mid-payload scan cannot tell audio from a real frame start. The gap
        // itself is the signal — real truncation always comes with one.
        if(rx_decoder.state != RadioDecodeStart &&
           (now - last_rx_us) >= (int64_t)RADIO_FRAME_IDLE_RESET_MS * 1000) {
            radio_decoder_reset(&rx_decoder);
            last_rx_us = now;
        }
    }
}

// ------------------------------------------------------- credit and stats

static void uart_link_tx_task(void* arg) {
    (void)arg;
    TickType_t last_stats = xTaskGetTickCount();
    uint64_t last_hb_audio = 0;
    uint32_t hb_ticks = 0;

    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(CREDIT_PERIOD_MS));

        const uint16_t credit = stream_hub_credit();
        const uint8_t le[2] = {(uint8_t)(credit & 0xFFu), (uint8_t)(credit >> 8)};
        link_send(RADIO_MSG_CREDIT, le, sizeof(le));

        const TickType_t now = xTaskGetTickCount();
        if((now - last_stats) >= pdMS_TO_TICKS(STATS_PERIOD_MS)) {
            last_stats = now;
            char json[STREAM_META_MAX];
            const int n = snprintf(
                json,
                sizeof(json),
                "{\"listeners\":%u,\"buf\":%u}",
                (unsigned)stream_hub_listener_count(),
                (unsigned)stream_hub_fill_percent());
            if(n > 0 && (size_t)n < sizeof(json)) link_send(RADIO_MSG_STATS, json, (uint16_t)n);

            // Heartbeat. The link used to be silent unless it was corrupting
            // frames, which left "the Flipper says it is playing but the board
            // shows no audio" with no way to tell apart a dead UART, a decoder
            // dropping every frame, and credit holding the Flipper off. Quiet
            // when the station is idle; one line a second once anything is
            // happening.
            UartLinkStats hb;
            uart_link_get_stats(&hb);
            const uint8_t listeners = stream_hub_listener_count();
            // Once a second while anything is happening, and every fifth
            // second when the station is idle -- an idle heap reading is the
            // baseline every other number has to be read against.
            hb_ticks++;
            if(listeners > 0 || hb.audio_bytes != last_hb_audio || (hb_ticks % 5) == 0) {
                ESP_LOGI(
                    TAG,
                    "hb audio=%llu (+%llu) frames=%lu crc=%lu resync=%lu trunc=%lu "
                    "credit=%u listeners=%u buf=%u%% heap=%u largest=%u",
                    (unsigned long long)hb.audio_bytes,
                    (unsigned long long)(hb.audio_bytes - last_hb_audio),
                    (unsigned long)hb.frames_ok,
                    (unsigned long)hb.crc_errors,
                    (unsigned long)hb.resyncs,
                    (unsigned long)hb.truncated,
                    (unsigned)stream_hub_credit(),
                    (unsigned)listeners,
                    (unsigned)stream_hub_fill_percent(),
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                    (unsigned)heap_caps_get_largest_free_block(
                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            }
            last_hb_audio = hb.audio_bytes;
        }
    }
}

// ---------------------------------------------------------------- lifecycle

void uart_link_get_stats(UartLinkStats* out) {
    if(!out) return;
    // Plain word-sized reads of counters only this module writes; a torn read
    // would at worst show a stale count on a status page.
    out->frames_ok = rx_decoder.frames_ok;
    out->crc_errors = rx_decoder.crc_errors;
    out->oversize_errors = rx_decoder.oversize_errors;
    out->resyncs = rx_decoder.resyncs;
    out->truncated = rx_decoder.truncated;
    out->audio_bytes = audio_bytes;
    out->ever_received = ever_received;
}

void uart_link_start(void) {
    radio_decoder_init(&rx_decoder);

    tx_lock = xSemaphoreCreateMutex();
    if(!tx_lock) {
        ESP_LOGE(TAG, "no memory for tx lock; link not started");
        return;
    }

    ESP_LOGI(TAG, "link on UART%d at %d baud", LINK_UART, LINK_BAUD);

    const uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(LINK_UART, LINK_RX_BUF, LINK_TX_BUF, 0, NULL, 0);
    if(err == ESP_OK) err = uart_param_config(LINK_UART, &cfg);
    // Set the pins explicitly rather than inheriting whatever the ROM left
    // behind: now that the console lives on USB, nothing else in the app
    // configures UART0, and the devboard wires the Flipper's header to exactly
    // these two pads.
    if(err == ESP_OK)
        err = uart_set_pin(
            LINK_UART, U0TXD_GPIO_NUM, U0RXD_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "uart setup failed: %s", esp_err_to_name(err));
        return;
    }

    // Above the default priority: the RX ring is the one thing in this firmware
    // with a hard deadline, since bytes the driver cannot store are simply lost.
    xTaskCreate(uart_link_rx_task, "link_rx", 4096, NULL, 10, NULL);
    xTaskCreate(uart_link_tx_task, "link_tx", 3072, NULL, 5, NULL);
}
