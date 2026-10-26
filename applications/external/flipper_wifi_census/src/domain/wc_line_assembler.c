#include "include/domain/wc_line_assembler.h"

void wc_lineasm_init(WcLineAsm *a) {
    a->len = 0;
    a->overflow = false;
}

static void emit(WcLineAsm *a, WcLineFn cb, void *ctx) {
    if (!a->overflow && a->len > 0) {
        a->buf[a->len] = '\0';
        cb(ctx, a->buf, a->len);
    }
    a->len = 0;
    a->overflow = false;
}

void wc_lineasm_feed(WcLineAsm *a, const uint8_t *data, size_t n, WcLineFn cb, void *ctx) {
    for (size_t i = 0; i < n; i++) {
        char c = (char)data[i];
        if (c == '\n') {
            emit(a, cb, ctx);
        } else if (c == '\r') {
            continue;
        } else if (a->len < WC_LINE_MAX - 1) {
            a->buf[a->len++] = c;
        } else {
            // Line too long: drop it whole until the next newline resyncs us.
            a->overflow = true;
        }
    }
}
