#include "font.h"
#include "assets.h"
#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include("gen/sysfont.h")
#include "gen/sysfont.h"
#else
#include "font_fallback.h"
#endif

// ---------------------------------------------------------------------------- system font
// The glyph rows are packed (gen/sysfont.h, contract 27): unpack one glyph into SYSFONT_H bytes, bit 7 = leftmost column. False for a code the font lacks.
static bool sys_glyph(uint8_t code, uint8_t* rows) {
    if(code < SYSFONT_FIRST || code > SYSFONT_LAST) return false;
    const uint8_t* g = sysfont_bits + (code - SYSFONT_FIRST) * SYSFONT_BYTES;
    for(int r = 0; r < SYSFONT_H; r++) {
        int b = r * SYSFONT_W;
        uint16_t v = (uint16_t)(g[b >> 3] | (g[(b >> 3) + 1] << 8));
        rows[r] = (uint8_t)(((v >> (b & 7)) & ((1 << SYSFONT_W) - 1)) << (8 - SYSFONT_W));
    }
    return true;
}

static bool all_zero(const uint8_t* g, int n) {
    for(int i = 0; i < n; i++)
        if(g[i]) return false;
    return true;
}

void sysfont_draw_glyph(uint8_t code, int x, int y, uint8_t color, int scale) {
    if(scale < 1) scale = 1;
    uint8_t g[SYSFONT_H];
    if(!sys_glyph(code, g) || (all_zero(g, SYSFONT_H) && code != ' ')) { // unknown: hollow box
        ov_frame(x, y + 2 * scale, SYSFONT_W * scale, 6 * scale, color);
        return;
    }
    if(scale == 1) {
        ov_bitmap(x, y, SYSFONT_W, SYSFONT_H, g, 1, color);
        return;
    }
    for(int r = 0; r < SYSFONT_H; r++)
        for(int c = 0; c < SYSFONT_W; c++)
            if(g[r] & (0x80 >> c)) ov_fill(x + c * scale, y + r * scale, scale, scale, color);
}

static void sys_draw_str(const uint8_t* s, int x, int y, uint8_t color, int scale) {
    int cx = x;
    for(; *s; s++) {
        if(*s == 0x0A) {
            cx = x;
            y += SYSFONT_H * scale;
            continue;
        }
        sysfont_draw_glyph(*s, cx, y, color, scale);
        cx += SYSFONT_ADV * scale;
    }
}

void sysfont_draw_str_c(const uint8_t* s, int x, int y, uint8_t color) {
    sys_draw_str(s, x, y, color, 1);
}

void sysfont_draw_str(const uint8_t* s, int x, int y) {
    sys_draw_str(s, x, y, 1, 1);
}

void sysfont_draw_str2x(const uint8_t* s, int x, int y) {
    sys_draw_str(s, x, y, 1, 2);
}

int sysfont_width(const uint8_t* s) {
    int best = 0, cur = 0;
    for(; *s; s++) {
        if(*s == 0x0A) {
            cur = 0;
            continue;
        }
        cur += SYSFONT_ADV;
        if(cur > best) best = cur;
    }
    return best;
}

int sysfont_from_ascii(const char* ascii, uint8_t* out, int cap) {
    int n = 0;
    while(ascii[n] && n < cap - 1) {
        out[n] = (uint8_t)ascii[n];
        n++;
    }
    if(cap > 0) out[n] = 0;
    return n;
}

// ---------------------------------------------------------------------------- language font
typedef struct {
    uint32_t stamp; // last use; 0 = empty slot
    tchar code;
} CacheTag;

static struct {
    bool loaded;
    uint8_t flags, cell_w, cell_h, adv_n, adv_w, pitch, baseline, bpr, bpg;
    uint16_t n, first, narrow_below;
    uint32_t charset, off_glyphs;
    uint8_t* adv;   // per-glyph advances (flags bit 0) or NULL
    uint8_t* ram;   // n * bpg glyph bytes (RAM font) or NULL
    bool sd;         // glyphs come from the SD card through the cache (file opened per font_prefetch only)
    char path[40];   // lang/<dir>/font.nfnt (SD-backed font)
    CacheTag* tags; // FONT_CACHE_GLYPHS tags + glyph data (SD-backed font)
    uint8_t* cdata;
    uint32_t tick;
} F;

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static __attribute__((always_inline)) inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void font_unload(void) {
    free(F.adv);
    free(F.ram);
    free(F.tags);
    free(F.cdata);
    memset(&F, 0, sizeof(F));
}

