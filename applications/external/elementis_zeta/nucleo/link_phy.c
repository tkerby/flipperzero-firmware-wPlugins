// IR LINK physical layer (contract section 45): see link_phy.h.
#include <string.h>
#include "link_phy.h"

void phy_tx_init(PhyTx* t, const uint8_t* buf, uint16_t n) {
    t->buf = buf;
    t->n = n;
    t->i = 0;
}

int phy_tx_next(PhyTx* t, uint32_t* dur_us, bool* mark) {
    const uint16_t i = t->i++;
    const uint16_t last = (uint16_t)(2 + 8 * t->n);
    if(i == 0) {
        *dur_us = LK_LEAD_MARK_US;
        *mark = true;
    } else if(i == 1) {
        *dur_us = LK_LEAD_SPACE_US;
        *mark = false;
    } else if(i >= last) {
        *dur_us = LK_MARK_US; // the stop mark
        *mark = true;
    } else {
        const uint16_t k = (uint16_t)((i - 2) >> 1); // symbol number
        const uint8_t sym = (uint8_t)((t->buf[k >> 2] >> (6 - 2 * (k & 3))) & 3);
        const bool is_mark = ((i - 2) & 1) == 0;
        *dur_us = is_mark ? LK_MARK_US : LK_BASE_SPACE_US + sym * LK_STEP_US;
        *mark = is_mark;
    }
    return i >= last ? PHY_TX_LAST : PHY_TX_MORE;
}

uint32_t phy_airtime_us(const uint8_t* buf, uint16_t n) {
    uint32_t t = LK_LEAD_MARK_US + LK_LEAD_SPACE_US + LK_MARK_US; // leader + stop mark
    for(uint16_t k = 0; k < n; k++) {
        if(buf) {
            for(int s = 0; s < 4; s++) t += LK_MARK_US + LK_BASE_SPACE_US + ((buf[k] >> (6 - 2 * s)) & 3) * LK_STEP_US;
        } else {
            t += 4 * (LK_MARK_US + LK_BASE_SPACE_US) + 4 * (3 * LK_STEP_US / 2); // the average byte
        }
    }
    return t;
}

void phy_rx_init(PhyRx* r) {
    r->st = 0;
    r->nsym = 0;
    r->acc = 0;
    r->mark = 0;
    r->errors = 0;
}

void phy_rx_silence(PhyRx* r) {
    r->st = 0;
    r->nsym = 0;
    r->acc = 0;
}

static inline bool in_range(uint32_t v, uint32_t lo, uint32_t hi) {
    return v >= lo && v <= hi;
}

int phy_rx_edge(PhyRx* r, bool mark, uint32_t dur_us) {
    // A long mark is a leader whatever we were doing (a burst that starts while the last one was cut short re-aligns at once).
    if(mark && in_range(dur_us, LK_LEAD_MARK_US - 800u, LK_LEAD_MARK_US + 1000u)) {
        r->st = 1;
        r->nsym = 0;
        r->acc = 0;
        return -1;
    }
    switch(r->st) {
    case 1: // the leader's space
        if(!mark && in_range(dur_us, LK_LEAD_SPACE_US - 600u, LK_LEAD_SPACE_US + 800u)) r->st = 2;
        else phy_rx_silence(r);
        return -1;
    case 2: // a symbol's mark (or the stop mark: nothing follows it)
        if(mark) {
            r->mark = (uint16_t)(dur_us > 0xFFFFu ? 0xFFFFu : dur_us);
            r->st = 3;
        } else {
            r->errors++;
            phy_rx_silence(r);
        }
        return -1;
    case 3: { // its space: classify the period
        r->st = 2;
        if(mark) {
            r->errors++;
            phy_rx_silence(r);
            return -1;
        }
        const uint32_t p = r->mark + dur_us;
        if(p + LK_STEP_US / 2 < LK_PERIOD0_US || p >= LK_PERIOD0_US + 3 * LK_STEP_US + LK_STEP_US / 2) { // outside [P0 - step/2, P0 + 3.5 * step)
            r->errors++;
            phy_rx_silence(r);
            return -1;
        }
        const uint32_t s = (p + LK_STEP_US / 2 - LK_PERIOD0_US) / LK_STEP_US; // 0..3
        r->acc = (uint8_t)((r->acc << 2) | s);
        if(++r->nsym == 4) {
            r->nsym = 0;
            const uint8_t b = r->acc;
            r->acc = 0;
            return b;
        }
        return -1;
    }
    default: return -1; // idle: not a leader, ignore (noise, another remote)
    }
}

// Round trip of 64 pseudo-random bytes through the transmitter's elements and the receiver, with a timing error of `jitter_us` on every space (alternately longer and shorter per byte):
// the PERIOD error that the classification thresholds (+-250 us) have to absorb. (A receiver module mostly moves a mark and its space by opposite amounts, which the period keeps.)
bool phy_selftest(uint32_t jitter_us, uint32_t* air_us) {
    uint8_t buf[64], got[64];
    uint32_t x = 0x1234ABCDu;
    for(int i = 0; i < 64; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        buf[i] = (uint8_t)(x >> 11);
    }
    PhyTx t;
    PhyRx r;
    phy_tx_init(&t, buf, 64);
    phy_rx_init(&r);
    uint32_t total = 0, dur;
    bool mark;
    int n = 0, last;
    do {
        const uint16_t idx = t.i;
        last = phy_tx_next(&t, &dur, &mark);
        total += dur;
        if(idx >= 2 && idx < (uint16_t)(2 + 8 * 64) && !mark) { // every space is off by +-jitter (alternating per byte): the period error the thresholds must absorb
            const bool odd = (((idx - 2) >> 3) & 1) != 0;
            dur = odd ? dur + jitter_us : (dur > jitter_us ? dur - jitter_us : 1);
        }
        const int b = phy_rx_edge(&r, mark, dur);
        if(b >= 0 && n < 64) got[n++] = (uint8_t)b;
    } while(last == PHY_TX_MORE);
    if(air_us) *air_us = total;
    return n == 64 && memcmp(buf, got, 64) == 0 && r.errors == 0;
}
