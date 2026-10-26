#include "include/platform/wc_serial_furi.h"

#include "include/domain/wc_line_assembler.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <string.h>

#define WC_RX_STREAM_SIZE 512
#define WORKER_FLAG_DATA (1u << 0)
#define WORKER_FLAG_STOP (1u << 1)

// Marauder serial CLI commands.
static const char *k_cmd_start = "sniffprobe\n";
static const char *k_cmd_stop = "stopscan\n";

struct WcSerialFuri {
    uint32_t baud;
    FuriHalSerialHandle *handle;
    FuriStreamBuffer *stream;
    FuriThread *thread;
    WcLineAsm assembler;
    WcSerialLineFn on_line;
    void *cb_ctx;
    bool running;
};

// ISR context: pull each received byte and hand it to the worker via the stream buffer.
static void rx_isr(FuriHalSerialHandle *handle, FuriHalSerialRxEvent event, void *context) {
    if (event & FuriHalSerialRxEventData) {
        WcSerialFuri *s = context;
        uint8_t b = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(s->stream, &b, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(s->thread), WORKER_FLAG_DATA);
    }
}

static int32_t worker(void *context) {
    WcSerialFuri *s = context;
    uint8_t chunk[64];
    while (true) {
        uint32_t flags = furi_thread_flags_wait(WORKER_FLAG_DATA | WORKER_FLAG_STOP,
                                                FuriFlagWaitAny, FuriWaitForever);
        if (flags & WORKER_FLAG_STOP) {
            break;
        }
        size_t got;
        while ((got = furi_stream_buffer_receive(s->stream, chunk, sizeof(chunk), 0)) > 0) {
            wc_lineasm_feed(&s->assembler, chunk, got, s->on_line, s->cb_ctx);
        }
    }
    return 0;
}

static bool serial_start(void *self, WcSerialLineFn on_line, void *ctx) {
    WcSerialFuri *s = self;
    if (s->running) {
        return true;
    }
    // Acquire the USART first. It returns NULL when another service already holds it (commonly
    // the firmware's Expansion Modules service, or the CLI on the GPIO pins). Doing this before
    // any allocation means a busy port costs nothing, leaks nothing, and is reported instead of
    // crashing the app.
    s->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if (!s->handle) {
        return false;
    }

    s->on_line = on_line;
    s->cb_ctx = ctx;
    wc_lineasm_init(&s->assembler);

    s->stream = furi_stream_buffer_alloc(WC_RX_STREAM_SIZE, 1);
    s->thread = furi_thread_alloc_ex("WcSerialRx", 1024, worker, s);
    furi_thread_start(s->thread);

    furi_hal_serial_init(s->handle, s->baud);
    furi_hal_serial_async_rx_start(s->handle, rx_isr, s, false);

    furi_hal_serial_tx(s->handle, (const uint8_t *)k_cmd_start, strlen(k_cmd_start));
    s->running = true;
    return true;
}

static void serial_stop(void *self) {
    WcSerialFuri *s = self;
    if (!s->running) {
        return;
    }
    furi_hal_serial_tx(s->handle, (const uint8_t *)k_cmd_stop, strlen(k_cmd_stop));
    furi_hal_serial_tx_wait_complete(s->handle);
    furi_hal_serial_async_rx_stop(s->handle);
    furi_hal_serial_deinit(s->handle);
    furi_hal_serial_control_release(s->handle);
    s->handle = NULL;

    furi_thread_flags_set(furi_thread_get_id(s->thread), WORKER_FLAG_STOP);
    furi_thread_join(s->thread);
    furi_thread_free(s->thread);
    s->thread = NULL;

    furi_stream_buffer_free(s->stream);
    s->stream = NULL;
    s->running = false;
}

WcSerialFuri *wc_serial_furi_alloc(uint32_t baud) {
    WcSerialFuri *s = malloc(sizeof(WcSerialFuri));
    if (!s) {
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->baud = baud;
    return s;
}

void wc_serial_furi_free(WcSerialFuri *s) {
    if (!s) {
        return;
    }
    if (s->running) {
        serial_stop(s);
    }
    free(s);
}

WcSerialPort wc_serial_furi_port(WcSerialFuri *s) {
    WcSerialPort p = {.self = s, .start = serial_start, .stop = serial_stop};
    return p;
}
