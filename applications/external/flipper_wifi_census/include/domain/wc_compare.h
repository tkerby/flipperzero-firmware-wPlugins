#pragma once

#include "include/domain/wc_census.h"

#include <stddef.h>

// How many matches are listed in detail. The intersection COUNT is always exact; only the
// listing stops here. The host tools raise it (-D) because two merged censuses can share far
// more devices than a Flipper capture ever holds.
#ifndef WC_COMPARE_MAX_MATCHES
#define WC_COMPARE_MAX_MATCHES WC_CENSUS_MAX_DEVICES
#endif

typedef enum {
    WcMatchByMac = 0, // same stable MAC in both captures
    WcMatchBySsid,    // both probe the same directed SSID (client, not an AP)
} WcMatchReason;

typedef struct {
    uint8_t mac[6];                   // representative MAC (from capture A)
    WcMatchReason reason;             // why it was considered the same device
    char detail[WC_SSID_MAX_LEN + 1]; // the shared SSID when reason == WcMatchBySsid
} WcMatch;

typedef struct {
    uint16_t na;           // devices in A
    uint16_t nb;           // devices in B
    uint16_t intersection; // devices in A with a high-confidence match in B
    uint16_t random_a;     // randomized-MAC devices in A (reported as not reliably crossable)
    uint16_t random_b;     // randomized-MAC devices in B
    WcMatch matches[WC_COMPARE_MAX_MATCHES];
    uint16_t match_count;
} WcCompareResult;

// Compare two captures. Every match is high confidence by construction: only an identical
// stable MAC or a shared directed SSID (between non-AP devices) counts. Randomized-MAC
// devices with no directed SSID never match and are surfaced via random_a / random_b so the
// caller can be honest that they are not crossable.
void wc_compare(WcCompareResult *r, const WcCensus *a, const WcCensus *b);

// The device in `c` that is the same as `sig` under those rules, or NULL. Lets a caller ask
// the question across more than two captures: who was at all of them.
const WcSignature *wc_compare_find(const WcCensus *c, const WcSignature *sig, WcMatchReason *reason,
                                   char *shared_ssid);
