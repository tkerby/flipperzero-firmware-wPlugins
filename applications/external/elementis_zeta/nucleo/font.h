#pragma once
// Bitmap fonts drawn into the frame overlay. No furi.h (host-compilable).
//
// Two fonts:
//  * the LANGUAGE font (font.nfnt, one per language, contract section 4): codes are u16 `tchar`s;
//    font_load() picks the active one. Small fonts live in RAM; a big one (hanzi) is read from the
//    SD card into an LRU glyph cache -- call font_prefetch() for the codes about to be drawn
//    (text_prefetch_page() does it for a dialog page); drawing never reads the card.
//  * the SYSTEM font (gen/sysfont.h, built in, 5x10 ASCII + the UI glyphs): the "NUCLEO" logo and
//    the debug overlays. It also stands in when a language font cannot be loaded.
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef uint16_t tchar; // text code (contract 4): 0x20-0x7E ASCII, 0x7F.. UI glyphs, 0x90+ per language

enum { GLYPH_MORE = 0x7F, GLYPH_CURSOR = 0x80, GLYPH_ELLIPSIS = 0x81 }; // v > ... in every font

// ---- language font
// Loads lang/<dir>/font.nfnt (replacing the previous font). Returns false (and keeps drawing with
// the system font) if the file is missing or invalid.
bool font_load(const char* lang_dir);
void font_unload(void);
bool font_loaded(void);
uint32_t font_charset_id(void); // charset id of the loaded font (0 = none / system font)
int font_pitch(void);           // line pitch in px
int font_cell_w(void);
int font_cell_h(void);
int font_baseline(void);
int font_adv(tchar c);          // advance in px
// Makes the glyphs of codes[0..n) resident (SD-backed fonts only; no-op for RAM fonts). n < 0:
// up to the first 0x0000 code. Control codes (< 0x20) are ignored.
void font_prefetch(const tchar* codes, int n);
// Same for `count` END-terminated strings with a single open of the font file (a menu's labels).
void font_prefetch_many(const tchar* const* strs, int count);
// Draws one glyph with its cell top-left at (x, y). An absent or non-resident glyph (and an empty
// glyph other than space) draws a hollow box. color: 1 black, 0 white; clear bits are transparent.
void font_draw_glyph(tchar c, int x, int y, uint8_t color);
// END (0x0000) terminated string, no line breaks: black.
void font_draw_text(const tchar* s, int x, int y);
int font_text_width(const tchar* s); // px, up to END

// ---- system font (strings are NUL-terminated codes, 0x20-0x81, one byte each)
void sysfont_draw_glyph(uint8_t code, int x, int y, uint8_t color, int scale); // scale 1 or 2
void sysfont_draw_str_c(const uint8_t* s, int x, int y, uint8_t color);        // 0x0A = next line
void sysfont_draw_str(const uint8_t* s, int x, int y);                         // black
void sysfont_draw_str2x(const uint8_t* s, int x, int y);                       // black, 2x
int sysfont_width(const uint8_t* s);                                           // longest line, px
// Copies an ASCII C string into codes (NUL terminated, truncated to cap-1); returns the length.
int sysfont_from_ascii(const char* ascii, uint8_t* out, int cap);

// Legacy names of the system-font API (debug overlays written before the language fonts).
#define FONT_ADV 6
#define font_draw_str_c sysfont_draw_str_c
#define font_draw_str sysfont_draw_str
#define font_draw_str2x sysfont_draw_str2x
#define font_width sysfont_width
#define font_from_ascii sysfont_from_ascii
