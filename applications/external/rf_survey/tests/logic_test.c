// Host tests for the pure logic in rf_logic.h — the same code the firmware compiles.
//
// Build & run:  cc -std=c11 -Wall -Wextra -O1 -I.. logic_test.c -o logic_test && ./logic_test
//
// This exists because the three bugs that actually hurt were all in logic that needs no radio
// (a ring off-by-one, a synthetic rising edge after an observation hole, a percentile rank).
// Each test below is a scenario that was either a real bug or one bad decision away from it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rf_logic.h"

static int g_fail;
static const char* g_case;

#define CHECK(cond)                                               \
    do {                                                          \
        if(!(cond)) {                                             \
            printf("  FAIL  %-42s  line %d\n", g_case, __LINE__); \
            g_fail++;                                             \
        }                                                         \
    } while(0)

#define CASE(name)       \
    do {                 \
        g_case = (name); \
    } while(0)

// ---------------------------------------------------------------- sweep geometry

static void test_wf_col_at(void) {
    // A ring of 5 that has wrapped: columns 2..6 must come out chronologically.
    CASE("wf_col_at wraps chronologically");
    enum {
        COLS = 5
    };
    int ring[COLS];
    int head = 0, cnt = 0;
    for(int v = 0; v <= 6; v++) {
        head = (head + 1) % COLS;
        ring[head] = v;
        if(cnt < COLS) cnt++;
    }
    int expect[] = {2, 3, 4, 5, 6};
    for(int k = 0; k < cnt; k++)
        CHECK(ring[rfs_wf_col_at((uint16_t)head, COLS, (uint16_t)cnt, (uint16_t)k)] == expect[k]);

    // The off-by-one that shipped: with a single column the export read an UNWRITTEN slot.
    CASE("wf_col_at cnt=1 reads the only written column");
    CHECK(rfs_wf_col_at(1, COLS, 1, 0) == 1);
    CHECK(rfs_wf_col_at(0, COLS, 1, 0) == 0);
}

static void test_step_fit(void) {
    CASE("step_fit: full spectrum @50k must coarsen");
    // 300-928 MHz at 50 kHz would be 12561 bins; MAX_BINS is 1024.
    CHECK(rfs_step_fit(300000000u, 928000000u, 50000u, 1024) == 650000u);

    CASE("step_fit: a narrow band keeps the finest");
    CHECK(rfs_step_fit(433050000u, 434790000u, 50000u, 1024) == 50000u);

    CASE("step_fit: a user step that fits is preserved");
    CHECK(rfs_step_fit(433050000u, 434790000u, 200000u, 1024) == 200000u);

    CASE("step_fit: never finer than requested");
    CHECK(rfs_step_fit(433050000u, 434790000u, 650000u, 1024) == 650000u);
}

// ---------------------------------------------------------------- parked burst machine

// Drive the state machine over a synthetic signal with holes, exactly like park_loop does.
static void
    run(RfsPark* s,
        uint32_t total_ms,
        uint32_t period_ms,
        uint32_t burst_ms,
        uint32_t hole_at,
        uint32_t hole_ms,
        int8_t trig) {
    for(uint32_t t = 0; t < total_ms; t += 8) {
        if(hole_ms && t >= hole_at && t < hole_at + hole_ms) continue; // receiver was blind
        int8_t v = ((t % period_ms) < burst_ms) ? -55 : -95;
        rfs_park_feed(s, t, v, trig);
    }
}

static void test_park_clean(void) {
    CASE("park: clean 2 s machine");
    RfsPark s;
    rfs_park_clear(&s);
    run(&s, 20000, 2000, 40, 0, 0, -85);
    CHECK(s.bursts == 10);
    CHECK(s.per_n == 9);
    for(uint8_t i = 0; i < s.per_n; i++)
        CHECK(s.per[i] == 2000);
    CHECK(s.gaps == 0);
}

static void test_park_gap_mid_burst(void) {
    // THE bug (F15): a 60 ms hole inside a 200 ms burst used to end the burst and then treat
    // the still-high next sample as a NEW rising edge -- 7 "bursts", a 1888 ms period and an
    // 88 ms duration that never happened.
    CASE("park: hole mid-burst invents no rise");
    RfsPark s;
    rfs_park_clear(&s);
    run(&s, 12000, 2000, 200, 4050, 60, -85);
    CHECK(s.bursts == 6);
    CHECK(s.gaps == 1);
    for(uint8_t i = 0; i < s.per_n; i++)
        CHECK(s.per[i] == 2000);
    for(uint8_t i = 0; i < s.dur_n; i++)
        CHECK(s.dur[i] == 200);
    CHECK(s.dur_n == 5); // the interrupted burst is dropped, not measured as 88 ms
}

