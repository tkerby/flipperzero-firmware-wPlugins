#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Called once per captured frame. `frame` is valid only for the call.
typedef void (*WcPcapFrameFn)(void *ctx, const uint8_t *frame, size_t len);

// Iterate a libpcap buffer (classic format, either byte order) whose link type is
// LINKTYPE_IEEE802_11 (105 — raw 802.11, as Marauder writes). Invokes `cb` for each record's
// frame, bounds-checked. Returns false if the global header is missing/invalid or the link
// type is not 105 (e.g. a radiotap capture), in which case `cb` is not called.
bool wc_pcap_read(const uint8_t *data, size_t len, WcPcapFrameFn cb, void *ctx);

// Pulls up to `cap` bytes from `offset` into `buf`; returns bytes read, 0 at end or on error.
typedef size_t (*WcPcapPullFn)(const void *ctx, size_t offset, uint8_t *buf, size_t cap);

// As wc_pcap_read, but pulled through a small window instead of held whole: any capture then
// costs a fixed few kilobytes rather than its own length in one block.
bool wc_pcap_stream(WcPcapPullFn pull, const void *pull_ctx, WcPcapFrameFn cb, void *ctx);
