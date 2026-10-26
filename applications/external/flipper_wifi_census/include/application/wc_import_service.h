#pragma once

#include "include/ports/wc_clock_port.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Read a raw-802.11 (linktype 105) pcap file at absolute path `pcap_path` (as picked in the file
// browser), build a census from its probe requests, and save it as "<out_basename>.wcen" (+ .csv).
// Streamed through a small window, so there is no size cap: a failed malloc reboots this
// hardware, and the file's length must not decide whether the app survives.
// Returns false if the file is missing, is not a supported pcap, or the save fails.
// `out_dropped` (nullable) receives how many devices the ceiling refused: a big pcap holds far
// more than the device can, and without this the loss happens before the file exists and is
// therefore invisible forever after.
bool wc_import_service_run(const WcStorePort *store, WcClockPort clock, const char *pcap_path,
                           const char *out_basename, uint16_t *out_dropped);
