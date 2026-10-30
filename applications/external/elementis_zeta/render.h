#pragma once
// World renderer (contract 5): ground, then depth-sorted objects / NPCs / hero into lvl.
// No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "blit.h"
#include "map.h"

// Hero sprite (gen/hero.h): 18x35, 8 dirs x 5 frames (0 = idle, 1..4 = walk), masked.
// Anchor: pixel (HERO_AX, HERO_AY) lands on tile centre + (0, ACTOR_FOOT_DY). The sprite's lowest
// opaque row is 30 (idle S) .. 34 (walk), typically 32, so with HERO_AY 31 and ACTOR_FOOT_DY 1 the
// feet land 0 (idle S) .. +4 px, typically +2 px, below the tile centre (shoes centred on the tile).
#define HERO_AX 12
#define HERO_AY 31

// Fills *out with the hero view of facing `dir` (0..7) and `frame` (0..4). Frames wrap.
// The pixels live in a one-facing cache loaded from files/hero.bin: the pointers in *out stay valid until hero_view() is
// called with a facing that stores different data (dir folded: S SE E NE N). Callers use the view at once (blit) or keep
// asking for the same facing (the battle's back view). A turn costs one card read the first time it is drawn.
void hero_view(uint8_t dir, uint8_t frame, Blit* out);
#ifndef NUCLEO_DEVICE
void render_hero_reset(
    void); // host tests: forgets the probed hero files (hero_bike.bin is probed once per run)
#endif
// Card reads of hero.bin so far (debug overlay / host tests): count, ms of the last and of the slowest one.
typedef struct {
    uint32_t loads, last_ms, max_ms;
} HeroLoadStats;
const HeroLoadStats* hero_load_stats(void);
// Frees what the renderer keeps in heap blocks (MEM3): the streamed NPC frames (map.h map_stream_release: read again when drawn) and the hero's cached facing (1.4 KB, allocated on the first draw; the next hero_view() reads it
// again: one card read) and the draw list (864 B, allocated by the next render_world() with a bank). battle_open.c calls it before the battle plugin
// loads and when the battle is over. If the heap has no room for a block, hero_view() gives an empty 0 x 0 view and render_world() draws the ground
// only (RenderStats.overflow 255); both retry at the next frame.
void render_cache_release(void);
#ifndef NUCLEO_DEVICE
uint32_t render_cache_bytes(void); // host tests: bytes of those two blocks that are allocated now
#endif

// Animated tiles / objects (contract 22, map flag MAPF_ANIM): a ground tile or object whose bank entry has nframes > 1 is drawn with frame
// `step % nframes`. The world sets render_anim_step (clock / ANIM_MS) before each render_world(); 0 by default (the host tests that call render_world themselves).
extern uint32_t render_anim_step;

// Camera (in pixels) for a hero at Q8 tile position (fi, fj): centre of tile (i,j) = i*256, j*256.
void render_camera(int32_t fi, int32_t fj, int* camx, int* camy);

typedef struct {
    bool draw; // false = hero not drawn (title / battle)
    uint8_t dir, frame; // facing 0..7, walk frame 0..4
    int32_t fi, fj; // Q8 tile position (screen anchor = tile centre + ACTOR_FOOT_DY)
    uint8_t i0, j0, i1,
        j1; // occupied tile box for depth sorting (the tile the hero is nearer to while stepping)
    int8_t
        lift; // ledge hop: pixels the sprite is raised above its ground position (the shadow stays on the ground)
    uint8_t
        rustle_ph; // shaking grass: 0 = none, else the animation phase 1..RUSTLE_PHASES of tile (rustle_i, rustle_j)
    uint8_t rustle_i, rustle_j;
    uint8_t
        balloon; // M4-C: NPC index + 1 that shows the "!" balloon (MAPF_BALLOON maps: the last bank sprite), 0 none
    uint8_t
        bike; // the hero's sprite set (C4-A contract 35.4, C5 contract 42.1): 0 walking (hero.bin), 1 riding the bicycle (hero_bike.bin), 2 surfing (hero_surf.bin); a missing file = the walking sprites, bobbing 1 px
    uint8_t
        fog; // C4b (contract 40.5): 0 = no fog; else 1 + the drift step 0..31 of the mist (MAPF_FOG maps; drawn after the dark pass)
    uint8_t
        dark; // C4-A (contract 35.1): 0 = draw everything; else everything outside the diamond of half-height `dark` px (half-width 2 x dark) around the hero is black
} RenderHero;
#define RUSTLE_PHASES 4

// Per-NPC runtime state (NpcState, map.h): position (Q8), facing, walk frame, visibility.
// NULL => static NPCs at their map tiles, facing from the map, all visible.

typedef struct {
    uint16_t tiles; // ground tiles blitted
    uint16_t fills; // of which fill fast path
    uint8_t drawables; // sorted this frame
    uint8_t overflow; // drawables dropped (more than RENDER_MAX_DRAW visible)
    uint16_t objs_seen; // objects that passed culling
} RenderStats;

#define RENDER_MAX_DRAW \
    48 // 18 B each in .bss; at most 22 are ever visible (every hero tile of every map, tools/host_world stats)

// Depth cues: ground shadows (render.c). Only darken: lvl = max(lvl, shadow level).
#define SHADOW_ACTOR_LV 2 // hero / NPC drop shadow (14 x 5 ellipse under the feet)
#define SHADOW_K_LOW_J  2 // contact band thickness (rows) for objects lower than 30 px: +j edge
#define SHADOW_K_LOW_I  3 //   ... +i edge
#define SHADOW_K_TALL_J 3 // objects 30 px or taller (walls, shelves, houses)
#define SHADOW_K_TALL_I 4
#define SHADOW_PAD      4 // rows below an object sprite that its shadow may reach (cull margin)
// OBJECT contact shadow = the catalog `fill` of the object (bank directory byte; none = 0xFF):
// 1, 2 = band level along the two front edges, 3 = blob under a 1x1 object (trees, rocks).

// Renders the whole world view into lvl (fills with the map bg_level first). `hero` may be NULL.
void render_world(
    LvlBuf lvl,
    const Map* m,
    int camx,
    int camy,
    const RenderHero* hero,
    const NpcState* npcs);

const RenderStats* render_stats(void);

// The screen-anchored pixels of the last render_world (config.h PIN_*): the hero's sprite, his shadow and the grass over his feet.
// Pass it to frame_set_pins(): while the camera follows the hero these stand still on the screen, so their dither phase must not
// scroll with the world (frame.h, world-anchored dither). Valid until the next render_world.
const uint8_t* render_pin(void);

// Tall grass over the feet of an actor standing on a C_GRASS tile.
// Test / debug switch: false draws the actors without it (the host harness compares the two renders).
void render_set_grass_overlay(bool on);

// Test / debug switch: false skips all ground shadows (host harness compares pure draw order).
void render_set_shadows(bool on);