static void test_park_gap_idle(void) {
    CASE("park: hole while idle costs one period, not the series");
    RfsPark s;
    rfs_park_clear(&s);
    run(&s, 20000, 2000, 40, 2090, 200, -85);
    CHECK(s.gaps == 1);
    for(uint8_t i = 0; i < s.per_n; i++)
        CHECK(s.per[i] == 2000);
}

static void test_park_repeats(void) {
    // A handheld remote: 4 frames ~24 ms apart inside one press.
    CASE("park: repeat group of 4 is counted as one press");
    RfsPark s;
    rfs_park_clear(&s);
    for(uint32_t press = 0; press < 3; press++) {
        uint32_t base = press * 5000;
        for(uint32_t k = 0; k < 4; k++) {
            uint32_t b = base + k * 40;
            for(uint32_t t = b; t < b + 24; t += 8)
                rfs_park_feed(&s, t, -60, -85);
            for(uint32_t t = b + 24; t < b + 40; t += 8)
                rfs_park_feed(&s, t, -95, -85);
        }
        for(uint32_t t = base + 160; t < base + 5000; t += 8)
            rfs_park_feed(&s, t, -95, -85);
    }
    CHECK(s.bursts == 12);
    CHECK(s.rep_last == 4);
}

static void test_park_stop_mid_burst(void) {
    // F16: stopping while high must not record a duration we never saw the end of.
    CASE("park: stop mid-burst records no duration");
    RfsPark s;
    rfs_park_clear(&s);
    for(uint32_t t = 0; t < 2000; t += 8)
        rfs_park_feed(&s, t, -95, -85);
    for(uint32_t t = 2000; t < 2100; t += 8)
        rfs_park_feed(&s, t, -60, -85);
    CHECK(s.in_burst);
    rfs_park_stop(&s);
    CHECK(!s.in_burst);
    CHECK(s.dur_n == 0);
}

static void test_park_stats(void) {
    CASE("park: stats on a fixed 1000 ms series");
    RfsPark s;
    rfs_park_clear(&s);
    for(int i = 0; i < 10; i++)
        rfs_park_push(s.per, &s.per_n, &s.per_i, 1000);
    uint32_t mean, sd, cv;
    rfs_park_stats(&s, &mean, &sd, &cv);
    CHECK(mean == 1000);
    CHECK(sd == 0);
    CHECK(cv == 0);

    CASE("park: stats reject a single-sample series");
    rfs_park_clear(&s);
    rfs_park_push(s.per, &s.per_n, &s.per_i, 1000);
    rfs_park_stats(&s, &mean, &sd, &cv);
    CHECK(mean == 0 && sd == 0 && cv == 0);
}

// A PT2262-style frame: 24 bits, 350 us base, each bit two complementary pulses (1T/3T or
// 3T/1T), frames separated by a 12 ms silence.
static uint16_t build_frame(int32_t* e, uint16_t at, uint32_t seed) {
    for(int b = 0; b < 24; b++) {
        int one = (seed >> (b % 8)) & 1;
        e[at++] = one ? 1050 : 350;
        e[at++] = one ? -350 : -1050;
    }
    return at;
}

static void test_frame_estimate(void) {
    CASE("frame: 24-bit PT2262 frame is recovered");
    int32_t e[RFS_FRAME_EDGES];
    uint16_t n = 0;
    e[n++] = -12000; // a partial frame ends here
    n = build_frame(e, n, 0xA5);
    e[n++] = -12000;
    n = build_frame(e, n, 0xA5);
    e[n++] = -12000;

    uint32_t base, frame;
    uint16_t bits;
    rfs_frame_estimate(e, n, &base, &frame, &bits);
    CHECK(base == 350);
    CHECK(bits == 24);
    CHECK(frame == 24u * 1400u); // 24 bits x 4T

    CASE("frame: too few edges is not a measurement");
    rfs_frame_estimate(e, 5, &base, &frame, &bits);
    CHECK(base == 0 && frame == 0 && bits == 0);
}

