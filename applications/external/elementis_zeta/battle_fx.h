#pragma once
// Battle effects (contract 10.4): short black-and-white animations drawn over the target creature, played from files/fx.bin.
// Table driven: type / move -> kind, kind -> duration; the frames are a function of the time since the effect started.
// No furi.h (host-compilable).
#include <stdint.h>
#include "blit.h"

enum {
    FX_NONE = 0,
    FX_STAR, // normal: big impact starburst
    FX_STARS3, // fighting / sparkles: three bursts
    FX_FLAME, // fire: rising flames
    FX_DROPS, // water: falling drops + a sweeping wave
    FX_LEAVES, // grass: swirling leaves
    FX_BUBBLES, // poison / bubble: rising bubbles
    FX_BOLT, // electric: zigzag bolts
    FX_RINGS, // psychic / dragon / ice / ghost / freeze / generic (no move of those types exists yet)
    FX_ROCKS, // rock / ground: falling rocks
    FX_SLASH, // claws / flying: diagonal slashes
    FX_SWARM, // bug: stingers
    FX_UP, // stat raised (arrows rising)
    FX_DOWN, // stat lowered (hollow arrows sinking)
    FX_ZZZ, // sleep
    FX_ORBIT, // confusion
    FX_WAVE, // wavewall: a wall of water sweeping across
    FX_FANG, // bite / chomp: jaws snapping shut
    FX_SPORE, // spore / pollen / dust: puffs
    FX_WHIFF, // a miss: the puff where the attack fizzled
    FX_COUNT
};

// Move styles (contract 10.4): per move id, low 5 bits = the effect played over the target (0: a status move without a
// cast effect, the user only hops), plus flags.
#define MST_KIND    0x1F
#define MST_PROJ    0x20 // a projectile flies from the user to the target first
#define MST_CONTACT 0x40 // the user lunges at the target (else a short step)
#define MST_BIG     0x80 // bigger / heavier version (longer reach, more shake)
uint8_t fx_move_style(uint8_t move, uint8_t type);

// Effect of a move of type `type` (0..14 of types.toml; anything else = the generic starburst).
uint8_t fx_for_type(uint8_t type);
// Effect of a status condition SK_* (sleep, poison, burn, freeze, paralysis, confusion).
uint8_t fx_for_status(uint8_t kind);
uint16_t fx_duration(uint8_t kind); // ms
// ---- the player.  The artwork of every effect is baked into files/fx.bin (tools/fxbake, format in fx_tab.h); one FxPlayer streams the
// frames of the effect being played: it loads (key table + a window of frames, at most ~1.4 KB of heap) when a draw call asks for an
// effect it does not hold, refills the window from the card when the time leaves it (no handle is kept open between reads) and
// frees everything when a whole frame passes without a draw call (fx_frame_end) or at fx_release().  A missing / short fx.bin draws nothing.
typedef struct {
    uint8_t* blk; // key table (nk x { u16 t_ms, u16 off }) then the window of frame data
    uint32_t base; // file offset of the frame data
    uint16_t
        id; // directory index + 1 of the animation loaded (or tried: blk NULL = it could not be loaded)
    uint16_t total; // bytes of frame data
    uint16_t win; // window size
    uint8_t nk, cur; // keys, the key shown last
    uint8_t wf0, wf1; // the window holds the frames [wf0, wf1)
    uint8_t live; // drawn since the last fx_frame_end
} FxPlayer;
// Draws the effect `t` ms after its start over the target's screen box [x0, x1) x [y0, y1). `dir` = +1 when the user stands
// to the left of the target (things travel to the right), -1 otherwise; `big`: the heavy version.
void fx_draw(
    FxPlayer* p,
    LvlBuf lvl,
    uint8_t kind,
    uint16_t t,
    int x0,
    int y0,
    int x1,
    int y1,
    int dir,
    int big);
#define FX_PROJ_MS \
    300 // a projectile's flight (the sprites in fx.bin cover this long; the scene's AT_PROJ_MS)
// A projectile of effect `kind` `t` ms into its `ms` ms flight from (ax, ay) to (bx, by).
void fx_proj(
    FxPlayer* p,
    LvlBuf lvl,
    uint8_t kind,
    uint16_t t,
    uint16_t ms,
    int ax,
    int ay,
    int bx,
    int by);
// Call once per drawn frame, after the draw calls: frees the animation when this frame drew nothing of it.
void fx_frame_end(FxPlayer* p);
void fx_release(FxPlayer* p);

// ---- the capture crystal (contract 14.4): the lab's upright hexagonal crystal (tools/gen_tiles.py crystal_actor) scaled down to
// 7 x 11 px, drawn in code. `cx` = centre column, `by` = bottom row; `tilt` shears the top by that many px to the right
// (the wobble); `dim` != 0 darkens it (caught). Light left facet, mid centre line, shaded right facet, black outline.
#define CRYSTAL_W 7
#define CRYSTAL_H 11
void fx_crystal(LvlBuf lvl, int cx, int by, int tilt, int dim);
