#pragma once

#include "include/domain/wc_signature.h"

#include <stddef.h>

#define WC_KNOWN_LABEL_MAX 24
#define WC_KNOWN_MAX 64
#define WC_KNOWN_VERSION 1

typedef enum {
    WcRuleByMac = 0, // match a specific stable MAC
    WcRuleBySsid,    // match any device probing this directed SSID
} WcKnownRuleType;

typedef struct {
    char label[WC_KNOWN_LABEL_MAX + 1];
    WcKnownRuleType type;
    uint8_t mac[6];                 // used when type == WcRuleByMac
    char ssid[WC_SSID_MAX_LEN + 1]; // used when type == WcRuleBySsid
} WcKnown;

typedef struct {
    WcKnown items[WC_KNOWN_MAX];
    uint16_t count;
} WcKnownDb;

void wc_known_init(WcKnownDb *db);

// Append a rule. Returns false if the db is full.
bool wc_known_add(WcKnownDb *db, const WcKnown *k);

// Build a rule from a device the user picked: a stable MAC becomes a MAC rule; a randomized
// MAC that probes a directed SSID becomes an SSID rule. A randomized MAC with no directed
// SSID cannot be turned into a durable rule -> returns false.
bool wc_known_rule_from_signature(WcKnown *out, const WcSignature *sig, const char *label);

// Build an SSID rule directly from a network name the user picked (any device probing this
// SSID will match). Returns false if the SSID is empty.
bool wc_known_rule_ssid(WcKnown *out, const char *ssid, const char *label);

// Remove the rule at `index`, shifting the rest down. Returns false if `index` is out of range.
bool wc_known_remove(WcKnownDb *db, uint16_t index);

// The first known rule that matches this device, or NULL.
const WcKnown *wc_known_match(const WcKnownDb *db, const WcSignature *sig);

size_t wc_known_size(const WcKnownDb *db);
size_t wc_known_write(uint8_t *buf, size_t cap, const WcKnownDb *db);
bool wc_known_read(WcKnownDb *db, const uint8_t *buf, size_t len);
