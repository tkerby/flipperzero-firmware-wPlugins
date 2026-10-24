#pragma once
// UART link to the Flipper. Receives AUDIO_DATA into the stream ring buffer,
// handles TRACK_META / FLUSH / SET_SSID / PING, and sends CREDIT / STATS / PONG.
// See ../../protocol.md.

#include <stdbool.h>
#include <stdint.h>

void uart_link_start(void);

// Link health. protocol.md asks that a flaky link be visible on the station
// console rather than only sounding bad, so these are readable by /status.
typedef struct {
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t oversize_errors;
    uint32_t resyncs; // non-start bytes skipped while hunting for a frame
    uint32_t truncated; // frames abandoned on an idle gap
    uint64_t audio_bytes; // total AUDIO_DATA bytes handed to the stream hub
    bool ever_received; // false until the first good frame — "is the Flipper there?"
} UartLinkStats;

void uart_link_get_stats(UartLinkStats* out);
