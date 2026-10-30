#pragma once
// UART link to the ESP32 devboard. Owns the expansion lock, the serial handle,
// and the RX worker that turns bytes into whole frames.
//
// Ported from flipper-usb-lab's usb_uart.c, which is proven on hardware.

#include <stdbool.h>
#include <stdint.h>

#include "radio_proto.h"

// Called from the worker thread (NOT an ISR) once per complete, CRC-valid
// frame. `payload` is only valid for the duration of the call.
typedef void (
    *RadioUartFrameCallback)(uint8_t type, const uint8_t* payload, uint16_t len, void* ctx);

typedef struct RadioUart RadioUart;

// Acquires the USART. Returns NULL if another app holds it (qFlipper counts).
RadioUart* radio_uart_open(RadioUartFrameCallback callback, void* context);

// Releases the USART and restores expansion-module support.
void radio_uart_close(RadioUart* uart);

bool radio_uart_send(RadioUart* uart, uint8_t type, const uint8_t* payload, uint16_t len);

// Link health for the status line: frames seen, and bytes the link mangled.
void radio_uart_stats(RadioUart* uart, uint32_t* frames_ok, uint32_t* errors);