static void prefetch_from(AssetFile* f, const tchar* codes, int n);

bool font_load(const char* lang_dir) {
    font_unload();
    char path[40];
    if(snprintf(path, sizeof(path), "lang/%s/font.nfnt", lang_dir) >= (int)sizeof(path)) return false;
    AssetFile* f = asset_open(path);
    if(!f) return false;
    uint8_t h[32];
    if(!asset_read(f, 0, h, sizeof(h)) || memcmp(h, "NFNT", 4) != 0 || h[4] != 1) goto fail;
    F.flags = h[5];
    F.cell_w = h[6];
    F.cell_h = h[7];
    F.adv_n = h[8];
    F.adv_w = h[9];
    F.pitch = h[10];
    F.baseline = h[11];
    F.n = le16(h + 12);
    F.first = le16(h + 14);
    F.bpr = h[16];
    F.bpg = h[17];
    F.narrow_below = le16(h + 18);
    uint32_t off_adv = le32(h + 20);
    F.off_glyphs = le32(h + 24);
    F.charset = le32(h + 28);
    if(F.cell_w < 1 || F.cell_w > 16 || F.cell_h < 1 || F.cell_h > 16 || F.bpr != (F.cell_w + 7) / 8 ||
       F.bpg != F.bpr * F.cell_h || F.bpg > FONT_MAX_BPG || F.n < 1 || F.pitch < 1 || F.off_glyphs < 32)
        goto fail;
    if(F.flags & 1) {
        if(!heap_can_alloc(F.n) || !(F.adv = malloc(F.n)) || !asset_read(f, off_adv, F.adv, F.n)) goto fail;
    }
    uint32_t total = (uint32_t)F.n * F.bpg;
    if(total <= FONT_RAM_MAX) { // small font: everything resident, file closed
        if(!heap_can_alloc(total) || !(F.ram = malloc(total)) || !asset_read(f, F.off_glyphs, F.ram, total))
            goto fail;
        asset_close(f);
    } else { // big font: glyphs come through the cache; the file is opened per font_prefetch only
        uint32_t need = FONT_CACHE_GLYPHS * (uint32_t)(sizeof(CacheTag) + F.bpg);
        if(!heap_can_alloc(need)) goto fail;
        F.tags = calloc(FONT_CACHE_GLYPHS, sizeof(CacheTag));
        F.cdata = malloc((uint32_t)FONT_CACHE_GLYPHS * F.bpg);
        if(!F.tags || !F.cdata) goto fail;
        F.sd = true;
        memcpy(F.path, path, strlen(path) + 1);
    }
    F.loaded = true;
    if(F.sd) { // SD-backed: the UI glyphs stay resident (read through the file that is open right now)
        static const tchar ui[3] = {GLYPH_MORE, GLYPH_CURSOR, GLYPH_ELLIPSIS};
        prefetch_from(f, ui, 3);
        asset_close(f); // no handle survives font_load
    }
    return true;
fail:
    asset_close(f);
    font_unload();
    return false;
}

bool font_loaded(void) {
    return F.loaded;
}

uint32_t font_charset_id(void) {
    return F.loaded ? F.charset : 0;
}

int font_pitch(void) {
    return F.loaded ? F.pitch : SYSFONT_H;
}

int font_cell_w(void) {
    return F.loaded ? F.cell_w : SYSFONT_W;
}

int font_cell_h(void) {
    return F.loaded ? F.cell_h : SYSFONT_H;
}

int font_baseline(void) {
    return F.loaded ? F.baseline : 8;
}

int font_adv(tchar c) {
    if(!F.loaded) return SYSFONT_ADV;
    int i = (int)c - F.first;
    if(i < 0 || i >= F.n) return F.adv_n;
    if(F.adv) return F.adv[i];
    return c < F.narrow_below ? F.adv_n : F.adv_w;
}

// The UI glyphs (more-arrow, cursor, ellipsis) are loaded with an SD-backed font and never evicted.
static bool pinned(tchar c) {
    return c >= GLYPH_MORE && c <= GLYPH_ELLIPSIS;
}

