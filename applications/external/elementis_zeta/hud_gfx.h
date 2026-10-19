#pragma once
// HUD drawing pieces shared by the battle plugin, the party / bag / shop screens (menu plugin) and the PC plugin: the 3x5 font of the battle plates,
// the HP bar and the language-neutral status tags. Overlay drawing (black / white 1-bit pixels). No furi.h.
#include <stdint.h>
#include <stdbool.h>

// 3x5 glyphs (N is 4 wide): digits / L H P S N v B R F Z A T x. Advance = glyph width + 1 (4 for the 3-px glyphs).
int tiny_width(const char* s);
void tiny_draw(const char* s, int x, int y); // black, top-left (x, y)
// HP bar: 1-px frame, w x h (h >= 3); fill = hp / max over the inside (at least 1 px while hp > 0), black.
void hpbar_draw_h(int x, int y, int w, int h, int hp, int max);
static inline void
    hpbar_draw(int x, int y, int w, int hp, int max) { // the party / bag / shop 5-px bar
    hpbar_draw_h(x, y, w, 5, hp, max);
}
// Writes v (>= 0) as ASCII digits at out (NUL terminated); returns the length. out needs 8 bytes for v <= 9999999.
int hud_num(char* out, int v);
// The level tag "Nv<v>" (es) / "Lv<v>" (every other language) for the tiny font; out needs 12 bytes. Returns the length.
int hud_level(char* out, int v);
// status tag of a Mon.status byte: "SLP" "PSN" "BRN" "FRZ" "PAR", NULL when healthy. `hp == 0` gives "FNT".
const char* hud_status_tag(uint8_t status, int hp);
// Contract 65: the variocolor mark, a 5 x 5 black star with its top-left at (x, y) (party list / summary, Registro). Code-drawn: language and font independent.
void star_draw(int x, int y);
