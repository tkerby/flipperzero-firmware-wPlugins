#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Called once per complete line received from the ESP32 (no trailing newline). `line` is
// only valid for the duration of the call.
typedef void (*WcSerialLineFn)(void *ctx, const char *line, size_t len);

// Port: the serial link to the ESP32 Marauder board. `start` begins probe sniffing and
// delivers each received line to `on_line`; `stop` ends it. Adapter (platform/) wraps
// furi_hal_serial and reassembles lines; tests inject canned lines through a fake.
typedef struct {
    void *self;
    // Returns false when the link could not be opened — typically the USART is already held by
    // another service (the firmware's Expansion Modules service, or the CLI on the GPIO pins).
    // Nothing is allocated or started in that case, so the caller can simply report it.
    bool (*start)(void *self, WcSerialLineFn on_line, void *ctx);
    void (*stop)(void *self);
} WcSerialPort;
