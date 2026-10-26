#include "include/domain/wc_marauder_parse.h"
#include "include/domain/wc_observation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool parse(const char *line, WcObservation *o) {
    return wc_parse_summary_line(line, strlen(line), o);
}

static void test_full_labeled_line(void) {
    WcObservation o;
    assert(parse("RSSI: -45 CH: 6 MAC: AC:DE:48:00:11:22 SSID: HomeNet", &o));
    const uint8_t expect[6] = {0xAC, 0xDE, 0x48, 0x00, 0x11, 0x22};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(!o.mac_random);
    assert(o.rssi == -45);
    assert(o.channel == 6);
    assert(strcmp(o.probed_ssid, "HomeNet") == 0);
    assert(!o.is_beacon);
}

static void test_wildcard_probe_random_mac(void) {
    WcObservation o;
    assert(parse("RSSI: -70 CH: 11 MAC: DA:11:22:33:44:55 SSID: ", &o));
    assert(o.mac_random);
    assert(o.rssi == -70);
    assert(o.channel == 11);
    assert(o.probed_ssid[0] == '\0'); // wildcard: no directed SSID
}

static void test_quoted_ssid_with_space(void) {
    WcObservation o;
    assert(parse("RSSI:-33 CH:1 MAC:00:1B:21:AA:BB:CC SSID:\"Cafe WiFi\"", &o));
    assert(o.rssi == -33);
    assert(o.channel == 1);
    assert(strcmp(o.probed_ssid, "Cafe WiFi") == 0);
}

static void test_lowercase_mac_no_labels(void) {
    WcObservation o;
    assert(parse("probe from b8:27:eb:aa:bb:cc", &o));
    const uint8_t expect[6] = {0xB8, 0x27, 0xEB, 0xAA, 0xBB, 0xCC};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(o.rssi == 0 && o.channel == 0);
    assert(o.probed_ssid[0] == '\0');
}

static void test_garbage_rejected(void) {
    WcObservation o;
    assert(!parse("no mac here at all", &o));
    assert(!parse("", &o));
    assert(!parse("aa:bb:cc", &o)); // partial MAC
}

