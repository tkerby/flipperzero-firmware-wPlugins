#pragma once
// Text tables (lang/<xx>/txt_NN.bin + txt_gl.bin, NTXT v2, contract 3.2), the active language and
// the text-box geometry. No furi.h; file access only via assets.h.
#include <stdint.h>
#include <stdbool.h>
#include "font.h" // tchar

enum {
    TX_END = 0x00,
    TX_PLAYER = 0x01,
    TX_RIVAL = 0x02,
    TX_VAR0 = 0x03, // {v0}..{v3}: runtime variables (text_set_var), TX_VAR0 + n
    TX_VAR1 = 0x04,
    TX_VAR2 = 0x05,
    TX_VAR3 = 0x06,
    TX_NL = 0x0A,
    TX_PAGE = 0x0C
};
#define TEXT_VARS 4
#define TEXT_VAR_LEN \
    16 // codes of one variable / name, including the END code (longer ones are cut)
#define TEXT_GLOBAL 0x8000

// ---- language (indices of gen/langs_gen.h: LANG_COUNT, LANG_IDX_ES, LANG_DIR[], LANG_NAME[])
// Makes language `idx` active: loads its font (lang/<dir>/font.nfnt) and points the text tables at
// lang/<dir>/. Returns false if idx is out of range or the font could not be loaded (text then
// falls back to the system font). Until this is called the first text_load()/tb_geom() activates
// language 0. After a switch every loaded string and measured width is stale: reload them (the
// scenes load their labels in enter(); a dialog opened afterwards loads its own text).
bool text_set_language(uint8_t idx);
uint8_t text_language(void);
// Called at the end of every text_set_language() (also when it fails): lets game state that holds text in the old
// language (the chosen {player} / {rival} names) reload it. NULL = none.
extern void (*text_lang_hook)(void);
// Frees the font's RAM (1.4 KB Latin, 3 KB kana, the hanzi cache) but KEEPS the active language, and text_font_restore() reads it
// back (the same font file, so every loaded string stays valid; the hanzi glyph cache starts cold). battle_open.c frees it while
// the battle plugin loads: the plugin load is the heap's low point and nothing is drawn or measured in between. Restore returns
// false when the font could not be loaded (text then falls back to the system font, as after text_set_language()).
void text_font_release(void);
bool text_font_restore(void);
// Releases the font and its caches and forgets the active language. Call once when the game thread
// ends (together with assets_shutdown()): the process-wide state must not outlive the app.
void text_shutdown(void);

// Typewriter speed: ms per glyph of the active language (gen/langs_gen.h LANG_TYPE_MS) scaled by the
// player's text speed (0 slow, 1 normal, 2 fast, 3 instant = 0 ms; settings.h TEXT_SPEED_*).
void text_set_speed(uint8_t speed);
uint16_t text_glyph_ms(void);

// Sets which txt_NN.bin ordinary (non-global) ids refer to.
void text_set_map(uint8_t map_id);
// Names substituted for TX_PLAYER / TX_RIVAL when drawing (END-terminated codes, COPIED, cut at
// TEXT_VAR_LEN-1; NULL = empty). The codes must belong to the active language's charset (the
// preset names come from the gl.name.* texts, so they do); a language switch invalidates them
// (reload them from the tables). Their glyphs are prefetched together with the text that uses them.
void text_set_names(const tchar* player, const tchar* rival);
// Variables {v0}..{v3} (TX_VAR0 + n): END-terminated codes (copied; NULL = empty), or a decimal
// number in ASCII digits (any font has them; negative gets a leading '-'). Convention: v0 creature
// name, v1 move / stat / type name, v2 number, v3 spare. Typically the codes come from
// text_load(TEXT_GLOBAL | id, ...) of a gl.sp.* / gl.mv.* / gl.bt.st.* entry, or text_set_var_id().
// Set them BEFORE the dialog that uses them is opened (the dialog prefetches / measures on open).
void text_set_var(uint8_t n, const tchar* s);
void text_set_var_num(uint8_t n, int value);
void text_set_var_rival(
    uint8_t
        n); // variable n = a copy of the current {rival} name (a trainer battle names the rival through {v3})
// Loads string `id` (bit 0x8000 = global) straight into variable n (first line only). Returns false
// (variable emptied) on error.
bool text_set_var_id(uint8_t n, uint16_t id);
void text_clear_vars(void); // empties v0..v3 (the names stay)

// Loads string `id` (bit 0x8000 = global) into buf as codes, END terminated, truncated to cap-1
// codes. Returns the string length (without END) or -1 on error / unknown id / a table whose
// language or charset does not match the active font.
int text_load(uint16_t id, tchar* buf, int cap);

// Paging over a loaded string. A "page" ends at TX_PAGE or TX_END.
// Returns the index of the next page's first code, or -1 if `pos`'s page is the last one.
int text_next_page(const tchar* s, int pos);
// Index of line `n` (0-based) of the page starting at pos, or -1 if the page has fewer lines.
int text_line_start(const tchar* s, int pos, int n);
// Number of codes in the line starting at pos (up to TX_NL / TX_PAGE / TX_END).
int text_line_len(const tchar* s, int pos);
// Glyph count of a line after expanding player/rival names.
int text_line_glyphs(const tchar* s, int pos);
// Width in px of a line after expanding names / variables (sum of font_adv()).
int text_line_px(const tchar* s, int pos);
// Draws up to max_glyphs glyphs of the line at (x, y) in black (typewriter support); each glyph
// advances by font_adv(). Returns the number of glyphs drawn.
int text_draw_line(const tchar* s, int pos, int x, int y, int max_glyphs);

// Asks the font to keep the glyphs of the page starting at `pos` (or of a whole END-terminated
// string) resident. Call when a page / label is set, never from the draw callback.
void text_prefetch_page(const tchar* s, int pos);
void text_prefetch(const tchar* s);
// `count` END-terminated strings, one open of the SD font in total (use for a screen's several labels).
void text_prefetch_many(const tchar* const* strs, int count);

#ifndef NUCLEO_DEVICE
// Host tests: the line at `pos` of a loaded string with every {player} / {rival} / {vN} replaced by its current text, as codes (END terminated,
// at most cap-1); returns the length.
int text_expand(const tchar* s, int pos, tchar* out, int cap);
#endif

// ---- text box geometry, derived from the active font (see config.h TB_*)
typedef struct {
    int16_t box_y, box_h; // frame: x = TB_X, w = TB_W, bottom-aligned
    int16_t text_x; // left edge of the text
    int16_t line_y[TB_LINES]; // top of each line's glyph cell
    int16_t max_px; // widest allowed line in px (TB_W - 2 * text_x)
    int16_t more_x,
        more_y; // top-left of the "more" arrow glyph's cell (GLYPH_MORE): the cell ends
    // TB_MORE_RIGHT px from the right edge, its top is TB_MORE_MARGIN above the box bottom
} TbGeom;
const TbGeom* tb_geom(void);