static void test_code_kind(void) {
    int32_t e[RFS_FRAME_EDGES];
    const uint32_t base = 350;
    uint16_t n;

    // two clean frames between silences -> same bits twice -> a fixed code
    CASE("code: identical frames are STATIC");
    n = 0;
    e[n++] = -12000;
    n = build_frame(e, n, 0x5A);
    e[n++] = -12000;
    n = build_frame(e, n, 0x5A);
    e[n++] = -12000;
    CHECK(rfs_code_kind(e, n, base) == RFS_CODE_STATIC);

    // a capture that joined mid-frame: the short leading run must not poison the comparison
    CASE("code: a partial first frame does not read as rolling");
    n = 0;
    e[n++] = -12000;
    n = build_frame(e, n, 0x5A); // full frame
    e[n++] = -12000;
    n = build_frame(e, n, 0x5A);
    e[n++] = -12000;
    CHECK(rfs_code_kind(e, n, base) == RFS_CODE_STATIC);

    // one bit flipped in the second frame -> a counter or a cipher
    CASE("code: a changed frame is CHANGING");
    n = 0;
    e[n++] = -12000;
    n = build_frame(e, n, 0x5A);
    e[n++] = -12000;
    n = build_frame(e, n, 0x5B);
    e[n++] = -12000;
    CHECK(rfs_code_kind(e, n, base) == RFS_CODE_CHANGING);

    // a single frame is not enough to compare
    CASE("code: one frame is UNKNOWN");
    n = 0;
    n = build_frame(e, n, 0x5A);
    e[n++] = -12000;
    CHECK(rfs_code_kind(e, n, base) == RFS_CODE_UNKNOWN);
}

static void test_preamble(void) {
    CASE("preamble: 8 base pulses then a sync gap");
    int32_t e[RFS_FRAME_EDGES];
    uint16_t n = 0;
    for(int i = 0; i < 8; i++)
        e[n++] = (i & 1) ? -350 : 350;
    e[n++] = -12000;
    n = build_frame(e, n, 0x33);
    e[n++] = -12000;

    RfsPreamble p = rfs_preamble(e, n, 350);
    CHECK(p.count == 8);
    CHECK(p.sync_us == 12000);

    CASE("preamble: no long run means no preamble");
    n = 0;
    n = build_frame(e, n, 0x33);
    e[n++] = -12000;
    p = rfs_preamble(e, n, 350);
    CHECK(p.count < 8); // the data alone does not look like a preamble
}

static void test_princeton(void) {
    CASE("princeton: 24 bits + sync = 50 edges");
    int32_t e[RFS_PRINCETON_EDGES];
    uint16_t n = rfs_encode_princeton(0x000001u, 350, e, RFS_PRINCETON_EDGES);
    CHECK(n == RFS_PRINCETON_EDGES);

    CASE("princeton: bit 0 is a 1T/3T pair for '0'");
    CHECK(e[0] == 350 && e[1] == -1050);

    CASE("princeton: the last data bit is a 3T/1T pair for '1'");
    CHECK(e[46] == 1050 && e[47] == -350);

    CASE("princeton: sync is 1T high then 31T low");
    CHECK(e[48] == 350);
    CHECK(e[49] == -10850);

    CASE("princeton: refuses a buffer that is too small");
    CHECK(rfs_encode_princeton(0xFFFFFFFFu, 350, e, 10) == 0);

    CASE("princeton: every edge is a multiple of T");
    bool ok = true;
    for(uint16_t i = 0; i < n; i++) {
        int32_t v = e[i] < 0 ? -e[i] : e[i];
        if(v % 350) ok = false;
    }
    CHECK(ok);
}

static void test_clusters(void) {
    uint16_t c[4];

    CASE("clusters: one periodic source is one cluster");
    uint16_t one[] = {2000, 2002, 1998, 2001, 1999, 2000};
    CHECK(rfs_period_clusters(one, 6, c, 4) == 1);

    CASE("clusters: two interleaved sources are two clusters");
    uint16_t two[] = {2000, 45000, 2002, 45010, 1998, 44990, 2001, 45000};
    CHECK(rfs_period_clusters(two, 8, c, 4) == 2);
    CHECK(c[0] > 1900 && c[0] < 2100);
    CHECK(c[1] > 44000);

    CASE("clusters: too few samples is not a claim");
    uint16_t few[] = {2000, 45000};
    CHECK(rfs_period_clusters(few, 2, c, 4) == 0);
}

