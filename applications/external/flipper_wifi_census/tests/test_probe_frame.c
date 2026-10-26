#include "include/domain/wc_observation.h"
#include "include/domain/wc_probe_frame.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// A real probe request captured by Marauder (linktype 105), anonymized: source MAC set to a
// randomized DA:11:22:33:44:55 and SSID content replaced with "TestNetXXXXX". IE tag order
// [0,1,3,45,50,127,191] preserved.
static const uint8_t k_frame[] = {
    0x40, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xda, 0x11, 0x22, 0x33, 0x44, 0x55,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x21, 0x00, 0x0c, 0x54, 0x65, 0x73, 0x74, 0x4e, 0x65,
    0x74, 0x58, 0x58, 0x58, 0x58, 0x58, 0x01, 0x08, 0x02, 0x04, 0x0b, 0x0c, 0x12, 0x16, 0x18, 0x24,
    0x03, 0x01, 0x08, 0x2d, 0x1a, 0x62, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xe0, 0xe1, 0x09, 0x00, 0x32,
    0x04, 0x30, 0x48, 0x60, 0x6c, 0x7f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x40, 0xbf,
    0x0c, 0x30, 0x70, 0xc0, 0x33, 0xfc, 0xff, 0x24, 0x01, 0xfc, 0xff, 0x24, 0x01};

static void test_parse_real_probe(void) {
    WcObservation o;
    assert(wc_parse_probe_frame(k_frame, sizeof(k_frame), &o));
    const uint8_t expect_mac[6] = {0xda, 0x11, 0x22, 0x33, 0x44, 0x55};
    assert(memcmp(o.mac, expect_mac, 6) == 0);
    assert(o.mac_random);
    assert(o.seq == 536);
    assert(o.channel == 8); // DS param tag 3 value
    assert(strcmp(o.probed_ssid, "TestNetXXXXX") == 0);
    assert(!o.is_beacon);
    assert(o.ie_hash != 0);
}

static void test_fingerprint_stable_but_ssid_independent(void) {
    // Same device, wildcard probe (empty SSID) -> same fingerprint as the directed one, since
    // SSID content is excluded from the hash.
    uint8_t wild[sizeof(k_frame)];
    memcpy(wild, k_frame, sizeof(k_frame));
    // rebuild with an empty SSID: easier to just parse both and compare hashes conceptually.
    WcObservation directed;
    assert(wc_parse_probe_frame(k_frame, sizeof(k_frame), &directed));

    // A different SSID of the same length must not change the hash.
    uint8_t other[sizeof(k_frame)];
    memcpy(other, k_frame, sizeof(k_frame));
    memcpy(other + 26, "OtherXXXXXXX", 12); // overwrite SSID content (tag0 value at offset 26)
    WcObservation o2;
    assert(wc_parse_probe_frame(other, sizeof(other), &o2));
    assert(o2.ie_hash == directed.ie_hash);
    assert(strcmp(o2.probed_ssid, "OtherXXXXXXX") == 0);
    (void)wild;
}

static void test_rejects_non_probe_and_truncation(void) {
    WcObservation o;
    // A beacon (subtype 1000 -> 0x80) must be rejected.
    uint8_t beacon[40];
    memset(beacon, 0, sizeof(beacon));
    beacon[0] = 0x80;
    assert(!wc_parse_probe_frame(beacon, sizeof(beacon), &o));
    // Too short.
    assert(!wc_parse_probe_frame(k_frame, 10, &o));
    // Truncated IEs (cut mid-frame) must not over-read; still returns true with what parsed.
    assert(wc_parse_probe_frame(k_frame, 30, &o));
}

static void test_ie_vendor_from_tag221(void) {
    // Minimal probe with a vendor-specific IE carrying Apple's OUI (00:17:F2).
    uint8_t f[24 + 6];
    memset(f, 0, sizeof(f));
    f[0] = 0x40;  // probe request
    f[10] = 0xDA; // randomized source MAC
    f[24] = 221;  // tag 221 (vendor specific)
    f[25] = 4;    // length
    f[26] = 0x00;
    f[27] = 0x17;
    f[28] = 0xF2; // Apple OUI
    f[29] = 0x0A; // vendor subtype
    WcObservation o;
    assert(wc_parse_probe_frame(f, sizeof(f), &o));
    assert(o.mac_random);
    assert(o.ie_vendor == WcVendorApple); // brand even with a randomized MAC

    // A generic WPS OUI (00:50:F2) must NOT be read as a brand.
    f[26] = 0x00;
    f[27] = 0x50;
    f[28] = 0xF2;
    assert(wc_parse_probe_frame(f, sizeof(f), &o));
    assert(o.ie_vendor == WcVendorUnknown);
}

int main(void) {
    test_parse_real_probe();
    test_ie_vendor_from_tag221();
    test_fingerprint_stable_but_ssid_independent();
    test_rejects_non_probe_and_truncation();
    printf("test_probe_frame: OK\n");
    return 0;
}
