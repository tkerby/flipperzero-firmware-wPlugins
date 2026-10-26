#include "include/domain/wc_known.h"
#include "include/domain/wc_signature.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WcSignature sig_stable(const uint8_t mac[6]) {
    WcSignature s;
    memset(&s, 0, sizeof(s));
    memcpy(s.mac, mac, 6);
    s.mac_random = false;
    return s;
}

static WcSignature sig_random_with_ssid(const char *ssid) {
    WcSignature s;
    memset(&s, 0, sizeof(s));
    const uint8_t mac[6] = {0xDA, 0, 0, 0, 0, 1};
    memcpy(s.mac, mac, 6);
    s.mac_random = true;
    strncpy(s.ssids[0], ssid, WC_SSID_MAX_LEN);
    s.ssid_count = 1;
    return s;
}

static void test_rule_from_stable_mac(void) {
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    WcSignature s = sig_stable(mac);
    WcKnown k;
    assert(wc_known_rule_from_signature(&k, &s, "Pi lab"));
    assert(k.type == WcRuleByMac);
    assert(memcmp(k.mac, mac, 6) == 0);
}

static void test_rule_from_random_needs_ssid(void) {
    WcSignature with = sig_random_with_ssid("Ekin_Casa");
    WcKnown k;
    assert(wc_known_rule_from_signature(&k, &with, "Ekin"));
    assert(k.type == WcRuleBySsid);
    assert(strcmp(k.ssid, "Ekin_Casa") == 0);

    WcSignature without;
    memset(&without, 0, sizeof(without));
    without.mac_random = true; // no SSID
    assert(!wc_known_rule_from_signature(&k, &without, "Nope"));
}

static void test_rule_ssid_direct(void) {
    WcKnown k;
    assert(wc_known_rule_ssid(&k, "Casa_Ana", "Ana"));
    assert(k.type == WcRuleBySsid);
    assert(strcmp(k.ssid, "Casa_Ana") == 0);
    assert(strcmp(k.label, "Ana") == 0);
    // A device probing that network matches, whatever its (randomized) MAC.
    WcKnownDb db;
    wc_known_init(&db);
    assert(wc_known_add(&db, &k));
    WcSignature phone = sig_random_with_ssid("Casa_Ana");
    const WcKnown *m = wc_known_match(&db, &phone);
    assert(m && strcmp(m->label, "Ana") == 0);
    // An empty SSID is rejected.
    assert(!wc_known_rule_ssid(&k, "", "x"));
}

static void test_remove(void) {
    WcKnownDb db;
    wc_known_init(&db);
    WcKnown a, b, c;
    assert(wc_known_rule_ssid(&a, "A", "a"));
    assert(wc_known_rule_ssid(&b, "B", "b"));
    assert(wc_known_rule_ssid(&c, "C", "c"));
    wc_known_add(&db, &a);
    wc_known_add(&db, &b);
    wc_known_add(&db, &c);
    assert(db.count == 3);
    assert(wc_known_remove(&db, 1)); // remove the middle one
    assert(db.count == 2);
    assert(strcmp(db.items[0].ssid, "A") == 0);
    assert(strcmp(db.items[1].ssid, "C") == 0); // C shifted down
    assert(!wc_known_remove(&db, 5));           // out of range
}

static void test_match(void) {
    WcKnownDb db;
    wc_known_init(&db);
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    WcSignature s = sig_stable(mac);
    WcKnown bymac, byssid;
    wc_known_rule_from_signature(&bymac, &s, "Pi lab");
    WcSignature phone = sig_random_with_ssid("Ekin_Casa");
    wc_known_rule_from_signature(&byssid, &phone, "Ekin");
    assert(wc_known_add(&db, &bymac));
    assert(wc_known_add(&db, &byssid));

    const WcKnown *m = wc_known_match(&db, &s);
    assert(m && strcmp(m->label, "Pi lab") == 0);

    WcSignature other_phone = sig_random_with_ssid("Ekin_Casa");
    other_phone.mac[0] = 0xEE; // a different random MAC, same SSID
    m = wc_known_match(&db, &other_phone);
    assert(m && strcmp(m->label, "Ekin") == 0);

    uint8_t stranger[6] = {0x00, 0x1B, 0x21, 9, 9, 9};
    WcSignature s2 = sig_stable(stranger);
    assert(wc_known_match(&db, &s2) == NULL);
}

static void test_round_trip(void) {
    WcKnownDb db;
    wc_known_init(&db);
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    WcSignature s = sig_stable(mac);
    WcKnown k1, k2;
    wc_known_rule_from_signature(&k1, &s, "Pi lab");
    WcSignature phone = sig_random_with_ssid("Ekin_Casa");
    wc_known_rule_from_signature(&k2, &phone, "Ekin");
    wc_known_add(&db, &k1);
    wc_known_add(&db, &k2);

    uint8_t buf[1024];
    size_t n = wc_known_write(buf, sizeof(buf), &db);
    assert(n == wc_known_size(&db) && n > 0);

    WcKnownDb db2;
    assert(wc_known_read(&db2, buf, n));
    assert(db2.count == 2);
    assert(db2.items[0].type == WcRuleByMac);
    assert(strcmp(db2.items[0].label, "Pi lab") == 0);
    assert(db2.items[1].type == WcRuleBySsid);
    assert(strcmp(db2.items[1].ssid, "Ekin_Casa") == 0);

    // truncation and bad magic rejected
    WcKnownDb db3;
    assert(!wc_known_read(&db3, buf, n - 1));
    buf[0] = 'X';
    assert(!wc_known_read(&db3, buf, n));
}

int main(void) {
    test_rule_from_stable_mac();
    test_rule_from_random_needs_ssid();
    test_rule_ssid_direct();
    test_remove();
    test_match();
    test_round_trip();
    printf("test_known: OK\n");
    return 0;
}
