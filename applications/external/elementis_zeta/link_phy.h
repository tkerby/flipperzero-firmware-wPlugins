#pragma once
// IR LINK, physical layer (contract section 45): bytes <-> the mark / space timings of the Flipper's infrared port. No furi.h: the device driver (link_ir.c) feeds
// phy_rx_edge() from the HAL's capture ISR and pulls phy_tx_next() from its TX-data ISR; the host tests run the very same codec through a simulated channel with jitter.
//
// A burst (one frame, link_proto.h) is   LEADER  then 4 symbols per byte (MSB first, 2 bits per symbol)  then a STOP mark:
//   leader   = a mark of LK_LEAD_MARK_US and a space of LK_LEAD_SPACE_US (a burst start no data symbol can imitate, so a receiver that starts in the middle of
//              something else, or hears a TV remote, never aligns on garbage by accident)
//   symbol s = a mark of LK_MARK_US followed by a space of LK_BASE_SPACE_US + s * LK_STEP_US   (s = 0..3)
//   stop     = a last mark (the receiver measures a space only when the next mark begins, so the last symbol needs one)
// The receiver classifies a symbol by its PERIOD (mark + space), not by the space alone: an IR receiver module (TSOP-like) stretches marks and shortens spaces
// by up to ~150 us, but the sum stays put, so the thresholds sit half a step from each nominal period. Marks are carrier bursts (38 kHz, 1/3 duty).
// Nominal rate: the average symbol lasts 500 + (500 + 1.5 * 500) = 1,750 us for 2 bits = 1,143 bit/s raw (143 B/s); a 40-byte frame takes ~0.29 s.
#include <stdint.h>
#include <stdbool.h>

#define LK_CARRIER_HZ    38000u
#define LK_DUTY          0.33f
#define LK_LEAD_MARK_US  3000u
#define LK_LEAD_SPACE_US 1500u
#define LK_MARK_US       500u
#define LK_BASE_SPACE_US 500u
#define LK_STEP_US       500u
#define LK_PERIOD0_US    (LK_MARK_US + LK_BASE_SPACE_US) // the period of symbol 0
#define LK_RX_SILENCE_US \
    6000u // no edge for this long = the burst is over (the receiver resets; the longest gap inside a burst is 2,000 us)

// ---- transmitter: one (duration, level) element per call, level true = mark
typedef struct {
    const uint8_t* buf;
    uint16_t n; // bytes
    uint16_t
        i; // element index (0 = leader mark, 1 = leader space, 2 + 2 * k = mark of symbol k, 3 + 2 * k = its space, 2 + 8 * n = the stop mark)
} PhyTx;
enum {
    PHY_TX_MORE = 0,
    PHY_TX_LAST = 1
};
void phy_tx_init(PhyTx* t, const uint8_t* buf, uint16_t n);
// Next element. Returns PHY_TX_LAST with the final one (the stop mark).
int phy_tx_next(PhyTx* t, uint32_t* dur_us, bool* mark);
// Total on-air time of a burst of n bytes, in microseconds: exact for the bytes in buf, or the average byte when buf is NULL.
uint32_t phy_airtime_us(const uint8_t* buf, uint16_t n);

// ---- receiver
typedef struct {
    uint8_t
        st; // 0 idle, 1 after the leader mark, 2 expecting a symbol's mark, 3 expecting its space
    uint8_t nsym; // symbols of the byte in progress
    uint8_t acc; // the byte in progress
    uint16_t mark; // the last mark's width (saturated), us
    uint32_t errors; // bursts abandoned because of a timing that fits nothing (diagnostics)
} PhyRx;
void phy_rx_init(PhyRx* r);
// One interval that just ended: `mark` true = carrier present (the receiver's output low), `dur_us` its length. Returns the byte completed by this edge (0..255), or -1.
int phy_rx_edge(PhyRx* r, bool mark, uint32_t dur_us);
// Silence (no edge for LK_RX_SILENCE_US): the burst ended; whatever was half-received is dropped.
void phy_rx_silence(PhyRx* r);

// The self-test of the codec: a round trip of 64 pseudo-random bytes through the transmitter's timings and the receiver, with a period error of `jitter_us` on every symbol (every space
// longer or shorter by `jitter_us`). True when every byte came back; *air_us = the on-air time of the 64 bytes.
bool phy_selftest(uint32_t jitter_us, uint32_t* air_us);
