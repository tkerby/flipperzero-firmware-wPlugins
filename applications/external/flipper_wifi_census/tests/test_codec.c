#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WcSignature *add_device(WcCensus *c, const uint8_t mac[6], bool random, WcDeviceType type,
                               int8_t rssi, uint32_t obs, uint32_t first, uint32_t last) {
    WcSignature d;
    memset(&d, 0, sizeof(d));
    memcpy(d.mac, mac, 6);
    d.mac_random = random;
    d.type = type;
    d.rssi_max = rssi;
    d.obs_count = obs;
    d.first_seen = first;
    d.last_seen = last;
    return wc_census_add(c, &d);
}

static void test_round_trip(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t m1[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    uint8_t m2[6] = {0xDA, 0, 0, 0, 0, 9};
    WcSignature *d0 = add_device(&c, m1, false, WcDeviceIot, -55, 10, 100, 200);
    wc_signature_add_ssid(d0, "Home,Net"); // comma to stress CSV, ok in binary
    d0->ie_hash = 0xDEADBEEFu;
    d0->ie_vendor = WcVendorApple;
    add_device(&c, m2, true, WcDevicePhone, -70, 3, 150, 160);

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.label, "bar_sabado", WC_LABEL_MAX);
    meta.epoch = 1758200000u;
    meta.duration_s = 300;
    meta.channels_mask = 0x1FFF; // channels 1-13
    meta.mode = 0;

    uint8_t buf[4096];
    size_t n = wc_capture_write(buf, sizeof(buf), &meta, &c);
    assert(n == wc_capture_size(&c));
    assert(n > 0);

    WcCaptureMeta meta2;
    WcCensus c2;
    wc_census_init(&c2);
    assert(wc_capture_read(&meta2, &c2, buf, n));
    assert(strcmp(meta2.label, "bar_sabado") == 0);
    assert(meta2.epoch == meta.epoch);
    assert(meta2.duration_s == 300);
    assert(meta2.channels_mask == 0x1FFF);
    assert(c2.count == 2);
    assert(memcmp(c2.devices[0].mac, m1, 6) == 0);
    assert(c2.devices[0].type == WcDeviceIot);
    assert(c2.devices[0].rssi_max == -55);
    assert(c2.devices[0].obs_count == 10);
    assert(c2.devices[0].first_seen == 100 && c2.devices[0].last_seen == 200);
    assert(c2.devices[0].ssid_count == 1);
    assert(strcmp(c2.devices[0].ssids[0], "Home,Net") == 0);
    assert(c2.devices[1].mac_random);
    assert(c2.devices[1].type == WcDevicePhone);
    assert(c2.devices[0].ie_hash == 0xDEADBEEFu);
    assert(c2.devices[0].ie_vendor == WcVendorApple);
}

// A capture with more devices than the ceiling is rejected, not loaded: the Flipper hasn't the
// RAM to browse it, so the reader returns false and the UI shows a clear "too large" message
// instead of silently opening an empty list.
static void test_rejects_capture_over_cap(void) {
    const uint16_t big = WC_CENSUS_MAX_DEVICES + 50; // above the ceiling
    WcCensus c;
    wc_census_init(&c);
    wc_census_set_max(&c, big); // writer allowed to build an oversized file (as a PC tool would)
    for (uint16_t i = 0; i < big; i++) {
        uint8_t mac[6] = {0x00, 0x1B, 0x21, (uint8_t)(i >> 8), (uint8_t)i, 0x01};
        add_device(&c, mac, false, WcDeviceLaptop, -60, 1, 100, 200);
    }
    assert(c.count == big);

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    meta.epoch = 1758200000u;
    size_t need = wc_capture_size(&c);
    uint8_t *buf = malloc(need);
    assert(buf);
    assert(wc_capture_write(buf, need, &meta, &c) == need);

    WcCaptureMeta m2;
    WcCensus c2;
    wc_census_init(&c2);
    assert(!wc_capture_read(&m2, &c2, buf, need)); // rejected: over the device ceiling
    // ...but the same file opens into a census whose ceiling was raised, which is exactly what
    // the host tools do. The ceiling is the caller's, never this build's.
    wc_census_set_max(&c2, 60000);
    assert(wc_capture_read(&m2, &c2, buf, need));
    assert(c2.count > WC_CENSUS_MAX_DEVICES);
    assert(c2.dropped == 0); // read whole or refused, never silently clipped
    wc_census_free(&c2);
    wc_census_free(&c);
    free(buf);
}

