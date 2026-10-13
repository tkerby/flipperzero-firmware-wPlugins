#pragma once

/* Own zlib / deflate decoder (RFC 1950, RFC 1951) for the Git object reader.
 *
 * - zlib header, stored, fixed and dynamic Huffman blocks. Adler-32 optional.
 * - No recursion. One malloc'd state block (about 4.5 KB). Small stack frames.
 * - Input comes from a read callback. The decoder keeps its own 512-byte
 *   buffer, so the callback may be a plain file read.
 * - The decoder reports the EXACT number of input bytes the stream used
 *   (header, blocks and Adler trailer). Pack entries are back to back, so the
 *   caller can find the next entry without knowing the compressed length.
 *   The decoder may read more bytes than that from the callback; the caller
 *   must not count those.
 * - Output mode (a) hub_inflate_buf: into a caller buffer of known size. The
 *   buffer is the window.
 * - Output mode (b) hub_inflate_stream: to a write callback, with a malloc'd
 *   ring window of at most HUB_INF_RING (8 KB). A stream that declares a bigger
 *   window (git's own objects declare 32 KB) needs a "far" callback that reads
 *   output written so far (the caller's output file) for back-references that
 *   reach beyond the ring; without it the call fails with HUB_INF_ERR_FAR before
 *   any output. No code path allocates more than 8 KB for a window.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Return codes. 0 is success, everything else is an error. */
#define HUB_INF_OK        0
#define HUB_INF_ERR_DATA  (-1) /* bad stream */
#define HUB_INF_ERR_INPUT (-2) /* input ended too early */
#define HUB_INF_ERR_SPACE (-3) /* output is bigger than the buffer */
#define HUB_INF_ERR_NOMEM (-4)
#define HUB_INF_ERR_WRITE (-5) /* the write callback failed */
#define HUB_INF_ERR_ADLER (-6) /* checksum mismatch (only with HUB_INF_ADLER) */
#define HUB_INF_ERR_FAR \
    (-7) /* the stream declares a window bigger than the ring and no far callback was given (nothing was written) */

/* flags */
#define HUB_INF_RAW   1 /* raw deflate: no zlib header and no trailer */
#define HUB_INF_ADLER 2 /* check the Adler-32 trailer */
#define HUB_INF_PARTIAL \
    4 /* buf mode: stop with success when the buffer is full.
                           * *in_used is not valid then. Used to read a header. */

#define HUB_INF_RING 8192

/* Read up to max bytes. Return the number of bytes, 0 at the end or on error. */
typedef size_t (*HubInfRead)(void* ctx, uint8_t* buf, size_t max);
/* Return false to abort. */
typedef bool (*HubInfWrite)(void* ctx, const uint8_t* buf, size_t len);

/* Inflate into out (capacity cap).
 * out_len: bytes produced (may be NULL). in_used: input bytes used (may be NULL). */
int hub_inflate_buf(
    HubInfRead rd,
    void* rctx,
    uint8_t* out,
    size_t cap,
    uint32_t flags,
    size_t* out_len,
    size_t* in_used);

/* Read n bytes of the output at absolute position pos (bytes the write callback
 * has already taken). Return false on failure. */
typedef bool (*HubInfFar)(void* ctx, uint64_t pos, uint8_t* buf, size_t n);

/* Inflate to a write callback (ring window of up to 8 KB is malloc'd).
 * far/fctx may be NULL (see above). */
int hub_inflate_stream(
    HubInfRead rd,
    void* rctx,
    HubInfWrite wr,
    void* wctx,
    uint32_t flags,
    size_t* out_len,
    size_t* in_used,
    HubInfFar far,
    void* fctx);
