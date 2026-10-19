#pragma once
// Tunables for Nucleo (M1 contract). No includes: usable from host tools.

// Screen / temporal grayscale
#define SCREEN_W 128
#define SCREEN_H 64
#define ROWB (SCREEN_W / 8)
#define PAGES (SCREEN_H / 8)
#define PLANE_BYTES (SCREEN_W * PAGES) // 1024
#define CYCLE 3
#define LVL_STRIDE (SCREEN_W / 4) // bytes per row of the level buffer: 2 bits per pixel, 4 pixels per byte (lvl.h)
// Screen-anchored pixels of the world view (the hero, his shadow and the grass band over his feet stay still on the screen while the
// world scrolls, so their dither phase must not follow the world): a 1 bpp mask of PIN_W x PIN_H pixels at (PIN_X0, 0) in the
// display's page layout (byte (y >> 3) * PIN_W + (x - PIN_X0), bit y & 7). render.c builds it, frame_compose reads it.
#define PIN_X0 40
#define PIN_W 48
#define PIN_H 40
#define PIN_PAGES (PIN_H / 8)
// Flip loop period (ms, 1 tick = 1 ms). ST7567 scans at ~70-80 Hz, so faster is pointless.
#define FLIP_PERIOD_MS 13

// Iso geometry
#define TW 28
#define TH 14
#define ACTOR_FOOT_DY 1 // actor anchor = tile centre + (0, this): feet stand on the tile centre

// Movement (time-based, grid)
#define STEP_MS_S_N 240 // facings 0 / 4
#define STEP_MS_ODD 267 // facings 1 3 5 7
#define STEP_MS_E_W 480 // facings 2 / 6
#define SETTLE_MS 60        // two keys held (diagonal): stable time before turning / walking
#define SETTLE_SINGLE_MS 90 // one key held: waits longer for a possible second key of a diagonal
#define TURN_MS 100
#define WCLOCK_MAX_DT 100
#define FOG_SHIFT 8         // MAPF_FOG maps (`@fog yes`, contract 40.5): the mist drifts one pixel and the world redraws every FOG_MS = 1 << FOG_SHIFT = 256 ms (a shift, not a division: ~30 B of code)
#define FOG_MS (1u << FOG_SHIFT)
#define ANIM_MS 400         // animated tiles / objects (bank nframes > 1, map flag MAPF_ANIM): frame = (clock / ANIM_MS) % nframes (contract 22)
#define SLIDE_ASSIST 1
#define STRICT_CORNERS 1 // block a diagonal step when EITHER flank tile is solid
#define WALK_PX_PER_FRAME 7

// Fades
#define FADE_STEPS 3
#define FADE_STEP_MS 70

// Text box (overlay). Vertical geometry comes from the active font (tb_geom() in text.h):
// box_h = TB_LINES * pitch + 2 * TB_PAD_Y, bottom-aligned; line k at box_y + TB_PAD_Y + k * pitch.
#define TB_X 0
#define TB_W 128
#define TB_TEXT_X 4    // left margin of the text; the same margin is kept on the right
#define TB_PAD_Y 2
#define TB_LINES 2
#define TB_MORE_MARGIN 10 // "more" arrow cell top: y = box bottom - 10
#define TB_MORE_RIGHT 5   // "more" arrow cell right edge: x = TB_W - 5 - cell_w (tools/textc.py MORE_RIGHT_X = 123)
#define TB_BUF 200        // codes of one loaded text entry (including the END code)

// Fonts (font.c): a language font whose glyph data is <= FONT_RAM_MAX bytes is loaded fully into
// RAM; a bigger one (hanzi) streams glyphs from the SD card into an LRU cache of
// FONT_CACHE_GLYPHS entries (font_prefetch(); drawing never touches the SD card).
#define FONT_RAM_MAX 4096 // Latin ~1.4 KB and kana ~3 KB stay in RAM; the hanzi font (6.9 KB and growing) streams
#define FONT_CACHE_GLYPHS 64 // >= a battle screen: dialog page (20 hanzi + names) + 3 pinned UI glyphs + menu labels + creature names
#define FONT_MAX_BPG 32 // bytes per glyph: 16 x 16 cells at most

// Threads / queues. Stacks come out of the heap (contract 5, Memory), so they are sized to the measured high-water mark + a margin:
// the game thread peaks at ~3.84 KB on the plugin load (device: overlay S = 2300 of 6144 right after it; the loader's ELF code is
// ~3.5 KB of that, our own frames to plugin_load are 0.35 KB: tools/stack_depth.py) -> 4.75 KB = 1 KB margin. The main (flip loop)
// thread holds only 72 B of our frames + canvas_commit / the exit-time log in the firmware: 3 KB (keep in step with
// application.fam stack_size). Read the real headroom on the overlay line 4 (S game, T main) and in the serial log.
#define GAME_STACK (4864)
#define MAIN_STACK (3 * 1024)
#define INPUT_QUEUE_LEN 16
#define MAX_SCENES 6 // title/world + dialog + choice + fade + one spare (battle inserted below a fade)

// Scripts (script.c, contract section 8)
#define SCRIPT_VARS 16       // u8 variables per game; var 0 = result of ASK (1 yes, 0 no)
#define SCRIPT_STACK 4       // CALL nesting depth
#define SCRIPT_BUDGET 256    // instructions per update before a runaway script is aborted

// Settings file (separate from the future save.bin): see settings.h
#ifndef SETTINGS_PATH // the host harness overrides it with -DSETTINGS_PATH=...
#define SETTINGS_PATH "/ext/apps_data/elementis_zeta/settings.bin"
#endif
