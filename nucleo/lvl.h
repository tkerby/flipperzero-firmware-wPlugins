#pragma once
// The level buffer `lvl` the world / scenes are drawn into (levels 0 white .. 3 black): 2 bits per pixel, 4 pixels per
// byte, row stride LVL_STRIDE (32) bytes, pixel x at bits (x & 3) * 2 of byte x >> 2 -- the SAME packing as the sprite
// data (contract section 2), so blits can move whole source bytes. 2 KB instead of the 8 KB one-byte-per-pixel buffer
// it was before (RAM diet, contract 5). Every helper here is inline: the battle plugin uses them without any import.
// No furi.h (host-compilable).
#include <stdint.h>
#include <string.h>
#include "config.h"

typedef uint8_t (*LvlBuf)[LVL_STRIDE]; // lvl[SCREEN_H][LVL_STRIDE]

// Level of pixel (x, y); the pixel must be on the screen.
static inline uint8_t lvl_get(LvlBuf lvl, int x, int y) {
    return (uint8_t)((lvl[y][x >> 2] >> ((x & 3) * 2)) & 3);
}
// Sets pixel (x, y) to v (0..3); the pixel must be on the screen.
static inline void lvl_put(LvlBuf lvl, int x, int y, uint8_t v) {
    unsigned sh = (unsigned)(x & 3) * 2;
    uint8_t* p = &lvl[y][x >> 2];
    *p = (uint8_t)((*p & ~(3u << sh)) | ((unsigned)(v & 3) << sh));
}
// Clipped lvl_put.
static inline void lvl_putc(LvlBuf lvl, int x, int y, uint8_t v) {
    if((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) lvl_put(lvl, x, y, v);
}
// Clipped: only ever darkens, lvl = max(lvl, v).
static inline void lvl_darken(LvlBuf lvl, int x, int y, uint8_t v) {
    if((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H && lvl_get(lvl, x, y) < v) lvl_put(lvl, x, y, v);
}
// Byte holding level v in all 4 pixels.
static inline uint8_t lvl_pat(uint8_t v) {
    return (uint8_t)((v & 3) * 0x55);
}
// Whole screen = level v.
static inline void lvl_fill(LvlBuf lvl, uint8_t v) {
    memset(lvl, lvl_pat(v), (size_t)SCREEN_H * LVL_STRIDE);
}
// Pixels [x0, x1) of row y (0 <= y < SCREEN_H) = level v, clipped horizontally.
static inline void lvl_span(LvlBuf lvl, int y, int x0, int x1, uint8_t v) {
    if(x0 < 0) x0 = 0;
    if(x1 > SCREEN_W) x1 = SCREEN_W;
    if(x0 >= x1) return;
    uint8_t* row = lvl[y];
    uint8_t pat = lvl_pat(v);
    int b0 = x0 >> 2, b1 = (x1 - 1) >> 2;
    uint8_t m0 = (uint8_t)(0xFFu << ((x0 & 3) * 2));              // pixels >= x0 of the first byte
    uint8_t m1 = (uint8_t)(0xFFu >> ((3 - ((x1 - 1) & 3)) * 2)); // pixels <= x1 - 1 of the last byte
    if(b0 == b1) {
        uint8_t m = (uint8_t)(m0 & m1);
        row[b0] = (uint8_t)((row[b0] & ~m) | (pat & m));
        return;
    }
    row[b0] = (uint8_t)((row[b0] & ~m0) | (pat & m0));
    if(b1 > b0 + 1) memset(row + b0 + 1, pat, (size_t)(b1 - b0 - 1));
    row[b1] = (uint8_t)((row[b1] & ~m1) | (pat & m1));
}
// lvl = 3 - lvl on the whole screen (the hit blink / transition flash).
static inline void lvl_invert(LvlBuf lvl) {
    uint8_t* p = &lvl[0][0];
    for(int i = 0; i < SCREEN_H * LVL_STRIDE; i++) p[i] ^= 0xFF;
}
