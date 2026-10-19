// Host unit tests for the pure geo/identity helpers in pwnpal/src/pwnpal_geo.h.
// No hardware needed: compile with g++ and run (see tests/run.sh).
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "../pwnpal/src/pwnpal_geo.h"

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

static bool feq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

int main(void) {
    // --- parse_deg ---
    CHECK(feq(parse_deg("50.0784950"), 50.078495f, 1e-4f), "parse_deg positive");
    CHECK(feq(parse_deg("-14.42"), -14.42f, 1e-4f), "parse_deg negative");
    CHECK(feq(parse_deg("+3.5"), 3.5f, 1e-4f), "parse_deg leading +");
    CHECK(parse_deg("") >= 1e8f, "parse_deg empty -> sentinel");
    CHECK(parse_deg(NULL) >= 1e8f, "parse_deg NULL -> sentinel");

    // --- coord_ok ---
    CHECK(coord_ok("50.08", "14.45"), "coord_ok valid");
    CHECK(!coord_ok("91.0", "0.0"), "coord_ok lat out of range");
    CHECK(!coord_ok("0.0", "181.0"), "coord_ok lon out of range");
    CHECK(!coord_ok("0.0", "0.0"), "coord_ok rejects null island");
    CHECK(!coord_ok("0.1", "-0.2"), "coord_ok rejects near null island");
    CHECK(coord_ok("50.08", "0.0"), "coord_ok keeps real lat with lon 0 (outlier guard's job)");

    // --- geo_km ---
    CHECK(feq(geo_km(50.0f, 14.0f, 50.0f, 14.0f), 0.0f, 0.01f), "geo_km same point ~0");
    CHECK(feq(geo_km(50.0f, 14.0f, 51.0f, 14.0f), 111.0f, 1.0f), "geo_km 1deg lat ~111km");
    CHECK(geo_km(50.08f, 14.45f, 50.08f, 1.0f) > 900.0f, "geo_km far-lon outlier ~1000km");

    // --- identity_is_64hex ---
    char id64[65];
    for(int i = 0; i < 64; i++) id64[i] = 'a';
    id64[64] = '\0';
    CHECK(identity_is_64hex(id64), "identity 64 lowercase hex ok");
    CHECK(
        identity_is_64hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
        "identity varied lowercase hex ok");
    CHECK(
        identity_is_64hex("0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"),
        "identity uppercase hex ok");
    CHECK(!identity_is_64hex("a63fa4wnd_run"), "identity non-hex rejected");
    CHECK(!identity_is_64hex("abcd"), "identity too short rejected");
    CHECK(!identity_is_64hex(""), "identity empty rejected");
    char id65[66];
    for(int i = 0; i < 65; i++) id65[i] = 'a';
    id65[65] = '\0';
    CHECK(!identity_is_64hex(id65), "identity 65 chars rejected");

    // --- loc_accumulate / loc_estimate (weighted-centroid) ---
    {
        uint16_t n = 0;
        float ws = 0, wla = 0, wlo = 0, olat = 0, olon = 0;
        loc_accumulate(&n, &ws, &wla, &wlo, 50.0f, 14.0f, -60); // equal weight (w=40)
        loc_accumulate(&n, &ws, &wla, &wlo, 52.0f, 16.0f, -60);
        bool ok = loc_estimate(true, n, ws, wla, wlo, 1e9f, 1e9f, &olat, &olon);
        CHECK(ok && feq(olat, 51.0f, 1e-3f) && feq(olon, 15.0f, 1e-3f),
              "wcl equal-weight centroid = midpoint");
    }
    {
        uint16_t n = 0;
        float ws = 0, wla = 0, wlo = 0, olat = 0, olon = 0;
        loc_accumulate(&n, &ws, &wla, &wlo, 50.0f, 14.0f, -30); // strong (w=70)
        loc_accumulate(&n, &ws, &wla, &wlo, 60.0f, 24.0f, -95); // weak (w=5)
        loc_estimate(true, n, ws, wla, wlo, 1e9f, 1e9f, &olat, &olon);
        CHECK(olat < 51.0f && olon < 15.0f, "wcl leans toward the stronger sample");
    }
    {
        uint16_t n = 0;
        float ws = 0, wla = 0, wlo = 0;
        loc_accumulate(&n, &ws, &wla, &wlo, 50.0f, 14.0f, 0); // rssi 0 = unknown -> skipped
        CHECK(n == 0 && ws == 0.0f, "loc_accumulate skips rssi==0");
    }
    {
        float olat = 0, olon = 0;
        bool ok = loc_estimate(true, 1, 40.0f, 40.0f * 50.0f, 40.0f * 14.0f, 48.0f, 12.0f, &olat,
                               &olon);
        CHECK(ok && feq(olat, 48.0f, 1e-3f), "loc_estimate <2 samples -> strongest fix");
    }
    {
        float olat = 0, olon = 0;
        bool ok = loc_estimate(false, 5, 200.0f, 200.0f * 50.0f, 200.0f * 14.0f, 48.0f, 12.0f,
                               &olat, &olon);
        CHECK(ok && feq(olat, 48.0f, 1e-3f), "loc_estimate tri-off -> strongest fix");
    }
    {
        float olat = 0, olon = 0;
        bool ok = loc_estimate(true, 0, 0, 0, 0, 1e9f, 1e9f, &olat, &olon);
        CHECK(!ok, "loc_estimate no data -> false");
    }

    if(failures) {
        printf("FAILED %d\n", failures);
        return 1;
    }
    printf("all geo tests passed\n");
    return 0;
}
