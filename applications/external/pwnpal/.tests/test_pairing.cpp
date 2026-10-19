// Host unit tests for the per-client 4-way tracker in pwnpal-marauder/pwnpal_frames.h (Phase B).
// Two verdicts per frame: loose (pwnagotchi/bettercap count) and strict (hcx-crackable ✓).
// Pure logic, no hardware. See tests/run.sh.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../pwnpal-marauder/pwnpal_frames.h"

static int failures = 0;
#define CHECK(cond, name)                \
    do {                                 \
        if(cond) {                       \
            printf("  ok   %s\n", name); \
        } else {                         \
            printf("  FAIL %s\n", name); \
            failures++;                  \
        }                                \
    } while(0)

#define N 32
#define WIN 2000

static void reset(PwnpalHs* t) {
    for(int i = 0; i < N; i++) t[i].used = false;
}
static void cli(uint8_t* c, uint8_t tag) {
    for(int i = 0; i < 6; i++) c[i] = tag;
}
static void rep(uint8_t* r, uint8_t tag) { // 8-byte big-endian replay counter = tag
    memset(r, 0, 8);
    r[7] = tag;
}

// convenience wrappers naming the 4-way message (kdv 2 = WPA2, the PSK-crackable case)
#define KDV2 2
static PwnpalHsVerdict m1(PwnpalHs* t, uint8_t ap, const uint8_t* c, const uint8_t* r, uint32_t ms) {
    return pwnpal_hs_note(t, N, ap, c, 1, false, KDV2, r, ms, WIN);
}
static PwnpalHsVerdict m2(PwnpalHs* t, uint8_t ap, const uint8_t* c, const uint8_t* r, uint32_t ms) {
    return pwnpal_hs_note(t, N, ap, c, 2, false, KDV2, r, ms, WIN);
}
static PwnpalHsVerdict m3(PwnpalHs* t, uint8_t ap, const uint8_t* c, const uint8_t* r, uint32_t ms) {
    return pwnpal_hs_note(t, N, ap, c, 3, false, KDV2, r, ms, WIN);
}
static PwnpalHsVerdict pmkid(PwnpalHs* t, uint8_t ap, const uint8_t* c, const uint8_t* r, uint32_t ms) {
    return pwnpal_hs_note(t, N, ap, c, 1, true, KDV2, r, ms, WIN);
}