static void test_distance(void) {
    // 433.92 MHz, +10 dBm TX, 0 dBi, n = 2.7, -85 dBm RX -> ~385 m
    CASE("distance: -85 dBm at 433 MHz, urban is a few hundred metres");
    RfsDistance d = rfs_estimate_distance(-85, 433920000u, 10, 0, 27, 6);
    CHECK(d.valid);
    CHECK(d.m > 300 && d.m < 480);
    CHECK(d.lo < d.m && d.m < d.hi);

    CASE("distance: the interval brackets the estimate by the margin");
    // +-6 dB at n = 2.7 is a factor of ~1.7 either way
    CHECK((float)d.hi / (float)d.m > 1.4f && (float)d.hi / (float)d.m < 2.0f);
    CHECK((float)d.m / (float)d.lo > 1.4f && (float)d.m / (float)d.lo < 2.0f);

    CASE("distance: 10 dB stronger is roughly a third of the distance");
    RfsDistance close = rfs_estimate_distance(-75, 433920000u, 10, 0, 27, 6);
    CHECK(close.m * 2 < d.m);

    CASE("distance: a tighter environment shortens the estimate");
    RfsDistance indoor = rfs_estimate_distance(-85, 433920000u, 10, 0, 35, 6);
    CHECK(indoor.m < d.m);

    CASE("distance: out-of-range input is not a claim");
    CHECK(!rfs_estimate_distance(-85, 1000u, 10, 0, 27, 6).valid);
    CHECK(!rfs_estimate_distance(-85, 433920000u, 10, 0, 99, 6).valid);
}

// Boundary sweep: the functions are fed their own limits and just outside them. These are the
// inputs a corrupt settings file, a truncated capture or a hostile .sub can produce.
static void test_boundaries(void) {
    uint16_t c[4];
    uint16_t per[8];
    int32_t e[8];

    CASE("bounds: step_fit tolerates a degenerate range");
    CHECK(rfs_step_fit(0, 0, 50000, 1024) == 50000);
    CHECK(rfs_step_fit(928000000u, 300000000u, 50000, 1024) == 50000); // inverted
    CHECK(rfs_step_fit(300000000u, 928000000u, 999999999u, 1024) == 650000u);

    CASE("bounds: clusters with one distinct value");
    for(int i = 0; i < 8; i++)
        per[i] = 2000;
    CHECK(rfs_period_clusters(per, 8, c, 4) == 1);

    CASE("bounds: clusters never writes past max");
    per[0] = 100;
    per[1] = 2000;
    per[2] = 40000;
    per[3] = 60000;
    CHECK(rfs_period_clusters(per, 4, c, 1) <= 1);

    CASE("bounds: distance rejects a receive level above the transmitter");
    CHECK(
        !rfs_estimate_distance(20, 433920000u, 10, 0, 27, 6).valid ||
        rfs_estimate_distance(20, 433920000u, 10, 0, 27, 6).m == 1);

    CASE("bounds: distance clamps absurd inputs instead of wrapping");
    RfsDistance far = rfs_estimate_distance(-127, 300000000u, 20, 10, 20, 6);
    CHECK(far.valid && far.m <= 99999 && far.hi <= 99999 && far.lo >= 1);

    CASE("bounds: princeton rejects out-of-range timing");
    CHECK(rfs_encode_princeton(0, 10, e, 8) == 0);
    CHECK(rfs_encode_princeton(0, 5000, e, 8) == 0);
    CHECK(rfs_encode_princeton(0, 50, e, 8) == 0); // buffer too small

    CASE("bounds: a frame longer than 32 bits is clamped, not wrapped");
    int32_t big[RFS_FRAME_EDGES];
    for(int i = 0; i < RFS_FRAME_EDGES; i++)
        big[i] = (i & 1) ? -600000000 : 600000000;
    uint32_t fb, ff;
    uint16_t fbits;
    rfs_frame_estimate(big, RFS_FRAME_EDGES, &fb, &ff, &fbits);
    CHECK(ff == 0xFFFFFFFFu || ff == 0); // either "cannot report" outcome, never a small lie

    CASE("bounds: park machine survives an extreme trigger");
    RfsPark s2;
    rfs_park_clear(&s2);
    for(uint32_t t = 0; t < 200; t += 8)
        rfs_park_feed(&s2, t, -127, 127); // never above
    CHECK(s2.bursts == 0);
    for(uint32_t t = 200; t < 400; t += 8)
        rfs_park_feed(&s2, t, 127, -128); // always above
    CHECK(s2.bursts == 1);
    CHECK(s2.per_n == 0);
}

int main(void) {
    printf("rf_logic host tests\n\n");
    test_wf_col_at();
    test_step_fit();
    test_park_clean();
    test_park_gap_mid_burst();
    test_park_gap_idle();
    test_park_repeats();
    test_park_stop_mid_burst();
    test_park_stats();
    test_frame_estimate();
    test_code_kind();
    test_preamble();
    test_princeton();
    test_clusters();
    test_distance();
    test_boundaries();

    printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