static void test_reads_v1_capture(void) {
    // A hand-built v1 capture (no IE fields) must still load, with ie_hash/ie_vendor defaulted.
    uint8_t buf[512];
    memset(buf, 0, sizeof(buf));
    size_t o = 0;
    memcpy(buf, "WCEN", 4);
    o = 4;
    buf[o++] = 1; // version 1 (LE)
    buf[o++] = 0;
    o += WC_LABEL_MAX;  // label (zeros)
    o += 4 + 4 + 2 + 1; // epoch, duration, channels, mode (zeros)
    buf[o++] = 1;       // count = 1
    buf[o++] = 0;
    size_t rec = o;
    const uint8_t mac[6] = {0x00, 0x1B, 0x21, 1, 2, 3};
    memcpy(buf + o, mac, 6);
    o += 6;
    buf[o++] = 0;                                  // mac_random
    buf[o++] = WcDeviceLaptop;                     // type
    buf[o++] = (uint8_t)(-50);                     // rssi
    buf[o++] = 0;                                  // ssid_count
    o += 4 + 4 + 4;                                // obs, first, last
    o += WC_SIG_MAX_SSIDS * (WC_SSID_MAX_LEN + 1); // ssid slots
    size_t v1_len = o;
    (void)rec;

    WcCaptureMeta m;
    WcCensus c;
    wc_census_init(&c);
    assert(wc_capture_read(&m, &c, buf, v1_len));
    assert(c.count == 1);
    assert(c.devices[0].type == WcDeviceLaptop);
    assert(c.devices[0].ie_hash == 0);
    assert(c.devices[0].ie_vendor == WcVendorUnknown);
}

static void test_write_rejects_small_buffer(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    add_device(&c, mac, false, WcDeviceLaptop, -60, 1, 1, 1);
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    uint8_t tiny[8];
    assert(wc_capture_write(tiny, sizeof(tiny), &meta, &c) == 0);
}

static void test_read_rejects_bad_input(void) {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    WcCaptureMeta meta;
    WcCensus c;
    // too short
    assert(!wc_capture_read(&meta, &c, buf, 3));
    // bad magic (all zeros)
    assert(!wc_capture_read(&meta, &c, buf, sizeof(buf)));

    // valid empty capture, then corrupt version
    WcCensus empty;
    wc_census_init(&empty);
    WcCaptureMeta m0;
    memset(&m0, 0, sizeof(m0));
    uint8_t good[512];
    size_t n = wc_capture_write(good, sizeof(good), &m0, &empty);
    assert(n > 0);
    assert(wc_capture_read(&meta, &c, good, n)); // sanity: empty round-trips
    good[4] = 0xFF;                              // clobber version low byte
    assert(!wc_capture_read(&meta, &c, good, n));
}

static void test_read_rejects_truncation(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 9, 9, 9};
    add_device(&c, mac, false, WcDeviceIot, -50, 1, 1, 1);
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    uint8_t buf[512];
    size_t n = wc_capture_write(buf, sizeof(buf), &meta, &c);
    WcCaptureMeta m2;
    WcCensus c2;
    assert(!wc_capture_read(&m2, &c2, buf, n - 1)); // one byte short -> exact-size check fails
}

static void test_csv(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac[6] = {0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03};
    WcSignature *d0 = add_device(&c, mac, false, WcDeviceIot, -55, 7, 100, 200);
    wc_signature_add_ssid(d0, "Cafe,WiFi"); // comma must be quoted
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.label, "loc", WC_LABEL_MAX);

    char out[512];
    size_t len = wc_capture_to_csv(out, sizeof(out), &meta, &c);
    assert(len < sizeof(out));
    assert(strstr(
        out, "mac,random,type,vendor,fingerprint,rssi_max,obs_count,first_seen,last_seen,ssids"));
    assert(strstr(out, "AB:CD:EF:01:02:03,0,iot,,00000000,-55,7,100,200,\"Cafe,WiFi\""));

    // Truncation: a tiny buffer stays NUL-terminated and reports the full needed length.
    char small[16];
    size_t need = wc_capture_to_csv(small, sizeof(small), &meta, &c);
    assert(need == len); // needed length independent of cap
    assert(small[15] == '\0' || strlen(small) < sizeof(small));
}

// Peeking the count must work from the header alone: it is what lets the app say "this file
// holds 187 devices and this build opens 100" instead of "could not read".
static void test_peek_count_from_header(void) {
    WcCensus c;
    wc_census_init(&c);
    for (uint16_t i = 0; i < 7; i++) {
        WcSignature sig;
        memset(&sig, 0, sizeof(sig));
        sig.mac[5] = (uint8_t)i;
        assert(wc_census_add(&c, &sig));
    }
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "peek");
    size_t need = wc_capture_size(&c);
    uint8_t *buf = malloc(need);
    assert(buf);
    assert(wc_capture_write(buf, need, &meta, &c) == need);

    assert(wc_capture_peek_count(buf, need) == 7);
    // The header alone is enough - no need for the records to be present.
    assert(wc_capture_peek_count(buf, wc_capture_header_size()) == 7);
    // Not a capture, and too short to be one.
    const uint8_t junk[64] = {'N', 'O', 'P', 'E'};
    assert(wc_capture_peek_count(junk, sizeof(junk)) == 0);
    assert(wc_capture_peek_count(buf, 4) == 0);

    free(buf);
    wc_census_free(&c);
}

int main(void) {
    test_round_trip();
    test_rejects_capture_over_cap();
    test_reads_v1_capture();
    test_write_rejects_small_buffer();
    test_read_rejects_bad_input();
    test_read_rejects_truncation();
    test_csv();
    test_peek_count_from_header();
    printf("test_codec: OK\n");
    return 0;
}
