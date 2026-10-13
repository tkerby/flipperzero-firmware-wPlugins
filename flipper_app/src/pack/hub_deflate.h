#pragma once

/* Small streaming zlib compressor (RFC 1950 / 1951) for packing loose Git
 * objects on the Flipper.
 *
 * LZ77 with a hash chain over a 4 KB window, greedy matching, fixed Huffman
 * blocks. A block of 1 KB input is stored uncompressed when that is smaller, so
 * the output never grows by more than a few bytes per 1 KB. The zlib header and
 * the Adler-32 trailer are written. One malloc'd state block of about 16 KB.
 * No recursion; small stack frames. Input and output are streamed.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Return false to abort. */
typedef bool (*HubDeflateOut)(void* ctx, const uint8_t* data, size_t len);

typedef struct HubDeflate HubDeflate;

/* NULL when out of memory. */
HubDeflate* hub_deflate_new(HubDeflateOut out, void* ctx);
/* Start a new stream with the same state block. */
void hub_deflate_reset(HubDeflate* d, HubDeflateOut out, void* ctx);
/* false once the output callback failed. */
bool hub_deflate_write(HubDeflate* d, const void* data, size_t len);
/* Final block and Adler-32 trailer. After this call only reset or free. */
bool hub_deflate_finish(HubDeflate* d);
void hub_deflate_free(HubDeflate* d);