// Glyph rows of `c`, or NULL when absent / not resident.
static const uint8_t* glyph_rows(tchar c, bool touch) {
    int i = (int)c - F.first;
    if(i < 0 || i >= F.n) return NULL;
    if(F.ram) return F.ram + (uint32_t)i * F.bpg;
    for(int k = 0; k < FONT_CACHE_GLYPHS; k++)
        if(F.tags[k].stamp && F.tags[k].code == c) {
            if(touch && !pinned(c)) F.tags[k].stamp = ++F.tick;
            return F.cdata + (uint32_t)k * F.bpg;
        }
    return NULL;
}

#define PREFETCH_MAX 128 // codes looked at per call (a page is 20 hanzi, a string at most TB_BUF)

static bool resident(tchar c) {
    return glyph_rows(c, false) != NULL;
}

// Reads the missing glyphs of codes[0..n) into the cache through the open file `f`. Stops at the
// first failed read (card gone / file damaged): the remaining glyphs just draw as hollow boxes.
static void prefetch_from(AssetFile* f, const tchar* codes, int n) {
    for(int i = 0; i < PREFETCH_MAX && (n < 0 ? codes[i] != 0 : i < n); i++) {
        tchar c = codes[i];
        int gi = (int)c - F.first;
        if(c < 0x20 || gi < 0 || gi >= F.n || glyph_rows(c, true)) continue;
        int v = -1; // victim: an empty slot or the least recently used unpinned one
        for(int k = 0; k < FONT_CACHE_GLYPHS; k++)
            if(!(F.tags[k].stamp && pinned(F.tags[k].code)) && (v < 0 || F.tags[k].stamp < F.tags[v].stamp)) v = k;
        if(v < 0) return;
        uint8_t* dst = F.cdata + (uint32_t)v * F.bpg;
        if(!asset_read(f, F.off_glyphs + (uint32_t)gi * F.bpg, dst, F.bpg)) {
            F.tags[v].stamp = 0;
            return;
        }
        F.tags[v].code = c;
        F.tags[v].stamp = ++F.tick;
    }
}

static bool missing_any(const tchar* codes, int n) {
    for(int i = 0; i < PREFETCH_MAX && (n < 0 ? codes[i] != 0 : i < n); i++) {
        tchar c = codes[i];
        int gi = (int)c - F.first;
        if(c >= 0x20 && gi >= 0 && gi < F.n && !resident(c)) return true;
    }
    return false;
}

// SD-backed fonts: ONE open/close of the font file per call, and none at all when every glyph is
// already resident. The file is never kept open between calls (see assets.h).
void font_prefetch_many(const tchar* const* strs, int count) {
    if(!F.loaded || !F.sd) return;
    bool missing = false;
    for(int j = 0; j < count && !missing; j++) missing = missing_any(strs[j], -1);
    if(missing) {
        AssetFile* f = asset_open(F.path);
        if(f) {
            for(int j = 0; j < count; j++) prefetch_from(f, strs[j], -1);
            asset_close(f);
        }
    }
    for(int j = 0; j < count; j++) // LRU touch of what was already resident
        for(int i = 0; i < PREFETCH_MAX && strs[j][i]; i++) glyph_rows(strs[j][i], true);
}

void font_prefetch(const tchar* codes, int n) {
    if(!F.loaded || !F.sd) return;
    if(missing_any(codes, n)) {
        AssetFile* f = asset_open(F.path);
        if(f) {
            prefetch_from(f, codes, n);
            asset_close(f);
        }
    }
    for(int i = 0; i < PREFETCH_MAX && (n < 0 ? codes[i] != 0 : i < n); i++) glyph_rows(codes[i], true); // LRU touch
}

void font_draw_glyph(tchar c, int x, int y, uint8_t color) {
    if(!F.loaded) {
        sysfont_draw_glyph(c < 0x100 ? (uint8_t)c : 0, x, y, color, 1);
        return;
    }
    const uint8_t* g = glyph_rows(c, true);
    if(!g || (all_zero(g, F.bpg) && c != ' ')) { // unknown: hollow box
        int top = F.baseline > 6 ? F.baseline - 6 : 0;
        ov_frame(x, y + top, F.cell_w, F.cell_h - top < 6 ? F.cell_h - top : 6, color);
        return;
    }
    ov_bitmap(x, y, F.cell_w, F.cell_h, g, F.bpr, color);
}

void font_draw_text(const tchar* s, int x, int y) {
    for(; *s; s++) {
        font_draw_glyph(*s, x, y, 1);
        x += font_adv(*s);
    }
}

int font_text_width(const tchar* s) {
    int w = 0;
    for(; *s; s++) w += font_adv(*s);
    return w;
}
