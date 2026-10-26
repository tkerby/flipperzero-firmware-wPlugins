#include "include/domain/wc_census.h"
#include "include/domain/wc_observation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WcObservation obs_of(const uint8_t mac[6], int8_t rssi, const char *ssid, bool beacon) {
    WcObservation o;
    memset(&o, 0, sizeof(o));
    memcpy(o.mac, mac, 6);
    o.mac_random = wc_mac_is_random(mac);
    o.rssi = rssi;
    o.channel = 6;
    o.is_beacon = beacon;
    if (ssid) {
        strncpy(o.probed_ssid, ssid, WC_SSID_MAX_LEN);
    }
    return o;
}

static void test_same_mac_is_one_device(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    WcObservation a = obs_of(mac, -60, NULL, false);
    WcObservation b = obs_of(mac, -50, NULL, false);
    wc_census_observe(&c, &a, 1);
    wc_census_observe(&c, &b, 2);
    assert(c.count == 1);
    assert(c.devices[0].obs_count == 2);
    assert(c.devices[0].rssi_max == -50);
}

static void test_ssid_links_rotating_random_macs(void) {
    WcCensus c;
    wc_census_init(&c);
    // Same phone, two different randomized MACs, both probing the same directed SSID.
    uint8_t mac1[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t mac2[6] = {0xDE, 0, 0, 0, 0, 2};
    WcObservation o1 = obs_of(mac1, -55, "MyHomeNet_5G", false);
    WcObservation o2 = obs_of(mac2, -57, "MyHomeNet_5G", false);
    wc_census_observe(&c, &o1, 1);
    wc_census_observe(&c, &o2, 2);
    assert(c.count == 1); // linked by shared directed SSID
}

static void test_random_without_ssid_never_merges(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac1[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t mac2[6] = {0xDE, 0, 0, 0, 0, 2};
    WcObservation o1 = obs_of(mac1, -55, NULL, false); // wildcard probe
    WcObservation o2 = obs_of(mac2, -57, NULL, false);
    wc_census_observe(&c, &o1, 1);
    wc_census_observe(&c, &o2, 2);
    assert(c.count == 2); // two distinct, not-linkable devices
}

static void test_ap_not_linked_to_client_by_ssid(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t ap[6] = {0x00, 0x1A, 0x11, 1, 1, 1};
    uint8_t client[6] = {0xDA, 0, 0, 0, 0, 9};
    // A beacon carries no probed SSID in this model; a client probes a directed SSID.
    WcObservation beacon = obs_of(ap, -40, NULL, true);
    WcObservation probe = obs_of(client, -60, "CorpWiFi", false);
    wc_census_observe(&c, &beacon, 1);
    wc_census_observe(&c, &probe, 2);
    assert(c.count == 2);
    assert(c.devices[0].type == WcDeviceAp);
}

static void test_stats(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t ap[6] = {0x00, 0x1A, 0x11, 1, 1, 1};     // Apple OUI -> but beacon -> AP
    uint8_t laptop[6] = {0x00, 0x1B, 0x21, 2, 2, 2}; // Intel -> laptop
    uint8_t phone[6] = {0xDA, 0, 0, 0, 0, 1};        // random -> phone
    WcObservation ob_ap = obs_of(ap, -40, NULL, true);
    WcObservation ob_lap = obs_of(laptop, -55, NULL, false);
    WcObservation ob_ph = obs_of(phone, -60, NULL, false);
    wc_census_observe(&c, &ob_ap, 1);
    wc_census_observe(&c, &ob_lap, 1);
    wc_census_observe(&c, &ob_ph, 1);

    WcCensusStats s = wc_census_stats(&c);
    assert(s.total == 3);
    assert(s.unique_stable == 2);
    assert(s.random_count == 1);
    assert(s.pct_random == 33);
    assert(s.by_type[WcDeviceAp] == 1);
    assert(s.by_type[WcDeviceLaptop] == 1);
    assert(s.by_type[WcDevicePhone] == 1);
}

static void test_table_full_drops(void) {
    WcCensus c;
    wc_census_init(&c);
    for (int i = 0; i < WC_CENSUS_MAX_DEVICES + 5; i++) {
        uint8_t mac[6] = {0x02, 0, 0, (uint8_t)(i >> 8), (uint8_t)i, 0};
        // 0x02 sets LAA; make each unique and wildcard so none merge.
        WcObservation o = obs_of(mac, -70, NULL, false);
        wc_census_observe(&c, &o, 1);
    }
    assert(c.count == WC_CENSUS_MAX_DEVICES);
    assert(c.dropped == 5);
}

static void test_ssid_tally(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t m1[6] = {0x00, 0x1B, 0x21, 0, 0, 1};
    uint8_t m2[6] = {0x00, 0x1B, 0x21, 0, 0, 2};
    uint8_t m3[6] = {0x00, 0x1B, 0x21, 0, 0, 3};
    WcObservation a = obs_of(m1, -50, "HomeA", false);
    WcObservation b = obs_of(m2, -50, "HomeA", false); // same net as a
    WcObservation d = obs_of(m3, -50, "HomeB", false);
    wc_census_observe(&c, &a, 1);
    wc_census_observe(&c, &b, 1);
    wc_census_observe(&c, &d, 1);

    WcCensusStats s = wc_census_stats(&c);
    assert(s.networks == 2); // HomeA, HomeB

    WcSsidTally t[8];
    uint16_t n = wc_census_ssid_tally(&c, t, 8);
    assert(n == 2);
    // HomeA sought by 2 devices, HomeB by 1 (order: first appearance)
    assert(strcmp(t[0].ssid, "HomeA") == 0 && t[0].devices == 2);
    assert(strcmp(t[1].ssid, "HomeB") == 0 && t[1].devices == 1);

    // count-only mode
    assert(wc_census_ssid_tally(&c, NULL, 0) == 2);
}

static void test_merge_accumulates(void) {
    // Day A: a laptop (stable) and a random phone probing "Ekin_Casa".
    WcCensus a;
    wc_census_init(&a);
    uint8_t laptop[6] = {0x00, 0x1B, 0x21, 1, 2, 3};
    uint8_t phoneA[6] = {0xDA, 0, 0, 0, 0, 1};
    WcObservation la = obs_of(laptop, -60, NULL, false);
    WcObservation pa = obs_of(phoneA, -55, "Ekin_Casa", false);
    wc_census_observe(&a, &la, 100);
    wc_census_observe(&a, &pa, 110);

    // Day B: the same laptop again (stronger), the same phone with a NEW random MAC but the
    // same SSID, and a brand-new stable device.
    WcCensus b;
    wc_census_init(&b);
    uint8_t phoneB[6] = {0xEE, 0, 0, 0, 0, 2};
    uint8_t newdev[6] = {0xB8, 0x27, 0xEB, 9, 9, 9};
    WcObservation lb = obs_of(laptop, -40, NULL, false);
    WcObservation pb = obs_of(phoneB, -50, "Ekin_Casa", false);
    WcObservation nb = obs_of(newdev, -70, NULL, false);
    wc_census_observe(&b, &lb, 200);
    wc_census_observe(&b, &pb, 210);
    wc_census_observe(&b, &nb, 220);

    wc_census_merge(&a, &b);
    // laptop merged, phone linked by SSID, newdev appended -> 3 devices.
    assert(a.count == 3);

    // laptop: obs summed, first=100 kept, last=200, rssi strongest -40.
    const WcSignature *lap = NULL;
    for (uint16_t i = 0; i < a.count; i++) {
        if (memcmp(a.devices[i].mac, laptop, 6) == 0) {
            lap = &a.devices[i];
        }
    }
    assert(lap && lap->obs_count == 2);
    assert(lap->first_seen == 100 && lap->last_seen == 200);
    assert(lap->rssi_max == -40);
}

static void test_merge_no_false_link(void) {
    // Two DIFFERENT stable devices sharing a common SSID must NOT merge across captures.
    WcCensus a, b;
    wc_census_init(&a);
    wc_census_init(&b);
    uint8_t d1[6] = {0x00, 0x1B, 0x21, 1, 1, 1};
    uint8_t d2[6] = {0x3C, 0xA9, 0xF4, 2, 2, 2};
    WcObservation o1 = obs_of(d1, -60, "CoffeeShop", false);
    WcObservation o2 = obs_of(d2, -60, "CoffeeShop", false);
    wc_census_observe(&a, &o1, 1);
    wc_census_observe(&b, &o2, 1);
    wc_census_merge(&a, &b);
    assert(a.count == 2); // distinct stable MACs, not merged by shared SSID
}

// The bound is the only honest use of the fingerprint: it groups phone MODELS, so it can put
// a floor under the phone count without ever merging two devices into one.
static void test_phone_bound(void) {
    WcCensus c;
    wc_census_init(&c);
    uint16_t lo = 0, hi = 0;

    // A live scan carries no fingerprints at all -> no bound can be stated.
    const uint8_t stable[6] = {0x00, 0x1B, 0x21, 0x00, 0x00, 0x01};
    const uint8_t rnd1[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    WcObservation o = obs_of(stable, -50, "Net", false);
    wc_census_observe(&c, &o, 1);
    o = obs_of(rnd1, -50, NULL, false);
    wc_census_observe(&c, &o, 1);
    assert(!wc_census_phone_bound(&c, &lo, &hi));

    // Three randomized MACs, two of them the same model: 2..3 phones, never 2 devices.
    const uint8_t rnd2[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
    const uint8_t rnd3[6] = {0x06, 0x00, 0x00, 0x00, 0x00, 0x03};
    wc_census_free(&c);
    wc_census_init(&c);
    o = obs_of(rnd1, -50, NULL, false);
    o.ie_hash = 0xAAAA;
    wc_census_observe(&c, &o, 1);
    o = obs_of(rnd2, -50, NULL, false);
    o.ie_hash = 0xAAAA; // same model as rnd1
    wc_census_observe(&c, &o, 1);
    o = obs_of(rnd3, -50, NULL, false);
    o.ie_hash = 0xBBBB;
    wc_census_observe(&c, &o, 1);
    assert(c.count == 3); // the fingerprint must NOT have merged anything
    assert(wc_census_phone_bound(&c, &lo, &hi));
    assert(lo == 2);
    assert(hi == 3);

    // A stable MAC is its own identity and never enters the bound.
    o = obs_of(stable, -50, "Net", false);
    o.ie_hash = 0xAAAA;
    wc_census_observe(&c, &o, 1);
    assert(wc_census_phone_bound(&c, &lo, &hi));
    assert(lo == 2 && hi == 3);

    // A randomized device with no fingerprint cannot join a group: it lifts the floor alone.
    const uint8_t rnd4[6] = {0x0A, 0x00, 0x00, 0x00, 0x00, 0x04};
    o = obs_of(rnd4, -50, NULL, false);
    wc_census_observe(&c, &o, 1);
    assert(wc_census_phone_bound(&c, &lo, &hi));
    assert(lo == 3);
    assert(hi == 4);

    wc_census_free(&c);
}

// Loss must travel with the numbers. A screen that shows totals gets the drop count in the
// same struct, so reporting it cannot depend on someone remembering to ask.
static void test_stats_carry_the_dropped_count(void) {
    WcCensus c;
    wc_census_init(&c);
    wc_census_set_max(&c, 3);
    for (uint16_t i = 0; i < 10; i++) {
        WcSignature sig;
        memset(&sig, 0, sizeof(sig));
        sig.mac[0] = 0x00;
        sig.mac[5] = (uint8_t)i;
        wc_census_add(&c, &sig);
    }
    WcCensusStats s = wc_census_stats(&c);
    assert(s.total == 3);
    assert(s.dropped == 7);
    assert(c.dropped == s.dropped);
    wc_census_free(&c);
}

// A phone whose first probe carried no name is recorded blind; the SSID rule must look again
// when it names a network later, or the rotation it exists to defeat slips past it.
static void test_late_link_catches_a_blind_birth(void) {
    WcCensus c;
    wc_census_init(&c);
    const uint8_t r1[6] = {0x02, 0, 0, 0, 0, 0x01};
    const uint8_t r2[6] = {0x06, 0, 0, 0, 0, 0x02};

    WcObservation o = obs_of(r1, -50, "MiCasa_4G", false);
    wc_census_observe(&c, &o, 1);
    // A different randomized MAC shows up with a nameless probe first: recorded blind.
    o = obs_of(r2, -50, NULL, false);
    wc_census_observe(&c, &o, 2);
    assert(c.count == 2);
    // ...and only then asks for the same network. That is the same phone, rotated.
    o = obs_of(r2, -50, "MiCasa_4G", false);
    wc_census_observe(&c, &o, 3);
    assert(c.count == 1);
    assert(wc_signature_has_ssid(&c.devices[0], "MiCasa_4G"));
    wc_census_free(&c);
}

// Measured on real captures: "DefaultSSID" was sought by 16 devices with distinct stable MACs.
// A name shared by two certain identities is a place, and must stop being usable as one.
static void test_a_shared_ssid_stops_being_an_identity(void) {
    WcCensus c;
    wc_census_init(&c);
    const uint8_t s1[6] = {0x00, 0x1B, 0x21, 0, 0, 0x01};
    const uint8_t s2[6] = {0x00, 0x1B, 0x21, 0, 0, 0x02};
    const uint8_t rnd[6] = {0x02, 0, 0, 0, 0, 0x09};

    WcObservation o = obs_of(s1, -50, "DefaultSSID", false);
    wc_census_observe(&c, &o, 1);
    o = obs_of(s2, -50, "DefaultSSID", false);
    wc_census_observe(&c, &o, 2);
    assert(c.count == 2); // two stable MACs are two devices, whatever they both seek

    // Now a rotating phone asks for it too. It must NOT be folded into either of them.
    o = obs_of(rnd, -50, "DefaultSSID", false);
    wc_census_observe(&c, &o, 3);
    assert(c.count == 3);
    wc_census_free(&c);
}

// A device only gets folded away when it is one of the two that seek the name. With its SSID
// slots already full it records none, so it is not a seeker at all - and must not be merged
// into a stranger that happens to share a name with somebody else.
static void test_late_link_never_touches_a_non_seeker(void) {
    WcCensus c;
    wc_census_init(&c);
    const uint8_t s1[6] = {0x00, 0x1B, 0x21, 0, 0, 0x01};
    const uint8_t s2[6] = {0x00, 0x1B, 0x21, 0, 0, 0x02};
    const uint8_t rnd[6] = {0x02, 0, 0, 0, 0, 0x09};

    // Two certain devices both seek "Cafe": it is a place, not an identity.
    WcObservation o = obs_of(s1, -50, "Cafe", false);
    wc_census_observe(&c, &o, 1);
    o = obs_of(s2, -50, "Cafe", false);
    wc_census_observe(&c, &o, 2);

    // A rotating phone fills both of its SSID slots with its own networks...
    o = obs_of(rnd, -50, "HomeA", false);
    wc_census_observe(&c, &o, 3);
    o = obs_of(rnd, -50, "WorkB", false);
    wc_census_observe(&c, &o, 4);
    assert(c.count == 3);
    // ...and then probes "Cafe" too, which it has no room to record.
    o = obs_of(rnd, -50, "Cafe", false);
    wc_census_observe(&c, &o, 5);

    assert(c.count == 3); // nobody was folded away
    bool home_still_its_own = false;
    for (uint16_t i = 0; i < c.count; i++) {
        if (c.devices[i].mac_random && wc_signature_has_ssid(&c.devices[i], "HomeA")) {
            home_still_its_own = true;
        }
    }
    assert(home_still_its_own); // its networks did not migrate into a stable device
    wc_census_free(&c);
}

int main(void) {
    test_same_mac_is_one_device();
    test_merge_accumulates();
    test_merge_no_false_link();
    test_ssid_tally();
    test_ssid_links_rotating_random_macs();
    test_random_without_ssid_never_merges();
    test_ap_not_linked_to_client_by_ssid();
    test_stats();
    test_table_full_drops();
    test_phone_bound();
    test_stats_carry_the_dropped_count();
    test_late_link_catches_a_blind_birth();
    test_a_shared_ssid_stops_being_an_identity();
    test_late_link_never_touches_a_non_seeker();
    printf("test_census: OK\n");
    return 0;
}
