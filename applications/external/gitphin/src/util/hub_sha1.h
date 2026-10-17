#pragma once

/* SHA-1 (FIPS 180-4), streaming. The SDK does not export one.
 * The state is about 100 bytes and the block function needs 64 bytes of stack. */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[5];
    uint64_t len; /* bytes hashed */
    uint32_t n; /* bytes in buf */
    uint8_t buf[64];
} HubSha1;

void hub_sha1_init(HubSha1* s);
void hub_sha1_update(HubSha1* s, const void* data, size_t len);
/* Writes the 20 byte digest. The state is not usable after this. */
void hub_sha1_final(HubSha1* s, uint8_t out[20]);
