#pragma once

// The parts of the MCP2518FD driver (LilyGO T-2CAN FD, can1) that need no
// hardware: the crystal check, the bit-timing check, the capture filters and
// the classic-CAN-only frame conversion. Host-tested by
// test/test_can_mcp2518fd.cpp; can_driver.cpp holds the SPI side.

#include "can_capture_filter.h"
#include "../../fsd_logic/fsd_can_ops.h"  // TESLA_CAN_MAX_DLC / tesla_can_tx_valid
#include <string.h>

// MCP2518FD registers the driver touches itself (DS20006027B, Table 3-2). The
// time base counter is not latched across its four bytes.
static constexpr uint16_t MCP2518FD_REG_TBC    = 0x010;
static constexpr uint16_t MCP2518FD_REG_TSCON  = 0x014;
static constexpr uint16_t MCP2518FD_REG_BDIAG0 = 0x038;
static constexpr uint32_t MCP2518FD_TSCON_TBCEN = 1u << 16;

// The crystal check runs the time base counter at SYSCLK / 1024: one count
// every 25.6 us at 40 MHz, so it moves by about one count during an SPI read.
static constexpr uint32_t MCP2518FD_TBC_PRESCALE = 1024;

// Two reads back to back must be this close: a read that tore across a carry
// between bytes is off by at least 255 counts.
inline bool mcp2518fd_tbc_steady(uint32_t first, uint32_t second) {
    return second - first <= 4u;
}

// SYSCLK in Hz from a TBC delta taken over elapsed_us microseconds.
inline uint32_t mcp2518fd_sysclk_hz(uint32_t ticks, uint32_t elapsed_us) {
    if (elapsed_us == 0) return 0;
    uint64_t hz = (uint64_t)ticks * MCP2518FD_TBC_PRESCALE * 1000000u / elapsed_us;
    return hz > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)hz;
}

// The crystal the build expects, within 2 %. The chip takes 20 or 40 MHz, a
// factor of two apart, and the measurement itself is good to well under 1 %.
inline bool mcp2518fd_osc_matches(uint32_t measured_hz, uint32_t expected_hz) {
    uint32_t diff = measured_hz > expected_hz ? measured_hz - expected_hz
                                              : expected_hz - measured_hz;
    return expected_hz != 0 && diff <= expected_hz / 50u;
}

// CiBDIAG0: four 8-bit error counters (nominal RX/TX, data RX/TX), each
// incremented once per error and cleared by writing zero.
inline uint32_t mcp2518fd_bdiag0_errors(uint32_t bdiag0) {
    return (bdiag0 & 0xFFu) + ((bdiag0 >> 8) & 0xFFu) + ((bdiag0 >> 16) & 0xFFu) + (bdiag0 >> 24);
}

// Nominal bit timing the driver accepts from the library before it starts the
// controller: exactly 500 kbit/s and a sample point between 75 and 87.5 %.
// tseg1/tseg2/sjw in time quanta (CiNBTCFG field + 1); one quantum of sync.
inline bool mcp2518fd_timing_ok(uint32_t sysclk_hz, uint32_t brp, uint32_t tseg1,
                                uint32_t tseg2, uint32_t sjw) {
    if (brp < 1 || brp > 256 || tseg1 < 2 || tseg1 > 256 || tseg2 < 1 || tseg2 > 128) return false;
    if (sjw < 1 || sjw > tseg2) return false;
    uint32_t tq = 1 + tseg1 + tseg2;
    if ((uint64_t)brp * tq * 500000u != sysclk_hz) return false;
    uint32_t sample_permille = (1 + tseg1) * 1000u / tq;
    return sample_permille >= 750 && sample_permille <= 875;
}

// Capture filters: one exact filter per standard id, or a single pass-all
// filter (standard and extended) when count is zero, matching the MCP2515
// restore. Filters only load in Configuration mode, so the caller restarts
// the controller with them.
template <typename Filters, typename Format>
void mcp2518fd_capture_filters(Filters &filters, Format standard,
                               const uint32_t *ids, uint8_t count) {
    if (!ids) count = 0;
    if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
    if (count == 0) {
        filters.appendPassAllFilter(nullptr);
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        filters.appendFrameFilter(standard, ids[i] & 0x7FFu, nullptr);
    }
}

// A received message as CanFrame, classic CAN only. CAN FD frames (the chip
// takes them in Listen-Only) and classic frames with DLC 9..15 (the library
// reports those as 12..64 bytes) return false and are counted as rejected.
// Remote frames carry no payload, so their data stays zero.
template <typename Msg>
bool mcp2518fd_rx_frame(const Msg &msg, CanFrame &frame) {
    bool remote = msg.type == Msg::CAN_REMOTE;
    if (!remote && msg.type != Msg::CAN_DATA) return false;
    if (msg.len > TESLA_CAN_MAX_DLC) return false;
    memset(&frame, 0, sizeof(frame));
    frame.id  = msg.id & (msg.ext ? 0x1FFFFFFFu : 0x7FFu);
    frame.ext = msg.ext ? 1u : 0u;
    frame.req = remote ? 1u : 0u;
    frame.dlc = msg.len;
    if (!remote) memcpy(frame.data, msg.data, frame.dlc);
    return true;
}

// The only message the driver hands to the controller: a classic data frame
// with a standard id and at most 8 bytes, never CAN FD (the library's default
// message type is CAN FD with bit rate switching).
template <typename Msg>
bool mcp2518fd_tx_message(const CanFrame &frame, Msg &msg) {
    if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
    msg.id   = frame.id;
    msg.ext  = false;
    msg.type = Msg::CAN_DATA;
    msg.idx  = 0;  // transmit FIFO, not the TXQ
    msg.len  = frame.dlc;
    memcpy(msg.data, frame.data, frame.dlc);
    return true;
}
