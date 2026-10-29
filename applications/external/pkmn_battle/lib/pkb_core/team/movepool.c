/* Reader for files/movepools.bin (see tools/gen/gen_movepools.py). */
#include <string.h>
#include <pkb/team.h>
#include "pb_tables.h"

#define MOVEPOOL_VERSION 3
#define MOVEPOOL_HEADER_LEN 8
#define MOVEPOOL_CHUNK 64 /* bytes read per storage call (an even number) */

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool pkb_movepool_check(const PkbBlobReader* r) {
    uint8_t h[MOVEPOOL_HEADER_LEN];
    if(!r->read(r->ctx, 0, h, sizeof(h))) return false;
    return memcmp(h, "PKMP", 4) == 0 && h[4] == MOVEPOOL_VERSION && h[5] == NUM_POKEMON;
}

int pkb_movepool_read(
    const PkbBlobReader* r,
    uint8_t species,
    uint8_t level,
    uint8_t out[PKB_MOVEPOOL_MAX],
    PkbSpeciesInfo* info) {
    uint8_t buf[MOVEPOOL_CHUNK];
    if(species < 1 || species > NUM_POKEMON) return -1;
    if(!r->read(r->ctx, MOVEPOOL_HEADER_LEN + 2u * (species - 1), buf, 4)) return -1;
    uint16_t start = rd16(buf), end = rd16(buf + 2);
    if(end < start + 2u || (end - start) % 2 || (end - start - 2) / 2 > PKB_MOVEPOOL_MAX) return -1;
    if(!r->read(r->ctx, start, buf, 2)) return -1;
    if(info) {
        info->min_level = buf[0];
        info->evolve_level = buf[1];
    }
    if(buf[0] > level) return 0;

    int n = 0;
    for(uint32_t pos = start + 2u; pos < end;) {
        uint32_t len = end - pos < MOVEPOOL_CHUNK ? end - pos : MOVEPOOL_CHUNK;
        if(!r->read(r->ctx, pos, buf, len)) return -1;
        for(uint32_t i = 0; i < len; i += 2) {
            if(buf[i + 1] <= level) out[n++] = buf[i];
        }
        pos += len;
    }
    return n;
}
