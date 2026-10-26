#pragma once
// The save.bin image (contract section 12.1), shared by save_io.c (UI plugin: the "Guardar" screen, the file code) and box.c (PC plugin: leaving the PC
// saves save.bin and box.bin together, contract 25). Header-only and static: every plugin carries its own copy of these few bytes, the layout is
// written down once. No furi.h.
#include <stdint.h>
#include <string.h>
#include "save.h"
#include "party.h"
#include "settings.h" // settings_crc32 (IEEE CRC32, shared with settings.bin)

#define SAVE_TAIL 4 // trailing CRC
#define SAVE_LEN  ((uint32_t)(SAVE_HDR_SIZE + sizeof(PartyState) + SAVE_TAIL))
_Static_assert(
    SAVE_HDR_SIZE == 16 + WORLD_FLAG_BYTES + SCRIPT_VARS && SCRIPT_VARS == 16,
    "save.bin layout: 16 B of header, flags[WORLD_FLAG_BYTES], vars[16], then the PartyState");

static inline void put32(uint8_t* p, uint32_t v) {
    for(int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static inline uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

// The current-format image (SAVE_FMT_VER) of the game, SAVE_LEN bytes.
static inline void save_encode(uint8_t* b, const World* w) {
    memset(b, 0, SAVE_LEN);
    memcpy(b, "NSAV", 4);
    b[4] = SAVE_FMT_VER;
    b[5] = w->map.id;
    b[6] = w->hero.ti;
    b[7] = w->hero.tj;
    b[8] = w->hero.facing & 7;
    b[9] = PARTY_STATE_VER;
    b[10] = (uint8_t)sizeof(PartyState);
    b[11] = (uint8_t)(sizeof(PartyState) >> 8);
    put32(b + 12, save_time_secs());
    memcpy(b + 16, w->flags, WORLD_FLAG_BYTES);
    memcpy(b + SAVE_HDR_SIZE - SCRIPT_VARS, w->vars, SCRIPT_VARS);
    memcpy(b + SAVE_HDR_SIZE, party_state(), sizeof(PartyState));
    put32(b + SAVE_LEN - SAVE_TAIL, settings_crc32(b, SAVE_LEN - SAVE_TAIL));
}
