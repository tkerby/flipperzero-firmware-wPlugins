#pragma once

#include "include/domain/wc_census.h"

#include <stddef.h>

#define WC_LABEL_MAX 32
#define WC_CAP_VERSION 2

// Capture metadata written alongside the device records. `channels_mask` has bit (ch-1) set
// for each 2.4 GHz channel scanned (1..14); `mode` is reserved (0 = census).
typedef struct {
    char label[WC_LABEL_MAX + 1];
    uint32_t epoch;
    uint32_t duration_s;
    uint16_t channels_mask;
    uint8_t mode;
} WcCaptureMeta;

// Exact byte size a binary capture of `c` will occupy.
size_t wc_capture_size(const WcCensus *c);

// Fixed serialized sizes, for streaming a capture record-by-record (no whole-file buffer).
size_t wc_capture_header_size(void);
size_t wc_capture_record_size(void);
// The stride of a stored capture of that version, which is NOT always the current one: v1
// records are shorter, and reading them at the v2 stride walks off the records.
size_t wc_capture_record_size_of(uint16_t version);

// Serialize just the header / one device record into `out` (>= the sizes above).
void wc_capture_put_header(uint8_t *out, const WcCaptureMeta *meta, uint16_t count);
void wc_capture_put_record(uint8_t *out, const WcSignature *d);

// Parse just the header / one record, for reading a capture straight off the SD without ever
// holding it whole. `wc_capture_get_header` fills `meta` and the device count; both return
// false on malformed input.
bool wc_capture_get_header(const uint8_t *in, size_t len, WcCaptureMeta *meta, uint16_t *count,
                           uint16_t *version);
bool wc_capture_get_record(const uint8_t *in, size_t len, uint16_t version, WcSignature *out);

// CSV, one piece at a time (for streaming): the header line and one device row. Each writes a
// NUL-terminated string into `out`/`cap` and returns the full length needed (snprintf-style).
size_t wc_capture_csv_header(char *out, size_t cap);
size_t wc_capture_csv_row(char *out, size_t cap, const WcSignature *d);

// Serialize meta + census into `buf` (little-endian, endian-independent). Returns bytes
// written, or 0 if `cap` is too small.
size_t wc_capture_write(uint8_t *buf, size_t cap, const WcCaptureMeta *meta, const WcCensus *c);

// Parse a capture into `c`, which MUST already be initialized; its ceiling is kept and its
// contents replaced. Bounds-checked, and refused whole rather than loaded in part.
bool wc_capture_read(WcCaptureMeta *meta, WcCensus *c, const uint8_t *buf, size_t len);

// How many devices a stored capture declares, read from its header alone. Returns 0 when the
// buffer is not a capture. Lets a caller tell "this file holds more devices than this build can
// open" apart from "this file is broken" - the same refusal otherwise reads as corruption.
uint16_t wc_capture_peek_count(const uint8_t *buf, size_t len);

// Render a human-readable CSV of the capture into `out` (NUL-terminated). Returns the length
// that a full render needs (excluding the NUL); if it is >= `cap` the output was truncated.
size_t wc_capture_to_csv(char *out, size_t cap, const WcCaptureMeta *meta, const WcCensus *c);
