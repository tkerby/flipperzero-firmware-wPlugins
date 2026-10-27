// Host helper: the most forgiving ace on a hole at the app's aim resolution,
// printed as key presses and an OK hold time.
//   cc -O2 -o /tmp/ace tools/ace.c course.c sim.c -lm && /tmp/ace 1
#include "../sim.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int sinks(const Hole* h, float a, float p) {
    Sim s;
    sim_reset(&s, h);
    sim_shoot(&s, a, p);
    int wait = 0;
    while(s.rolling && wait < 1000) {
        sim_step(&s);
        wait++;
    }
    return s.sunk;
}

int main(int argc, char** argv) {
    int hole = argc > 1 ? atoi(argv[1]) - 1 : 0;
    const Hole* h = &holes[hole];
    float fine = 1.5f * 3.14159265f / 180.0f;
    int best_k = 0, best_lo = 0, best_len = 0;
    Sim s0;
    sim_reset(&s0, h);
    float base = sim_aim_to_cup(&s0);
    for(int k = -90; k <= 90; k++) {
        int run = 0, lo = 0;
        for(int p = 0; p <= 101; p++) {
            if(p <= 100 && sinks(h, base + k * fine, p)) {
                if(run == 0) lo = p;
                run++;
            } else {
                if(run > best_len) {
                    best_len = run;
                    best_lo = lo;
                    best_k = k;
                }
                run = 0;
            }
        }
    }
    if(!best_len) {
        printf("hole %d: no ace\n", hole + 1);
        return 1;
    }
    int p = best_lo + best_len / 2;
    printf(
        "hole %d: %d presses %s, power %d (window %d..%d), hold %.2fs rising or %.2fs falling\n",
        hole + 1,
        abs(best_k),
        best_k < 0 ? "LEFT" : "RIGHT",
        p,
        best_lo,
        best_lo + best_len - 1,
        p / 100.0f * 0.7f,
        1.4f - p / 100.0f * 0.7f);
    return 0;
}
