// Host unit tests for the pure 802.11 frame parsers in pwnpal-marauder/pwnpal_frames.h.
// No hardware / Arduino needed: compile with g++ and run (see tests/run.sh).
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../pwnpal-marauder/pwnpal_frames.h"

static int failures = 0;
#define CHECK(cond, name)                       \
    do {                                        \
        if(cond) {                              \
            printf("  ok   %s\n", name);        \
        } else {                                \
            printf("  FAIL %s\n", name);        \
            failures++;                         \
        }                                       \
    } while(0)

// Wrap tagged-param bytes as a beacon: 36-byte header/fixed-params prefix, then the IEs.
static int beacon(uint8_t* buf, const uint8_t* ies, int ielen) {
    memset(buf, 0, 36);
    buf[0] = 0x80; // beacon subtype
    memcpy(buf + 36, ies, ielen);
    return 36 + ielen;
}

// Wrap tagged-param bytes as an assoc-req (28-byte hdr+fixed) or reassoc-req (34-byte, subtype 0x20).
static int assocreq(uint8_t* buf, const uint8_t* ies, int ielen, bool reassoc) {
    int hdr = reassoc ? 34 : 28;
    memset(buf, 0, hdr);
    buf[0] = reassoc ? 0x20 : 0x00;
    memcpy(buf + hdr, ies, ielen);
    return hdr + ielen;
}

// An RSN IE (id 48, len 20) with the given capabilities low/high bytes.
// version, group=CCMP, 1 pairwise=CCMP, 1 AKM=PSK, caps.
#define RSN_IE(caplo, caphi)                                                          \
    48, 20, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 2,   \
        (caplo), (caphi)

int main(void) {
    uint8_t f[256];
    int n;

    printf("pwnpal_rsn_requires_pmf:\n");

    {
        uint8_t ie[] = {RSN_IE(0x40, 0x00)}; // MFPR (bit6) set = PMF required
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == true, "MFPR set -> required");
    }
    {
        uint8_t ie[] = {RSN_IE(0x00, 0x00)}; // no PMF bits
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == false, "no PMF caps -> not required");
    }
    {
        uint8_t ie[] = {RSN_IE(0x80, 0x00)}; // MFPC only (capable, not required)
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == false, "MFPC only -> not required");
    }
    {
        uint8_t ie[] = {RSN_IE(0xC0, 0x00)}; // MFPC + MFPR
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == true, "MFPC+MFPR -> required");
    }
    {
        uint8_t ie[] = {0, 4, 't', 'e', 's', 't'}; // SSID IE only, no RSN
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == false, "no RSN IE -> not required");
    }
    {
        // SSID + Supported Rates before the RSN IE: the walker must reach it.
        uint8_t ie[] = {0, 3, 'a', 'b', 'c', 1, 4, 0x82, 0x84, 0x8b, 0x96, RSN_IE(0x40, 0x00)};
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == true, "RSN after other IEs -> required");
    }
    {
        // RSN IE claims len 20 but the frame is truncated: must not read past the end.
        uint8_t ie[] = {48, 20, 1, 0, 0, 0x0f}; // only 6 bytes present
        n = beacon(f, ie, sizeof(ie));
        CHECK(pwnpal_rsn_requires_pmf(f, n) == false, "truncated RSN -> false (no overread)");
    }
    {
        // Too short to hold any tagged params.
        memset(f, 0, sizeof(f));
        f[0] = 0x80;
        CHECK(pwnpal_rsn_requires_pmf(f, 20) == false, "short frame -> false");
    }

    printf("pwnpal_assoc_pmkid:\n");
    // RSN IE (len 38) with 1 PMKID: version, group, 1 pairwise, 1 AKM, caps, pmkid-count=1, PMKID.
#define RSN_PMKID_HEAD 48, 38, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 2, 0, 0, 1, 0
    {
        uint8_t ie[] = {RSN_PMKID_HEAD, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
                        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x01};
        n = assocreq(f, ie, sizeof(ie), false);
        uint8_t pm[16];
        CHECK(pwnpal_assoc_pmkid(f, n, pm) && pm[0] == 0xaa && pm[15] == 0x01,
              "assoc-req PMKID extracted");
    }
    {
        // reassoc-req (ie@34) with SSID + rates before the RSN IE
        uint8_t ie[] = {0, 3, 'a', 'b', 'c', 1, 4, 0x82, 0x84, 0x8b, 0x96, RSN_PMKID_HEAD,
                        0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        n = assocreq(f, ie, sizeof(ie), true);
        uint8_t pm[16];
        CHECK(pwnpal_assoc_pmkid(f, n, pm) && pm[0] == 0x07, "reassoc-req PMKID after other IEs");
    }
    {
        uint8_t ie[] = {RSN_PMKID_HEAD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        n = assocreq(f, ie, sizeof(ie), false);
        uint8_t pm[16];
        CHECK(!pwnpal_assoc_pmkid(f, n, pm), "all-zero PMKID -> false");
    }
    {
        // RSN IE with PMKID-count claiming 1 but no room for it -> no overread
        uint8_t ie[] = {48, 22, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f, 0xac, 4, 1, 0, 0, 0x0f,
                        0xac, 2, 0, 0, 1, 0};
        n = assocreq(f, ie, sizeof(ie), false);
        uint8_t pm[16];
        CHECK(!pwnpal_assoc_pmkid(f, n, pm), "PMKID count 1 but no room -> false");
    }
    {
        uint8_t ie[] = {RSN_IE(0, 0)}; // beacon, wrong subtype
        n = beacon(f, ie, sizeof(ie));
        uint8_t pm[16];
        CHECK(!pwnpal_assoc_pmkid(f, n, pm), "beacon subtype -> false");
    }

    if(failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nall tests passed\n");
    return 0;
}
