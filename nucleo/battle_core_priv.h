// Private to battle_core.c and battle_load.c (contract 38.1): the record sizes of the data files, the load-time scan switch, the little-endian readers and the record cache block.
// battle_load.c is compiled into the UI plugin on the device and included by battle_core.c on the host, so these live in a header both read.
#pragma once
#include "battle_core.h"
#include <stdlib.h>
#include <string.h>

#define SPECIES_REC 40u
#define MOVE_REC 12u
#define TYPE_REC 4u
#define CHART_BYTES(n) ((uint32_t)(n) + (((uint32_t)(n) + 3u) >> 2)) // contract 83: the packed chart of moves.bin: n bytes (att | def << 4) + ceil(n / 4) bytes of 2-bit multiplier codes
#define TRAINER_REC 32u
#define ITEM_REC 8u
#define MAX_FILE 65536
// Every record is validated when it is READ (rec_*; a bad one answers the all-zero record). The host also scans them all at load, so a bad data set is
// rejected as before (bc_test fuzz); the device skips the scan (a full read of reg.bin at every start; ~0.4 KB of code). bc_test builds -DBC_SCAN=0 to fuzz the device's path.
#ifndef BC_SCAN
#ifdef NUCLEO_DEVICE
#define BC_SCAN 0
#else
#define BC_SCAN 1
#endif
#endif


static inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// ---- on-demand records: a few LRU caches in ONE heap block (battle_core.c fills it, battle_load.c allocates it). An empty entry has id 0 (id 0 itself is never cached).
struct BcCache {
    uint16_t clock;
    uint8_t sp_id[BC_CACHE_SP], mv_id[BC_CACHE_MV], tr_id[BC_CACHE_TR];
    uint16_t sp_age[BC_CACHE_SP], mv_age[BC_CACHE_MV], tr_age[BC_CACHE_TR];
    SpeciesDef sp[BC_CACHE_SP];
    MoveDef mv[BC_CACHE_MV];
    TrainerDef tr[BC_CACHE_TR];
};
