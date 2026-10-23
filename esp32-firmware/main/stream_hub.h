#pragma once
// Stream hub — one live MP3 stream, many listeners.
//
// True radio semantics: there is a single write cursor (fed by the Flipper over
// UART, or by the Phase-1 test source) and every listener reads from the same
// ring buffer at its own cursor. A listener that falls further behind than the
// buffer holds is fast-forwarded to the live edge: on a radio you miss what you
// were late for, you don't get a private delayed copy.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_proto.h"

// ~64 KB per protocol.md: 4 seconds of 128 kbps audio, which covers UART
// hiccups while staying well inside the S2's internal RAM.
#define STREAM_RING_SIZE (64 * 1024)
#define STREAM_MAX_LISTENERS 8
// Head start handed to a joining listener: ~1 s of 128 kbps audio, enough for
// iOS to identify the stream at once instead of waiting it out at the live edge.
#define STREAM_PRIME_BYTES (16 * 1024)
// A listener that has not read for this long is not being drained -- the tab is
// paused, locked, closed, or its buffer is full -- so it stops counting against
// the writer. Without this a listener's backlog can grow until credit reaches
// zero, and the only thing that releases it (being lapped) needs the writes
// that credit just stopped. Generous on purpose: well inside the 4 s ring, but
// far longer than the gaps a healthy player leaves between reads.
#define STREAM_STALL_IGNORE_MS 2000
// Credit never drops below this, whatever the listeners are doing. The Flipper
// paces at the file's own bitrate, so this is not the clock -- it is the
// guarantee that the writer can ALWAYS make progress, because every mechanism
// for recovering from a stuck listener depends on more bytes being written.
#define STREAM_MIN_CREDIT (4 * RADIO_MAX_PAYLOAD)
// Give up hunting for an MP3 frame header after this much data and hand the
// bytes over regardless. A decoder resyncs by itself mid-stream; a listener
// that waits forever for a header that never parses just reads as "stalled"
// with no way to tell it apart from the Flipper having stopped sending.
#define STREAM_SYNC_SCAN_MAX (8 * 1024)
// Sized by the protocol, not chosen here: TRACK_META arrives over the wire and
// is stored verbatim, so this must match the FAP's cap exactly.
#define STREAM_META_MAX ((int)RADIO_META_MAX)

typedef struct StreamListener StreamListener;

void stream_hub_init(void);

// True once stream_hub_init() has run. Anything polling the hub from a task
// started earlier in boot MUST check this first: every call below takes a mutex
// that does not exist until init, and taking a NULL semaphore is a FreeRTOS
// assert, not a graceful failure.
bool stream_hub_ready(void);

// Append audio to the live stream. Always accepts the whole buffer; the oldest
// unread bytes are overwritten if listeners are not keeping up.
void stream_hub_write(const uint8_t* data, size_t len);

// Drop everything buffered (track skip — RADIO_MSG_FLUSH).
void stream_hub_flush(void);

// NULL when STREAM_MAX_LISTENERS are already connected.
StreamListener* stream_hub_listener_add(void);
void stream_hub_listener_remove(StreamListener* l);

// Copy up to `max` bytes for this listener. Returns 0 when nothing new is
// buffered yet; the caller should wait briefly and retry.
size_t stream_hub_listener_read(StreamListener* l, uint8_t* out, size_t max);

uint8_t stream_hub_listener_count(void);

// Hang up on whoever is furthest behind. Used when every slot is taken: a slot
// held by a phone that has gone away must not keep the next person out. The
// victim's own task notices via stream_hub_listener_evicted() and releases it.
bool stream_hub_evict_stalest(void);

// True once this listener has been hung up on; its task must stop and remove
// it. Reading from an evicted listener always returns 0.
bool stream_hub_listener_evicted(StreamListener* l);

// How full the ring is, 0-100 — the `buf` field in RADIO_MSG_STATS.
uint8_t stream_hub_fill_percent(void);

// Bytes the writer may send before the slowest listener loses audio. This is
// the credit figure granted back to the Flipper.
uint16_t stream_hub_credit(void);

void stream_hub_set_meta(const char* json);
void stream_hub_get_meta(char* out, size_t out_cap);

// Total bytes ever written — lets /status show whether audio is actually flowing.
uint64_t stream_hub_total_written(void);
