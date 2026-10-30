// Round trip: encode a frame, play it back with timing jitter and noise, decode it.
#include "../wand.h"
#include <stdio.h>
#include <stdlib.h>
static uint32_t seed = 12345;
static uint32_t rnd(uint32_t n) {
    seed = seed * 1103515245u + 12345u;
    return (seed >> 8) % n;
}
int main(void) {
    int fails = 0, cases = 0;
    static WandPulse pulses[WAND_PULSES];
    for(int planet = 0; planet < 7; planet++) {
        for(int t = 0; t < 40; t++) {
            WandMessage m = {
                .answer = t & 1,
                .planet = planet,
                .is_day = t & 2,
                .index = 1 + t % 12,
                .minute = (t * 97 + planet * 311) % 1440};
            size_t n = wand_encode(&m, pulses);
            WandDecoder d;
            wand_decoder_reset(&d);
            WandMessage got;
            int decoded = 0;
            // some noise first
            for(int k = 0; k < 20; k++)
                wand_decoder_feed(&d, k & 1, 100 + rnd(3000), &got);
            for(size_t i = 0; i < n; i++) {
                // up to +-20% jitter on every pulse
                int32_t us = pulses[i].duration;
                us += (int32_t)(us * ((int)rnd(41) - 20)) / 100;
                if(wand_decoder_feed(&d, pulses[i].level, (uint32_t)us, &got)) decoded++;
            }
            cases++;
            int ok = decoded >= 1 && got.answer == m.answer && got.planet == m.planet &&
                     got.is_day == m.is_day && got.index == m.index && got.minute == m.minute;
            if(!ok) {
                fails++;
                printf("FAIL planet %d t %d decoded %d\n", planet, t, decoded);
            }
        }
    }
    // a corrupted word must not pass
    WandMessage m = {0, 3, 1, 5, 720};
    uint32_t w = wand_pack(&m) ^ (1u << 17);
    WandMessage x;
    int rejected = !wand_unpack(w, &x);
    printf(
        "%d frames round-tripped with jitter and noise, %d failed; flipped bit %s\n",
        cases,
        fails,
        rejected ? "rejected" : "ACCEPTED");
    printf(
        "frame: %zu pulses, %u us\n",
        wand_encode(&m, pulses),
        (unsigned)(2000 + 6000 + 32 * 1600 + 400 + 4000) * WAND_REPEATS);
    return (fails || !rejected) ? 1 : 0;
}
