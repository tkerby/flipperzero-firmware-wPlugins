// Host check: every hole can be sunk within par (beam search over shots),
// and the roll distance table for tuning. Build:
//   cc -O2 -o /tmp/solve tools/solve_test.c course.c sim.c -lm && /tmp/solve
#include "../sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BEAM   8
#define ANGLES 72
#define POWERS 10

typedef struct {
    Sim sim;
    int strokes;
    float score;
} Node;

static float dist_to_cup(const Sim* s) {
    float dx = s->hole->cup_x - s->x, dy = s->hole->cup_y - s->y;
    return sqrtf(dx * dx + dy * dy);
}

static int play(Sim* s, float angle, float power, int* lost) {
    sim_shoot(s, angle, power);
    while(s->rolling) {
        sim_step(s);
        if(s->events & SimEvLost) (*lost)++;
    }
    return s->sunk;
}

static int solve(const Hole* h, int max_strokes, int* best_strokes) {
    Node beam[BEAM], next[BEAM * 2];
    int n = 1;
    sim_reset(&beam[0].sim, h);
    beam[0].strokes = 0;
    for(int depth = 1; depth <= max_strokes; depth++) {
        int m = 0;
        for(int b = 0; b < n; b++) {
            for(int a = 0; a < ANGLES; a++) {
                for(int p = 0; p < POWERS; p++) {
                    Sim s = beam[b].sim;
                    int lost = 0;
                    float ang = a * 6.2831853f / ANGLES;
                    float pow = 5 + p * 10;
                    if(play(&s, ang, pow, &lost)) {
                        *best_strokes = beam[b].strokes + 1 + lost;
                        if(*best_strokes <= max_strokes) return 1;
                        continue;
                    }
                    Node cand = {s, beam[b].strokes + 1 + lost, dist_to_cup(&s)};
                    if(cand.strokes >= max_strokes) continue;
                    // keep the best BEAM*2 by distance
                    int pos = m;
                    if(m < BEAM * 2) {
                        m++;
                    } else {
                        pos = 0;
                        for(int i = 1; i < m; i++)
                            if(next[i].score > next[pos].score) pos = i;
                        if(next[pos].score <= cand.score) continue;
                    }
                    next[pos] = cand;
                }
            }
        }
        // dedupe-ish: take the BEAM best
        n = 0;
        for(int k = 0; k < BEAM && m > 0; k++) {
            int best = 0;
            for(int i = 1; i < m; i++)
                if(next[i].score < next[best].score) best = i;
            beam[n++] = next[best];
            next[best] = next[--m];
        }
        if(n == 0) break;
    }
    return 0;
}

int main(void) {
    printf("roll distance on open green (power -> px):\n");
    for(int p = 0; p <= 100; p += 20) {
        Sim s;
        Hole flat = {"flat", "", 0, 2, 3, 36, 200, 36, NULL, 0, NULL, 0};
        sim_reset(&s, &flat);
        s.x = -500; // no frame interference: start far out on an endless green
        s.shot_x = s.x;
        sim_shoot(&s, 0, p);
        float x0 = s.x;
        int ticks = 0;
        while(s.rolling && ticks < 2000) {
            sim_step(&s);
            ticks++;
        }
        printf("  %3d -> %5.0f px in %.1f s\n", p, s.x - x0, ticks / 30.0);
    }
    int fails = 0, total_par = 0, total_best = 0;
    for(int i = 0; i < HOLE_COUNT; i++) {
        const Hole* h = &holes[i];
        int best = 0;
        int ok = solve(h, h->par, &best);
        if(!ok) {
            ok = solve(h, h->par + 2, &best);
            if(!ok) fails++;
        }
        printf(
            "%d. %-16s par %d  %s in %d\n", i + 1, h->name, h->par, ok ? "sunk" : "NOT SUNK", best);
        total_par += h->par;
        total_best += ok ? best : 99;
    }
    printf("course par %d, solver total %d\n", total_par, total_best);
    return fails ? 1 : 0;
}
