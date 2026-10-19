#pragma once
// Contract 68: the legend of the eight Origin Fragments, Chapter 10 (the Ruinas del Origen) and the hidden Gruta del Eclipse. Header-inline only: no symbol, no resident code.
// The story flags of this chapter are D1's reserved block (950 .. 999); the numbers below are the ones of tools/mapc.py BUILTIN_FLAGS (the maps use the names), the plugins use these.
//   951 legend_rumor   a rumor was heard: the Registro shows Genesa as a "???" silhouette (scene_reg.c, PC plugin)
//   952 ruins_open     the sealed gate of the Meseta Cumbre was opened with the eight fragments
//   953 ruins_solved   the mural's order was placed: Genesa appears (also the eighth pedestal's light)
//   954 genesa_gone    the ONE chance at Genesa is spent (set BEFORE the fight: a faint or a flight cannot undo it)
//   955 cave_open      the Gruta del Eclipse's light shaft and stone are shown (the clock window is open and the stone is not in the bag yet; set / cleared by the cave's on_enter script)
//   995 gruta_pit      the pit of the Gruta del Eclipse (the engine sets it when the boulder fills it; 995 is the Planeador's unused slot 11)
//   996 genesa_try    the ONE chance was taken: set BEFORE the battle, read by the ruins' on_enter script (which then sets genesa_gone: a faint or a flight cannot undo it)
//   956 .. 959, 978 .. 980   lit_1 .. lit_7: the pedestals lit so far, in the mural's order (the eighth is ruins_solved)
//   970 .. 977         frag_1 .. frag_8: the eight fragments (1 Lin, 2 Pueblo Cantera, 3 Puerto Dinamo, 4 Ciudad Ambar, 5 the night, 6 the Reserva, 7 the first IR trade, 8 the champion)
//   981 .. 983         clk_on, clk_b0, clk_b1: the DEV / host clock override (NUCLEO_DEBUG or the host only; a release build never reads them): clk_on set = the clock reads PRESET[b1 b0]
#include <stdint.h>
#include <stdbool.h>
#include "world.h"

#define LG_FLAG_RUMOR 951
#define LG_FLAG_FRAG1 970 // frag_k = 970 + k - 1
#define LG_FRAGMENTS 8
#define LG_FLAG_CLK_ON 981
#define LG_FLAG_CLK_B0 982
#define LG_FLAG_CLK_B1 983

enum { CLK_HOUR = 0, CLK_NIGHT, CLK_CAVE }; // the sub-operations of the casino plugin's CAS_LEGEND (casino_host.h): `clock hour|night|cave VAR`
// night = 20:00 .. 05:59; cave = the Gruta del Eclipse's windows 05:00 .. 06:59 (dawn) and 23:00 .. 00:59 (midnight)
static inline bool lg_night(uint8_t h) { return h >= 20 || h < 6; }
static inline bool lg_cave_window(uint8_t h) { return (h >= 5 && h < 7) || h == 23 || h == 0; }
// DEV / host presets (clk_b1 clk_b0): 00 = 05:30 (dawn: night + cave), 01 = 23:30 (midnight: night + cave), 10 = 02:00 (night, cave closed), 11 = 12:00 (day)
static const uint8_t LG_CLK_PRESET[4] = {5, 23, 2, 12};

static inline bool lg_flag(const World* w, unsigned id) { return (w->flags[id >> 3] >> (id & 7)) & 1; }
static inline void lg_flag_set(World* w, unsigned id) { w->flags[id >> 3] = (uint8_t)(w->flags[id >> 3] | (1u << (id & 7))); }
