#pragma once
// Sprite bank (contract 3.1), parsed in place: no per-sprite structs, the bank block is the data.
// No furi.h (host-compilable). The bank pointer must be 4-byte aligned (heap blocks are).
#include <stdint.h>
#include <stdbool.h>
#include "blit.h"

enum { SPR_TILE = 0, SPR_OBJECT = 1, SPR_ACTOR = 2 };
// SPRF_MIRROR: only the directions that are not horizontal mirrors of another one are stored (ndirs 8: S SE E NE N, the
// other three are the flipped NE / E / SE; ndirs 4: S E N, W is the flipped E). bank_view() maps them and sets Blit.flip.
// SPRF_HOP (M3-E, actors): the walking NPC bobs (world.c npcs_pose: a small arc per step, frame 1 in the air).
// SPRF_STREAM (map file v2, actors): the pixel data is NOT in the resident bank block (data_off >= the bank header's `size`, it lies after it in the
// file); map_actor_view() (map.h) reads one facing at a time from the card. bank_view() answers false for such a sprite.
// SPRF_SLIDE_SHIFT (C4-A, contract 35, TILES only): bits 4..6 of a tile's `flags` = the facing (0..7) the tile pushes the hero to when its cflags carry C_SLIDE (an arrow floor).
// SPRF_SLIDE_BIKE (C5, contract 42.6, TILES only, with a slide facing): bit 7 = a bike slope: it only pushes the hero while he rides the bicycle (afoot it is plain floor).
enum { SPRF_MASK = 1, SPRF_MIRROR = 2, SPRF_HOP = 4, SPRF_STREAM = 8, SPRF_SLIDE_SHIFT = 4, SPRF_SLIDE_BIKE = 128 };

// Cflags (contract 3.1)
enum {
    C_SOLID = 1,
    C_GRASS = 2,
    C_WATER = 4,
    C_COUNTER = 8,
    C_WARP = 16,
    C_TRIGGER = 32,
    C_LEDGE = 64,
    C_SLIDE = 128, // C4-A (contract 35): an arrow floor tile; the push facing is in the bank entry's flags bits 4..6 (SPRF_SLIDE_SHIFT)
};

// Directory entry, 16 bytes, little-endian, exactly as stored in the file.
typedef struct {
    uint8_t kind;    // SPR_*
    uint8_t w, h;    // pixel size
    uint8_t ndirs;   // 1, 4 or 8
    uint8_t nframes; // per direction
    uint8_t flags;   // SPRF_MASK | SPRF_MIRROR | SPRF_HOP
    uint8_t fw, fh;  // footprint in tiles (OBJECT)
    uint8_t ax, ay;  // anchor pixel (its top-left corner sits on the anchor point)
    uint8_t cflags;  // TILE default / OBJECT footprint
    uint8_t fill;    // TILE: 0xFF none, else level 0..3 diamond UNDERLAY (tile pixels are blitted over it)
    uint8_t data_off[4]; // u32 LE from bank start (read with bank_data_off)
} SprEntry;
_Static_assert(sizeof(SprEntry) == 16, "SprEntry layout");

#define BANK_HDR 8

// Byte-wise little-endian read (never cast file bytes to wider types).
static inline uint32_t bank_data_off(const SprEntry* e) {
    return (uint32_t)e->data_off[0] | ((uint32_t)e->data_off[1] << 8) |
           ((uint32_t)e->data_off[2] << 16) | ((uint32_t)e->data_off[3] << 24);
}

// Checks header, directory and that every resident sprite's data lies inside `size` bytes (the resident bank block). A streamed sprite's data must
// lie behind the header's `size`, 4-aligned, and inside `limit` (the bank's bytes in the file: resident + streamed).
// Returns true when the bank can be used safely by bank_spr()/bank_view().
bool bank_validate_lim(const uint8_t* bank, uint32_t size, uint32_t limit);
static inline bool bank_validate(const uint8_t* bank, uint32_t size) { // a bank with nothing streamed
    return bank_validate_lim(bank, size, size);
}
// The bank header's `size`: bytes of the resident part (the whole bank when nothing is streamed).
static inline uint32_t bank_resident_size(const uint8_t* bank) {
    return (uint32_t)bank[4] | ((uint32_t)bank[5] << 8) | ((uint32_t)bank[6] << 16) | ((uint32_t)bank[7] << 24);
}
// Bytes of one frame of the sprite: pixels, then the mask when it has one (the unit of the file layout).
static inline uint32_t bank_frame_bytes(const SprEntry* e) {
    return (uint32_t)((e->w + 3u) / 4u) * e->h + ((e->flags & SPRF_MASK) ? (uint32_t)((e->w + 7u) / 8u) * e->h : 0u);
}
// Fills *out with the view of one frame whose bytes (pixels then mask) start at `p`.
void bank_fill_view(const SprEntry* e, const uint8_t* p, uint8_t flip, Blit* out);

static inline uint8_t bank_count(const uint8_t* bank) {
    return bank[1];
}

// Directory entry of sprite `idx`, or NULL when idx >= count (or bank is NULL: a map whose bank is released).
static inline const SprEntry* bank_spr(const uint8_t* bank, uint8_t idx) {
    return bank && idx < bank[1] ? (const SprEntry*)(bank + BANK_HDR + 16u * idx) : (const SprEntry*)0;
}

// Maps a facing 0..7 (contract 2) to the stored direction index for `ndirs` (1, 4 or 8).
static inline uint8_t bank_dir_index(uint8_t ndirs, uint8_t facing) {
    if(ndirs >= 8) return facing & 7;
    if(ndirs == 4) return (uint8_t)(((facing + 1) >> 1) & 3);
    return 0;
}

// Directions actually stored for a sprite entry (ndirs, or the unfolded part when SPRF_MIRROR is set).
static inline uint8_t bank_stored_dirs(const SprEntry* e) {
    return (e->flags & SPRF_MIRROR) ? (uint8_t)(e->ndirs == 8 ? 5 : 3) : e->ndirs;
}

// Fills *out with the view of frame `frame` (wrapped modulo nframes) of facing `dir`.
// Returns false (and leaves *out untouched) for a bad idx.
bool bank_view(const uint8_t* bank, uint8_t idx, uint8_t dir, uint8_t frame, Blit* out);
