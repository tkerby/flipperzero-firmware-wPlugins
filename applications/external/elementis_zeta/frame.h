#pragma once
// Frame buffers + temporal-grayscale compose + 1-bit overlay. No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "lvl.h"

typedef struct {
    uint8_t lvl[SCREEN_H][LVL_STRIDE] __attribute__((
        aligned(4))); // game thread only; 2 bits per pixel (lvl.h): 0 white .. 3 black
    // Two sets of CYCLE display PAGE-layout planes (bit set = black), each its own 3 KB heap block so the idle one can be freed
    // while the battle plugin loads (frame_idle_release). sets[i][s] is plane s of set i, as when this was a 3-D array.
    uint8_t (*sets[2])[PLANE_BYTES];
    volatile uint8_t front; // set shown by the flip loop
    volatile int8_t pending; // set waiting to be adopted, or -1
    uint8_t back; // set being composed (game thread)
    uint8_t
        dither_off; // 0..2: phase offset of the temporal dither (frame_set_dither), game thread only
    const uint8_t* pin; // NULL, or the PIN_W x PIN_H screen-anchored pixel mask (frame_set_pins)
} Frame;

// Plane layout = the display's native u8g2 page layout: byte (y >> 3) * SCREEN_W + x,
// bit (y & 7) (LSB = top row of the page). Bit set = black pixel.
static inline int plane_px(const uint8_t* plane, int x, int y) {
    return (plane[(y >> 3) * SCREEN_W + x] >> (y & 7)) & 1;
}

// Zeroes *f and allocates the two sets (the previous contents of *f are NOT freed: call it on fresh memory, once). False when
// the heap cannot hold them (*f is then unusable: frame_deinit and quit).
bool frame_init(Frame* f);
void frame_deinit(Frame* f); // frees the sets (not *f itself)
// Heap relief around the battle plugin load (contract 10.7): frees the set that is neither shown nor pending (3 KB). Needs
// frame_ready() (the caller waits with game_wait_ready first) and a game thread that composes / publishes nothing until
// frame_idle_restore() has brought the set back. Release is idempotent and answers whether the set is gone; restore false =
// no heap for it (the caller must not compose again).
bool frame_idle_release(Frame* f);
bool frame_idle_restore(Frame* f);
void frame_clear(Frame* f, uint8_t level); // fill lvl with a level

// True when a set is free to compose into (nothing pending adoption).
static inline bool frame_ready(const Frame* f) {
    return f->pending < 0;
}

// World-anchored dither (contract 2): the gray phase of pixel (x, y) is (x + y + dither_off) % 3. The world scene sets it from the
// camera it rendered with (camx, camy = screen position of world point (0, 0) negated, i.e. screen x shows world x + camx), so a
// world point keeps its phase -- and its on/off sequence over the 3 sub-frames -- while the view scrolls. Everything that does
// not scroll (title, menus, battle) leaves it 0. frame_clear / frame_dither_reset bring it back to 0.
void frame_set_dither(Frame* f, int camx, int camy);
// Pixels set in `pin` (config.h PIN_*; render_pin()) keep the screen-anchored phase (x + y) % 3 whatever the offset is: the
// hero, who stands still on the screen while the world scrolls. The mask must stay valid until the next frame_set_pins / reset.
void frame_set_pins(Frame* f, const uint8_t* pin);
void frame_dither_reset(Frame* f); // offset 0, no pins

// Build the free set from lvl with fade (0..3, lut[l] = max(0, l - fade)).
// Must only be called when frame_ready(). Selects the overlay target.
void frame_compose(Frame* f, uint8_t fade);
// Select the free set as overlay target without composing (e.g. after custom edits).
void frame_overlay_begin(Frame* f);
// Hand the composed set to the flip loop (adopted when it reaches sub-frame 0).
void frame_publish(Frame* f);
// Called by the flip loop after finishing sub-frame 2 (s wrapped to 0).
// Returns true when a pending set was adopted (the game thread may now compose again).
bool frame_adopt(Frame* f);

// Overlay primitives: constant 1-bit pixels into all 3 planes of the set being composed.
// color: 1 = black, 0 = white. Out-of-screen parts are clipped.
void ov_px(int x, int y, uint8_t color);
void ov_fill(int x, int y, int w, int h, uint8_t color);
void ov_frame(int x, int y, int w, int h, uint8_t color); // 1 px outline
// Draws a 1-bpp bitmap (glyph): `rows` = h rows of `bpr` bytes, the left pixel in bit 7 of a
// row's first byte; SET bits are painted with `color`, clear bits are left untouched (transparent).
// Clipped to the screen.
void ov_bitmap(int x, int y, int w, int h, const uint8_t* rows, int bpr, uint8_t color);
