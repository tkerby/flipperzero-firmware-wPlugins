#include "radio_uart.h"

#include <expansion/expansion.h>
#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define TAG "RadioUart"

#define UART_BAUD 921600

// Audio arrives in bursts and the worker may be descheduled behind the pacer;
// this is ~4 full frames of slack, which is plenty at 921600.
#define RX_STREAM_SIZE 2048
#define RX_BATCH       64

#define FLAG_DATA  (1u << 0)
#define FLAG_IDLE  (1u << 1)
#define FLAG_ERROR (1u << 2)
#define FLAG_STOP  (1u << 3)
#define FLAG_ALL   (FLAG_DATA | FLAG_IDLE | FLAG_ERROR | FLAG_STOP)

struct RadioUart {
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    FuriStreamBuffer* rx_stream;
    FuriEventFlag* flags;
    FuriThread* worker;
    RadioDecoder decoder;
    RadioUartFrameCallback callback;
    void* context;
    uint32_t link_errors;

    // TX is serialized: the pacer thread sends audio while the UI thread can
    // send PING or SET_SSID, and two frames interleaved on the wire is garbage.
    FuriMutex* tx_lock;
};

// ISR context: do the absolute minimum. No logging, no allocation, no locks.
static void radio_uart_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* ctx) {
    RadioUart* uart = ctx;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t byte = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(uart->rx_stream, &byte, 1, 0);
        }
        furi_event_flag_set(uart->flags, FLAG_DATA);
    }

    // The UART's own idle-line detect is exactly the "sender stopped mid-frame"
    // signal protocol.md calls for, and it costs nothing: no software timer,
    // and no risk of the timer firing late under load. 0xAA occurs naturally
    // inside MP3 data, so this gap is the ONLY reliable resync point.
    if(event & FuriHalSerialRxEventIdle) {
        furi_event_flag_set(uart->flags, FLAG_IDLE);
    }

    if(event & (FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError |
                FuriHalSerialRxEventOverrunError | FuriHalSerialRxEventParityError)) {
        uart->link_errors++;
        furi_event_flag_set(uart->flags, FLAG_ERROR);
    }
}

static int32_t radio_uart_worker(void* ctx) {
    RadioUart* uart = ctx;
    uint8_t batch[RX_BATCH];

    while(true) {
        uint32_t flags =
            furi_event_flag_wait(uart->flags, FLAG_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) break;
        if(flags & FLAG_STOP) break;

        // Drain everything buffered before acting on an idle gap, so bytes that
        // arrived before the gap are still fed to the decoder in order.
        size_t n;
        while((n = furi_stream_buffer_receive(uart->rx_stream, batch, sizeof(batch), 0)) > 0) {
            for(size_t i = 0; i < n; i++) {
                if(radio_decoder_feed(&uart->decoder, batch[i]) && uart->callback) {
                    uart->callback(
                        uart->decoder.type,
                        uart->decoder.payload,
                        uart->decoder.len,
                        uart->context);
                }
            }
        }

        if(flags & FLAG_IDLE) radio_decoder_reset(&uart->decoder);
    }
    return 0;
}

RadioUart* radio_uart_open(RadioUartFrameCallback callback, void* context) {
    RadioUart* uart = malloc(sizeof(RadioUart));
    memset(uart, 0, sizeof(RadioUart));
    uart->callback = callback;
    uart->context = context;
    radio_decoder_init(&uart->decoder);
    uart->tx_lock = furi_mutex_alloc(FuriMutexTypeNormal);

    // MUST come before acquiring the serial port, or the expansion service
    // keeps ownership of the pins. Restored in radio_uart_close().
    uart->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(uart->expansion);

    uart->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!uart->serial) {
        FURI_LOG_E(TAG, "USART busy");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        furi_mutex_free(uart->tx_lock);
        free(uart);
        return NULL;
    }

    uart->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);
    uart->flags = furi_event_flag_alloc();

    uart->worker = furi_thread_alloc_ex("RadioUartRx", 2048, radio_uart_worker, uart);
    furi_thread_start(uart->worker);

    furi_hal_serial_init(uart->serial, UART_BAUD);
    // report_errors = true: a noisy link should be visible, not silent.
    furi_hal_serial_async_rx_start(uart->serial, radio_uart_rx_isr, uart, true);

    FURI_LOG_I(TAG, "link up at %d baud", UART_BAUD);
    return uart;
}

void radio_uart_close(RadioUart* uart) {
    if(!uart) return;

    furi_hal_serial_async_rx_stop(uart->serial);
    furi_hal_serial_deinit(uart->serial);

    furi_event_flag_set(uart->flags, FLAG_STOP);
    furi_thread_join(uart->worker);
    furi_thread_free(uart->worker);

    furi_hal_serial_control_release(uart->serial);
    // Restore expansion support immediately after releasing, per the HAL docs —
    // otherwise every other app loses its expansion module.
    expansion_enable(uart->expansion);
    furi_record_close(RECORD_EXPANSION);

    furi_event_flag_free(uart->flags);
    furi_stream_buffer_free(uart->rx_stream);
    furi_mutex_free(uart->tx_lock);
    free(uart);
}

bool radio_uart_send(RadioUart* uart, uint8_t type, const uint8_t* payload, uint16_t len) {
    if(!uart) return false;
    uint8_t frame[RADIO_MAX_FRAME];
    size_t n = radio_frame_encode(type, payload, len, frame, sizeof(frame));
    if(!n) return false;

    furi_mutex_acquire(uart->tx_lock, FuriWaitForever);
    furi_hal_serial_tx(uart->serial, frame, n);
    furi_mutex_release(uart->tx_lock);
    return true;
}

void radio_uart_stats(RadioUart* uart, uint32_t* frames_ok, uint32_t* errors) {
    if(!uart) return;
    if(frames_ok) *frames_ok = uart->decoder.frames_ok;
    if(errors)
        *errors = uart->link_errors + uart->decoder.crc_errors + uart->decoder.truncated +
                  uart->decoder.oversize_errors;
}
