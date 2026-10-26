#pragma once

#include "include/domain/wc_observation.h"

#include <stddef.h>

// Parse one raw 802.11 frame (linktype 105, no radiotap — as Marauder writes to pcap) into
// an observation. Only probe requests are accepted; anything else returns false. Fills MAC,
// randomized flag, sequence number, directed SSID, channel (from the DS-param IE), and an
// IE fingerprint (wc_observation.ie_hash). The fingerprint groups devices of the same
// model/config — it is a grouping/insight signal (shown in the CSV), NOT a unique-device id,
// so it is deliberately not used to merge devices or cross-match captures (that would
// under-count identical models). RSSI is left 0 (not present without radiotap). Bounds-
// checked against untrusted input. Returns true on a probe request.
bool wc_parse_probe_frame(const uint8_t *frame, size_t len, WcObservation *out);
