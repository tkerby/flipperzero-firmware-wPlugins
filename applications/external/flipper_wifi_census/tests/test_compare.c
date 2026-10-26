#include "include/domain/wc_census.h"
#include "include/domain/wc_compare.h"
#include "include/domain/wc_observation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WcObservation obs_of(const uint8_t mac[6], const char *ssid, bool beacon) {
    WcObservation o;
    memset(&o, 0, sizeof(o));
    memcpy(o.mac, mac, 6);
    o.mac_random = wc_mac_is_random(mac);
    o.rssi = -60;
    o.channel = 6;
    o.is_beacon = beacon;
    if (ssid) {
        strncpy(o.probed_ssid, ssid, WC_SSID_MAX_LEN);
    }
    return o;
}

static void test_stable_mac_crosses(void) {
    WcCensus a, b;
    wc_census_init(&a);
    wc_census_init(&b);
    uint8_t laptop[6] = {0x00, 0x1B, 0x21, 1, 2, 3};
    uint8_t onlyA[6] = {0xB8, 0x27, 0xEB, 9, 9, 9};
    WcObservation oa = obs_of(laptop, NULL, false);
    WcObservation ob = obs_of(laptop, NULL, false);
    WcObservation oc = obs_of(onlyA, NULL, false);
    wc_census_observe(&a, &oa, 1);
    wc_census_observe(&a, &oc, 1);
    wc_census_observe(&b, &ob, 1);

    WcCompareResult r;
    wc_compare(&r, &a, &b);
    assert(r.na == 2 && r.nb == 1);
    assert(r.intersection == 1);
    assert(r.match_count == 1);
    assert(r.matches[0].reason == WcMatchByMac);
}

static void test_ssid_crosses_random_macs(void) {
    WcCensus a, b;
    wc_census_init(&a);
    wc_census_init(&b);
    // Friend's phone: different randomized MAC in each place, same directed SSID.
    uint8_t macA[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t macB[6] = {0xEE, 0, 0, 0, 0, 2};
    WcObservation oa = obs_of(macA, "Ekin_Casa", false);
    WcObservation ob = obs_of(macB, "Ekin_Casa", false);
    wc_census_observe(&a, &oa, 1);
    wc_census_observe(&b, &ob, 1);

    WcCompareResult r;
    wc_compare(&r, &a, &b);
    assert(r.intersection == 1);
    assert(r.matches[0].reason == WcMatchBySsid);
    assert(strcmp(r.matches[0].detail, "Ekin_Casa") == 0);
    assert(r.random_a == 1 && r.random_b == 1);
}

static void test_random_without_ssid_never_crosses(void) {
    WcCensus a, b;
    wc_census_init(&a);
    wc_census_init(&b);
    uint8_t macA[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t macB[6] = {0xEE, 0, 0, 0, 0, 2};
    WcObservation oa = obs_of(macA, NULL, false);
    WcObservation ob = obs_of(macB, NULL, false);
    wc_census_observe(&a, &oa, 1);
    wc_census_observe(&b, &ob, 1);

    WcCompareResult r;
    wc_compare(&r, &a, &b);
    assert(r.intersection == 0);
    assert(r.random_a == 1 && r.random_b == 1);
}

int main(void) {
    test_stable_mac_crosses();
    test_ssid_crosses_random_macs();
    test_random_without_ssid_never_crosses();
    printf("test_compare: OK\n");
    return 0;
}
