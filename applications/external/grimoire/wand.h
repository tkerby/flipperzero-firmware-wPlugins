// The wand's frame: a Sub-GHz blessing carrying the planetary hour, and the
// answer to one. On-off keying, pulse-width coded, so a Flipper, an RTL-SDR or
// anything that can time pulses can read it. Pure C.
//
//   sync     high 2000 us, low 6000 us
//   32 bits  most significant first; 1 = high 1200 low 400, 0 = high 400 low 1200
//   tail     high 400 us, low 4000 us
//
// The 32 bits: magic 0xB7 (8), kind (1: 0 blessing, 1 answer), planet (3, in the
// Chaldean order Saturn..Moon), day (1), hour index 1..12 (4), minute of the
// local day 0..1439 (11), and a check (4): the sum of the seven nibbles before
// it, modulo 16. A frame is sent five times over.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define WAND_MAGIC   0xB7
#define WAND_REPEATS 5
#define WAND_TE      400 // microseconds

typedef struct {
    bool answer;
    uint8_t planet; // 0..6
    bool is_day;
    uint8_t index; // 1..12
    uint16_t minute; // 0..1439
} WandMessage;

typedef struct {
    bool level;
    uint16_t duration; // microseconds
} WandPulse;

// Sync + 32 bits + tail, times the repeats, plus an end marker
#define WAND_PULSES ((2 + 64 + 2) * WAND_REPEATS)

uint32_t wand_pack(const WandMessage* m);
bool wand_unpack(uint32_t word, WandMessage* m);

// Fills pulses (WAND_PULSES of them) for the message. Returns the count.
size_t wand_encode(const WandMessage* m, WandPulse* pulses);

typedef struct {
    uint8_t state; // 0 idle, 1 after the sync high, 2 in the bits
    uint8_t bits;
    uint32_t word;
    bool have_high;
    uint32_t high;
} WandDecoder;

void wand_decoder_reset(WandDecoder* d);
// Feed one pulse. True when a whole valid frame has just been read into m.
bool wand_decoder_feed(WandDecoder* d, bool level, uint32_t duration, WandMessage* m);
