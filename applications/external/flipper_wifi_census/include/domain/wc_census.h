#pragma once

#include "include/domain/wc_observation.h"
#include "include/domain/wc_signature.h"

// Ceiling for a live scan, a merge, and a loaded capture (bounds the FAP heap). A capture file
// with more devices than this is rejected, not loaded — the Flipper hasn't the RAM to browse it;
// combine those on a PC with wc_merge.
#define WC_CENSUS_MAX_DEVICES 320
#define WC_CENSUS_GROW 16 // device-array growth chunk

// The set of unique devices seen over one scan session. The device array grows on demand
// (realloc), so RAM tracks the real device count — a quiet room costs a few hundred bytes, a
// packed venue grows toward the ceiling. Always pair wc_census_init with wc_census_free.
typedef struct {
    WcSignature *devices;
    uint16_t count;
    uint16_t capacity;
    uint16_t dropped; // observations discarded at the ceiling / on allocation failure
    uint16_t max; // per-census ceiling (WC_CENSUS_MAX_DEVICES on the Flipper; higher off-device)
} WcCensus;

typedef struct {
    uint16_t total;         // == count
    uint16_t unique_stable; // devices with a stable (non-random) MAC
    uint16_t random_count;  // devices with a randomized MAC (not linkable across sessions)
    uint8_t pct_random;     // random_count as a percentage of total
    uint16_t by_type[WcDeviceTypeCount]; // indexed by WcDeviceType
    uint16_t networks;                   // distinct directed SSIDs devices are probing for
    // Devices the ceiling refused. It rides inside the stats so that no screen can show the
    // totals without having the loss right there: reporting it must not depend on remembering.
    uint16_t dropped;
} WcCensusStats;

// One "network a device is looking for": a directed probe SSID and how many devices sought it.
typedef struct {
    char ssid[WC_SSID_MAX_LEN + 1];
    uint16_t devices;
} WcSsidTally;

// Tally the distinct directed SSIDs across the census. With out != NULL, fills up to `cap`
// entries (SSID + device count). Returns the number of distinct SSIDs (may exceed `cap`;
// pass out=NULL, cap=0 to only count).
uint16_t wc_census_ssid_tally(const WcCensus *c, WcSsidTally *out, uint16_t cap);

void wc_census_init(WcCensus *c);

// Raise (or lower) this census's device ceiling. Default after init is WC_CENSUS_MAX_DEVICES,
// which suits the Flipper's RAM; an off-device tool (e.g. merging many captures on a PC) can
// set it much higher. Call right after wc_census_init, before adding devices.
void wc_census_set_max(WcCensus *c, uint16_t max);

// Release the device array. Safe to call on a zeroed/empty census; leaves it re-init'd.
void wc_census_free(WcCensus *c);

// Append a copy of `sig` (growing the array). Returns the stored signature, or NULL at the
// ceiling / on allocation failure (bumps dropped). Used by the codec and tests.
WcSignature *wc_census_add(WcCensus *c, const WcSignature *sig);

// Pre-allocate room for `n` devices (capped at the ceiling) in one shot. Doing this before a
// scan or a load avoids repeated realloc growth — which transiently needs ~2x the array and
// can exhaust the FAP heap on a busy scan. Returns false on allocation failure.
bool wc_census_reserve(WcCensus *c, uint16_t n);

// Fold one observation into the census, deduping into an existing device or adding a new
// one. Dedup rules, in order:
//   1. exact MAC match -> same device (repeated frames from the current MAC);
//   2. else, a randomized-MAC probe naming a directed SSID that EXACTLY ONE known client
//      seeks -> linked to it (collapses MAC rotation);
//   3. else -> a new device.
// Rule 2 never links into an access point, and randomized MACs with no directed SSID are
// never merged across MACs (counted separately, reported as not-linkable).
// Two refinements, measured rather than assumed: a name sought by two devices is a place, not
// an identity, so it stops linking; and a device that names a network late is reconsidered
// then, since most are first heard on a nameless probe.
// Returns the new/updated signature, or NULL if the table was full.
WcSignature *wc_census_observe(WcCensus *c, const WcObservation *obs, uint32_t now);

WcCensusStats wc_census_stats(const WcCensus *c);

// Bound the phones behind the randomized MACs: distinct fingerprints <= phones <= randomized
// MACs. The fingerprint names a model, not a device (51 shared one in a real capture), so it
// bounds but never merges. False when nothing carries a fingerprint, as after a UART scan.
bool wc_census_phone_bound(const WcCensus *c, uint16_t *min_phones, uint16_t *max_phones);

// Merge every device of `src` into `dst` using the same dedup rules as a live session (same
// stable MAC, or a shared directed SSID for a randomized device), accumulating stats. Used
// to combine captures of one place taken on different days. Overflow bumps dst->dropped.
void wc_census_merge(WcCensus *dst, const WcCensus *src);

// Fold ONE device in, with the same rules. Lets a capture be merged straight off the SD, record
// by record, instead of being loaded into a second census first.
void wc_census_merge_one(WcCensus *dst, const WcSignature *src);