static void test_ssid_over_32_truncated(void) {
    WcObservation o;
    char line[128];
    snprintf(line, sizeof(line), "MAC: 00:1B:21:00:00:01 SSID: %s",
             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 40 A's
    assert(parse(line, &o));
    assert(strlen(o.probed_ssid) == WC_SSID_MAX_LEN); // clamped to 32
}

static void test_long_line_is_safe(void) {
    WcObservation o;
    // MAC near the front of a very long line: parses fine, no overflow.
    char big[600];
    memset(big, 'X', sizeof(big));
    memcpy(big, "MAC: 00:1B:21:00:00:02 ", 23);
    big[sizeof(big) - 1] = '\0';
    assert(wc_parse_summary_line(big, strlen(big), &o));
    const uint8_t expect[6] = {0x00, 0x1B, 0x21, 0x00, 0x00, 0x02};
    assert(memcmp(o.mac, expect, 6) == 0);

    // MAC only beyond the parse window: safely truncated away -> not found, no crash.
    char big2[600];
    memset(big2, 'y', sizeof(big2));
    memcpy(big2 + 300, "00:1B:21:00:00:03", 17);
    assert(!wc_parse_summary_line(big2, sizeof(big2), &o));
}

static void test_long_digit_run_does_not_overflow(void) {
    WcObservation o;
    // A pathological RSSI with ~150 digits must not overflow the accumulator (UBSan).
    char line[220];
    int k = snprintf(line, sizeof(line), "MAC: 00:1B:21:00:00:04 RSSI:");
    for (int i = k; i < 200; i++)
        line[i] = '9';
    line[200] = '\0';
    assert(wc_parse_summary_line(line, strlen(line), &o));
    assert(o.rssi == 127); // clamped, no UB
}

static void test_quoted_ssid_without_label(void) {
    // Some Marauder builds print the probed network in quotes with no "SSID" token.
    WcObservation o;
    assert(parse("RSSI: -50 CH: 6 AA:BB:CC:DD:EE:FF -> \"HomeNet\"", &o));
    assert(strcmp(o.probed_ssid, "HomeNet") == 0);
}

// The three cases below are verbatim lines captured from a real ESP32 Marauder v1.17 running
// `sniffprobe`: "<-RSSI> Ch: <n> Client: <MAC> Requesting: <SSID>". RSSI is a bare leading
// value, the channel label is "Ch", the MAC is under "Client:", and the probed network follows
// "Requesting:" (empty for a wildcard probe).
static void test_marauder_v117_requesting_directed(void) {
    WcObservation o;
    assert(parse("-86 Ch: 1 Client: c0:38:96:31:db:b7 Requesting: Xiaomi 15T Pro", &o));
    const uint8_t expect[6] = {0xC0, 0x38, 0x96, 0x31, 0xDB, 0xB7};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(o.rssi == -86);
    assert(o.channel == 1);
    assert(strcmp(o.probed_ssid, "Xiaomi 15T Pro") == 0);
    assert(!o.is_beacon);
}

static void test_marauder_v117_wildcard_empty_requesting(void) {
    WcObservation o;
    assert(parse("-91 Ch: 1 Client: 96:8d:49:fc:e9:5f Requesting:", &o));
    const uint8_t expect[6] = {0x96, 0x8D, 0x49, 0xFC, 0xE9, 0x5F};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(o.rssi == -91);
    assert(o.channel == 1);
    assert(o.mac_random); // 0x96 has the locally-administered bit set
    assert(o.probed_ssid[0] == '\0');
}

static void test_marauder_v117_directed_underscore_ssid(void) {
    WcObservation o;
    assert(parse("-74 Ch: 1 Client: 7c:2e:bd:67:e8:bf Requesting: REDWIFI_Az3t", &o));
    const uint8_t expect[6] = {0x7C, 0x2E, 0xBD, 0x67, 0xE8, 0xBF};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(o.rssi == -74);
    assert(o.channel == 1);
    assert(!o.mac_random);
    assert(strcmp(o.probed_ssid, "REDWIFI_Az3t") == 0);
}

// "SSID" must only match where a word starts. Inside "BSSID" it captures an access point's MAC
// as the network a client was asking for, and that string can become a permanent known rule.
static void test_bssid_is_not_an_ssid(void) {
    WcObservation o;
    const char *line = "RSSI: -50 Ch: 6 BSSID: 00:11:22:33:44:55 ESSID: HomeNet";
    assert(wc_parse_summary_line(line, strlen(line), &o));
    // The MAC text must not become the network name, and the real name must not be lost.
    assert(strcmp(o.probed_ssid, "HomeNet") == 0);
    assert(o.rssi == -50);
    assert(o.channel == 6);
}

// A phone asking for a network called "BEACON" is a phone. Typing it as an access point drops
// it out of the linking rule and out of every comparison.
static void test_a_network_named_beacon_is_not_a_beacon(void) {
    WcObservation o;
    const char *line = "MAC: 02:11:22:33:44:55 RSSI: -60 SSID: BEACON";
    assert(wc_parse_summary_line(line, strlen(line), &o));
    assert(!o.is_beacon);
    assert(strcmp(o.probed_ssid, "BEACON") == 0);
}

int main(void) {
    test_full_labeled_line();
    test_marauder_v117_requesting_directed();
    test_marauder_v117_wildcard_empty_requesting();
    test_marauder_v117_directed_underscore_ssid();
    test_quoted_ssid_without_label();
    test_long_digit_run_does_not_overflow();
    test_wildcard_probe_random_mac();
    test_quoted_ssid_with_space();
    test_lowercase_mac_no_labels();
    test_garbage_rejected();
    test_ssid_over_32_truncated();
    test_long_line_is_safe();
    test_bssid_is_not_an_ssid();
    test_a_network_named_beacon_is_not_a_beacon();
    printf("test_marauder_parse: OK\n");
    return 0;
}