int main(void) {
    PwnpalHs t[N];
    uint8_t cA[6], cB[6], r1[8], r2[8], r3[8];
    cli(cA, 0xA1); cli(cB, 0xB2);
    rep(r1, 1); rep(r2, 2); rep(r3, 3);

    printf("pwnpal_rc_dec:\n");
    {
        uint8_t out[8];
        CHECK(pwnpal_rc_dec(r2, out) && memcmp(out, r1, 8) == 0, "dec(2) == 1");
        uint8_t carry_in[8] = {0,0,0,0,0,0,1,0}, carry_out[8];
        uint8_t want[8] = {0,0,0,0,0,0,0,0xFF};
        CHECK(pwnpal_rc_dec(carry_in, carry_out) && memcmp(carry_out, want, 8) == 0, "dec borrows across a byte");
        uint8_t zero[8] = {0}, z_out[8];
        CHECK(!pwnpal_rc_dec(zero, z_out), "dec(0) underflows -> false");
    }

    printf("pwnpal_hs_note (loose = count, strict = crackable):\n");

    // M1 then M2, same replay, in window -> both.
    reset(t);
    CHECK(!m1(t, 5, cA, r1, 1000).loose, "M1 alone -> not loose");
    { PwnpalHsVerdict v = m2(t, 5, cA, r1, 1100);
      CHECK(v.loose && v.strict, "M1+M2 same replay in window -> loose + strict"); }

    // M2 then M1 (out-of-order) still pairs strict.
    reset(t);
    m2(t, 5, cA, r1, 1000);
    CHECK(m1(t, 5, cA, r1, 1100).strict, "M2 then M1 -> strict (order-independent)");

    // M1 + M2 different replay -> loose (we saw both) but NOT crackable.
    reset(t);
    m1(t, 5, cA, r1, 1000);
    { PwnpalHsVerdict v = m2(t, 5, cA, r2, 1100);
      CHECK(v.loose && !v.strict, "M1+M2 mismatched replay -> loose, not strict"); }

    // M1 + M2 same replay but outside the window -> loose, not strict.
    reset(t);
    m1(t, 5, cA, r1, 1000);
    { PwnpalHsVerdict v = m2(t, 5, cA, r1, 1000 + WIN + 1);
      CHECK(v.loose && !v.strict, "M1+M2 outside window -> loose, not strict"); }

    // M2 + M3: M3 replay is M1's + 1, so it normalises onto M2's -> strict.
    reset(t);
    CHECK(!m2(t, 5, cA, r1, 1000).strict, "M2 alone -> not strict");
    { PwnpalHsVerdict v = m3(t, 5, cA, r2, 1100); // M2 rc=1, M3 rc=2 -> norm 1
      CHECK(v.loose && v.strict, "M2+M3 (replay+1) -> loose + strict"); }

    // M2 + M3 with a non-adjacent replay -> loose but not strict.
    reset(t);
    m2(t, 5, cA, r1, 1000);
    { PwnpalHsVerdict v = m3(t, 5, cA, r3, 1100); // M3 rc=3 -> norm 2 != M2 rc=1
      CHECK(v.loose && !v.strict, "M2+M3 non-adjacent replay -> loose, not strict"); }

    // Lone halves never count.
    reset(t);
    CHECK(!m2(t, 5, cA, r1, 1000).loose, "M2 alone -> not loose");
    reset(t);
    { PwnpalHsVerdict v = m1(t, 5, cA, r1, 1000);
      CHECK(!v.loose && !v.strict, "M1 alone -> neither"); }
    reset(t);
    { PwnpalHsVerdict v = m3(t, 5, cA, r2, 1000);
      CHECK(!v.loose && !v.strict, "M3 alone -> neither"); }

    // PMKID is self-contained: loose + strict on the spot.
    reset(t);
    { PwnpalHsVerdict v = pmkid(t, 5, cA, r1, 1000);
      CHECK(v.loose && v.strict, "PMKID -> loose + strict immediately"); }

    // Different clients don't cross-complete: M1 from cA, M2 from cB -> cB has only M2.
    reset(t);
    m1(t, 5, cA, r1, 1000);
    { PwnpalHsVerdict v = m2(t, 5, cB, r1, 1100);
      CHECK(!v.loose && !v.strict, "M1(cA) + M2(cB) -> no cross-client pair"); }

    // Different APs don't cross-complete either.
    reset(t);
    m1(t, 5, cA, r1, 1000);
    { PwnpalHsVerdict v = m2(t, 7, cA, r1, 1100);
      CHECK(!v.loose && !v.strict, "M1(ap5) + M2(ap7) -> no cross-ap pair"); }

    // Same (ap,client) upserts one slot, not two.
    reset(t);
    m1(t, 5, cA, r1, 1000);
    m2(t, 5, cA, r1, 1100);
    { int used = 0; for(int i = 0; i < N; i++) if(t[i].used) used++;
      CHECK(used == 1, "one (ap,client) uses one slot"); }

    // Table full -> evict oldest. Fill N slots with distinct clients (M1 only), then a new client
    // evicts the oldest; that evicted client's M2 must not find its M1.
    reset(t);
    for(int i = 0; i < N; i++) { uint8_t c[6]; cli(c, (uint8_t)(i + 1)); m1(t, 5, c, r1, 1000 + i); }
    uint8_t cNew[6]; cli(cNew, 0xFF);
    m1(t, 5, cNew, r1, 5000); // evicts oldest (ms=1000, client 1)
    uint8_t cOldest[6]; cli(cOldest, 1);
    CHECK(!m2(t, 5, cOldest, r1, 5050).strict, "evicted client -> M2 finds no M1");
    CHECK(m2(t, 5, cNew, r1, 5050).strict, "newest client still pairs");

    // WPA3-SAE etc (kdv 0): counted loose, but never strict-crackable (hashcat can't take it).
    printf("kdv 0 (WPA3-SAE) exclusion from strict:\n");
    reset(t);
    { PwnpalHsVerdict v = pwnpal_hs_note(t, N, 5, cA, 1, true, 0, r1, 1000, WIN);
      CHECK(v.loose && !v.strict, "PMKID kdv0 -> loose, not strict"); }
    reset(t);
    pwnpal_hs_note(t, N, 5, cA, 1, false, 0, r1, 1000, WIN); // M1 kdv0
    { PwnpalHsVerdict v = pwnpal_hs_note(t, N, 5, cA, 2, false, 0, r1, 1100, WIN); // M2 kdv0
      CHECK(v.loose && !v.strict, "M1+M2 both kdv0 -> loose, not strict"); }
    reset(t);
    pwnpal_hs_note(t, N, 5, cA, 1, false, 0, r1, 1000, WIN); // M1 kdv0
    { PwnpalHsVerdict v = pwnpal_hs_note(t, N, 5, cA, 2, false, KDV2, r1, 1100, WIN); // M2 kdv2
      CHECK(!v.strict, "pair with one kdv0 half -> not strict"); }
    reset(t);
    pwnpal_hs_note(t, N, 5, cA, 2, false, KDV2, r1, 1000, WIN); // M2 kdv2
    { PwnpalHsVerdict v = pwnpal_hs_note(t, N, 5, cA, 3, false, 0, r2, 1100, WIN); // M3 kdv0
      CHECK(!v.strict, "M2+M3 with kdv0 M3 -> not strict"); }
    reset(t);
    { PwnpalHsVerdict v = pwnpal_hs_note(t, N, 5, cA, 1, true, KDV2, r1, 1000, WIN);
      CHECK(v.strict, "PMKID kdv2 -> strict (sanity)"); }

    printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
