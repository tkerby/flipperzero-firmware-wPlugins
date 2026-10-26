#pragma once

#include "include/ports/wc_serial_port.h"

#include <stdint.h>

typedef struct WcSerialFuri WcSerialFuri;

// Allocate the serial adapter for the ESP32 Marauder board on the USART lines at `baud`
// (Marauder's default is 115200). Nothing is opened until the port's start() is called.
WcSerialFuri *wc_serial_furi_alloc(uint32_t baud);
void wc_serial_furi_free(WcSerialFuri *s);

// The port view over this adapter (start begins probe sniffing, stop ends it).
WcSerialPort wc_serial_furi_port(WcSerialFuri *s);
