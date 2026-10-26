// Tiny HUD pieces shared by the battle plugin and the menu (party) plugin: the 3x5 digit/letter font, the HP bar and the
// status tags. Compiled into BOTH plugins (application.fam), never into the resident app. No furi.h (host-compilable).
#include "hud_gfx.h"
#include "frame.h"
#include "battle_core.h"
#include "text.h"
#include "gen/langs_gen.h"
#include <string.h>

typedef struct {
    char c;
    uint8_t r[5]; // 3 bits per row in bits 7..5; the 4-px 'N' also uses bit 4
} Tiny;
#define R3(a, b, c)    (uint8_t)(((a) << 7) | ((b) << 6) | ((c) << 5))
#define R4(a, b, c, d) (uint8_t)(((a) << 7) | ((b) << 6) | ((c) << 5) | ((d) << 4))
static const Tiny TINY[] = {
    {'0', {R3(1, 1, 1), R3(1, 0, 1), R3(1, 0, 1), R3(1, 0, 1), R3(1, 1, 1)}},
    {'1',
     {R3(1, 1, 0),
      R3(1, 1, 0),
      R3(0, 1, 0),
      R3(0, 1, 0),
      R3(1,
         1,
         1)}}, // 2 px thick top-left serif: a lone 1 next to the HP bar's black end or a "/" read as nothing ("104/106" looked like "04/06", contract 78)
    {'2', {R3(1, 1, 1), R3(0, 0, 1), R3(1, 1, 1), R3(1, 0, 0), R3(1, 1, 1)}},
    {'3',
     {R3(1, 1, 0),
      R3(0, 0, 1),
      R3(0, 1, 0),
      R3(0, 0, 1),
      R3(1, 1, 0)}}, // rounded: the full middle bar made "L3" read as "LE"
    {'4', {R3(1, 0, 1), R3(1, 0, 1), R3(1, 1, 1), R3(0, 0, 1), R3(0, 0, 1)}},
    {'5', {R3(1, 1, 1), R3(1, 0, 0), R3(1, 1, 1), R3(0, 0, 1), R3(1, 1, 1)}},
    {'6', {R3(1, 1, 1), R3(1, 0, 0), R3(1, 1, 1), R3(1, 0, 1), R3(1, 1, 1)}},
    {'7', {R3(1, 1, 1), R3(0, 0, 1), R3(0, 0, 1), R3(0, 1, 0), R3(0, 1, 0)}},
    {'8', {R3(1, 1, 1), R3(1, 0, 1), R3(1, 1, 1), R3(1, 0, 1), R3(1, 1, 1)}},
    {'9', {R3(1, 1, 1), R3(1, 0, 1), R3(1, 1, 1), R3(0, 0, 1), R3(1, 1, 1)}},
    {'/', {R3(0, 0, 1), R3(0, 0, 1), R3(0, 1, 0), R3(1, 0, 0), R3(1, 0, 0)}},
    {'L', {R3(1, 0, 0), R3(1, 0, 0), R3(1, 0, 0), R3(1, 0, 0), R3(1, 1, 1)}},
    {'H', {R3(1, 0, 1), R3(1, 0, 1), R3(1, 1, 1), R3(1, 0, 1), R3(1, 0, 1)}},
    {'P', {R3(1, 1, 1), R3(1, 0, 1), R3(1, 1, 1), R3(1, 0, 0), R3(1, 0, 0)}},
    {'S', {R3(1, 1, 1), R3(1, 0, 0), R3(1, 1, 1), R3(0, 0, 1), R3(1, 1, 1)}},
    {'N',
     {R4(1, 0, 0, 1),
      R4(1, 1, 0, 1),
      R4(1, 0, 1, 1),
      R4(1, 0, 0, 1),
      R4(1, 0, 0, 1)}}, // 4 px: "Nv" (Spanish level prefix), PSN / FNT tags
    {'v', {R3(0, 0, 0), R3(0, 0, 0), R3(1, 0, 1), R3(1, 0, 1), R3(0, 1, 0)}},
    {'B', {R3(1, 1, 0), R3(1, 0, 1), R3(1, 1, 0), R3(1, 0, 1), R3(1, 1, 0)}},
    {'R', {R3(1, 1, 0), R3(1, 0, 1), R3(1, 1, 0), R3(1, 0, 1), R3(1, 0, 1)}},
    {'F', {R3(1, 1, 1), R3(1, 0, 0), R3(1, 1, 0), R3(1, 0, 0), R3(1, 0, 0)}},
    {'Z', {R3(1, 1, 1), R3(0, 0, 1), R3(0, 1, 0), R3(1, 0, 0), R3(1, 1, 1)}},
    {'A', {R3(0, 1, 0), R3(1, 0, 1), R3(1, 1, 1), R3(1, 0, 1), R3(1, 0, 1)}},
    {'T', {R3(1, 1, 1), R3(0, 1, 0), R3(0, 1, 0), R3(0, 1, 0), R3(0, 1, 0)}},
    {'x',
     {R3(0, 0, 0),
      R3(1, 0, 1),
      R3(0, 1, 0),
      R3(1, 0, 1),
      R3(0, 0, 0)}}, // multiplication sign (bag counts)
};

static const Tiny* tiny_find(char c) {
    for(unsigned i = 0; i < sizeof(TINY) / sizeof(TINY[0]); i++)
        if(TINY[i].c == c) return &TINY[i];
    return NULL;
}

static int tiny_adv(
    char c) { // advance = glyph width + 1: 'N' is 4 px wide, everything else (and a character without a glyph) 3
    return c == 'N' ? 5 : 4;
}

int tiny_width(const char* s) {
    int w = 0;
    for(; *s; s++)
        w += tiny_adv(*s);
    return w ? w - 1 : 0;
}

void tiny_draw(const char* s, int x, int y) {
    for(; *s; x += tiny_adv(*s++)) {
        const Tiny* g = tiny_find(*s);
        if(g) ov_bitmap(x, y, tiny_adv(*s) - 1, 5, g->r, 1, 1);
    }
}

void star_draw(int x, int y) {
    static const uint8_t STAR[5] = {0x20, 0xF8, 0x70, 0xD8, 0x88}; // ..X.. XXXXX .XXX. XX.XX X...X
    ov_bitmap(x, y, 5, 5, STAR, 1, 1);
}

void hpbar_draw_h(int x, int y, int w, int h, int hp, int max) {
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
    int in = w - 2;
    int fill = 0;
    if(max > 0 && hp > 0) {
        fill = (hp * in + max - 1) / max;
        if(fill < 1) fill = 1;
        if(fill > in) fill = in;
    }
    ov_fill(x + 1, y + 1, fill, h - 2, 1);
}

int hud_num(char* out, int v) {
    char t[12];
    int n = 0;
    if(v < 0) v = 0;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while(v && n < 10);
    for(int i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    out[n] = 0;
    return n;
}

int hud_level(
    char* out,
    int v) { // "Nv10" in Spanish, "Lv10" elsewhere (the tiny font is ASCII: ja / zh use "Lv")
    out[0] = text_language() == LANG_IDX_ES ? 'N' : 'L';
    out[1] = 'v';
    return 2 + hud_num(out + 2, v);
}

const char* hud_status_tag(uint8_t status, int hp) {
    if(hp <= 0) return "FNT";
    if(status & BS_SLP_MASK) return "SLP";
    if(status & BS_PSN) return "PSN";
    if(status & BS_BRN) return "BRN";
    if(status & BS_FRZ) return "FRZ";
    if(status & BS_PAR) return "PAR";
    return NULL;
}
