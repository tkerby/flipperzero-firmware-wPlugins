#include "wand.h"
#include <string.h>

uint32_t wand_pack(const WandMessage* m) {
    uint32_t w = 0;
    w |= (uint32_t)WAND_MAGIC << 24;
    w |= (uint32_t)(m->answer ? 1 : 0) << 23;
    w |= (uint32_t)(m->planet & 7) << 20;
    w |= (uint32_t)(m->is_day ? 1 : 0) << 19;
    w |= (uint32_t)(m->index & 15) << 15;
    w |= (uint32_t)(m->minute & 0x7FF) << 4;
    uint32_t check = 0;
    for(int i = 1; i < 8; i++)
        check += (w >> (i * 4)) & 15;
    w |= check & 15;
    return w;
}

bool wand_unpack(uint32_t w, WandMessage* m) {
    if((w >> 24) != WAND_MAGIC) return false;
    uint32_t check = 0;
    for(int i = 1; i < 8; i++)
        check += (w >> (i * 4)) & 15;
    if((check & 15) != (w & 15)) return false;
    m->answer = (w >> 23) & 1;
    m->planet = (w >> 20) & 7;
    m->is_day = (w >> 19) & 1;
    m->index = (w >> 15) & 15;
    m->minute = (w >> 4) & 0x7FF;
    return m->planet < 7 && m->index >= 1 && m->index <= 12 && m->minute < 1440;
}

static size_t put(WandPulse* p, size_t i, bool level, uint16_t us) {
    p[i].level = level;
    p[i].duration = us;
    return i + 1;
}

size_t wand_encode(const WandMessage* m, WandPulse* pulses) {
    uint32_t w = wand_pack(m);
    size_t i = 0;
    for(int rep = 0; rep < WAND_REPEATS; rep++) {
        i = put(pulses, i, true, 5 * WAND_TE);
        i = put(pulses, i, false, 15 * WAND_TE);
        for(int b = 31; b >= 0; b--) {
            bool one = (w >> b) & 1;
            i = put(pulses, i, true, one ? 3 * WAND_TE : WAND_TE);
            i = put(pulses, i, false, one ? WAND_TE : 3 * WAND_TE);
        }
        i = put(pulses, i, true, WAND_TE);
        i = put(pulses, i, false, 10 * WAND_TE);
    }
    return i;
}

void wand_decoder_reset(WandDecoder* d) {
    memset(d, 0, sizeof(*d));
}

static bool near(uint32_t us, uint32_t want) {
    uint32_t tol = want * 3 / 10 + 60; // thirty percent, and a little slack for short pulses
    return us + tol >= want && us <= want + tol;
}

bool wand_decoder_feed(WandDecoder* d, bool level, uint32_t duration, WandMessage* m) {
    switch(d->state) {
    case 0:
        if(level && near(duration, 5 * WAND_TE)) d->state = 1;
        return false;
    case 1:
        if(!level && near(duration, 15 * WAND_TE)) {
            d->state = 2;
            d->bits = 0;
            d->word = 0;
            d->have_high = false;
        } else {
            d->state = (level && near(duration, 5 * WAND_TE)) ? 1 : 0;
        }
        return false;
    default:
        if(level) {
            if(d->have_high) {
                d->state = 0; // two highs in a row: not ours
                return false;
            }
            d->have_high = true;
            d->high = duration;
            return false;
        }
        if(!d->have_high) {
            d->state = 0;
            return false;
        }
        d->have_high = false;
        bool one = near(d->high, 3 * WAND_TE) && near(duration, WAND_TE);
        bool zero = near(d->high, WAND_TE) && near(duration, 3 * WAND_TE);
        if(!one && !zero) {
            // the tail, or noise: the frame is over either way
            d->state = 0;
            return false;
        }
        d->word = (d->word << 1) | (one ? 1 : 0);
        if(++d->bits == 32) {
            d->state = 0;
            return wand_unpack(d->word, m);
        }
        return false;
    }
}
