#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-tree-reassoc,no-tree-loop-im,no-tree-coalesce-vars,no-tree-forwprop") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "text.h"
#include "assets.h"
#include "font.h"
#include "gen/langs_gen.h"
#include <stdio.h>
#include <string.h>

#define NTXT_HDR 12 // "NTXT", u16 n, u8 ver (2), u8 lang, u32 charset_id

static uint8_t cur_map;
static uint8_t cur_lang = 0xFF; // 0xFF: not activated yet
static bool font_ok;
static tchar name_player[TEXT_VAR_LEN];
static tchar name_rival[TEXT_VAR_LEN];
static tchar vars[TEXT_VARS][TEXT_VAR_LEN];
static TbGeom geom;
static uint8_t speed_idx = 1; // TEXT_SPEED_NORMAL (settings.h)

static void geom_update(void) {
    int pitch = font_pitch();
    geom.box_h = (int16_t)(TB_LINES * pitch + 2 * TB_PAD_Y);
    geom.box_y = (int16_t)(SCREEN_H - geom.box_h);
    geom.text_x = TB_TEXT_X;
    for(int k = 0; k < TB_LINES; k++) geom.line_y[k] = (int16_t)(geom.box_y + TB_PAD_Y + k * pitch);
    geom.max_px = (int16_t)(TB_W - 2 * TB_TEXT_X);
    geom.more_x = (int16_t)(TB_W - TB_MORE_RIGHT - font_cell_w());
    geom.more_y = (int16_t)(geom.box_y + geom.box_h - TB_MORE_MARGIN);
}

void (*text_lang_hook)(void);

bool text_set_language(uint8_t idx) {
    if(idx >= LANG_COUNT) return false;
    cur_lang = idx;
    font_ok = font_load(LANG_DIR[idx]);
    geom_update();
    if(text_lang_hook) text_lang_hook();
    return font_ok;
}

void text_font_release(void) {
    font_unload();
    font_ok = false;
}

bool text_font_restore(void) {
    if(cur_lang == 0xFF) return true; // nothing was active
    font_ok = font_load(LANG_DIR[cur_lang]);
    geom_update();
    return font_ok;
}

void text_shutdown(void) {
    font_unload();
    font_ok = false;
    cur_lang = 0xFF;
    name_player[0] = name_rival[0] = 0;
    memset(vars, 0, sizeof(vars));
}

static void ensure_lang(void) {
    if(cur_lang == 0xFF) text_set_language(0);
}

uint8_t text_language(void) {
    ensure_lang();
    return cur_lang;
}

const TbGeom* tb_geom(void) {
    ensure_lang();
    return &geom;
}

void text_set_speed(uint8_t speed) {
    speed_idx = speed;
}

uint16_t text_glyph_ms(void) {
    static const uint16_t PCT[4] = {150, 100, 55, 0}; // slow, normal, fast, instant (settings.h TEXT_SPEED_*)
    ensure_lang();
    unsigned base = LANG_TYPE_MS[cur_lang];
    return (uint16_t)(base * PCT[speed_idx < 4 ? speed_idx : 1] / 100);
}

void text_set_map(uint8_t map_id) {
    cur_map = map_id;
}

static void copy_codes(tchar* dst, const tchar* src) {
    int i = 0;
    for(; src && src[i] && i < TEXT_VAR_LEN - 1; i++) dst[i] = src[i];
    dst[i] = 0;
}

void text_set_names(const tchar* player, const tchar* rival) {
    copy_codes(name_player, player);
    copy_codes(name_rival, rival);
}

void text_set_var(uint8_t n, const tchar* s) {
    if(n < TEXT_VARS) copy_codes(vars[n], s);
}

void text_set_var_rival(uint8_t n) {
    if(n < TEXT_VARS) copy_codes(vars[n], name_rival);
}

void text_set_var_num(uint8_t n, int value) {
    if(n >= TEXT_VARS) return;
    tchar tmp[12];
    int k = 0, i = 0;
    unsigned u = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    do {
        tmp[k++] = (tchar)('0' + u % 10);
        u /= 10;
    } while(u && k < 11);
    if(value < 0) vars[n][i++] = '-';
    while(k && i < TEXT_VAR_LEN - 1) vars[n][i++] = tmp[--k];
    vars[n][i] = 0;
}

bool text_set_var_id(uint8_t n, uint16_t id) {
    if(n >= TEXT_VARS) return false;
    int len = text_load(id, vars[n], TEXT_VAR_LEN); // truncated to TEXT_VAR_LEN-1 codes, END terminated
    if(len < 0) {
        vars[n][0] = 0;
        return false;
    }
    for(int i = 0; i < len; i++) // first line only
        if(vars[n][i] == TX_NL || vars[n][i] == TX_PAGE) {
            vars[n][i] = 0;
            break;
        }
    return true;
}

void text_clear_vars(void) {
    memset(vars, 0, sizeof(vars));
}

// Substitution text of a name / variable code, or NULL when `c` is an ordinary code.
static const tchar* token_text(tchar c) {
    if(c == TX_PLAYER) return name_player;
    if(c == TX_RIVAL) return name_rival;
    if(c >= TX_VAR0 && c < TX_VAR0 + TEXT_VARS) return vars[c - TX_VAR0];
    return NULL;
}

int text_load(uint16_t id, tchar* buf, int cap) {
    if(cap < 1) return -1;
    ensure_lang();
    char path[32];
    if(id & TEXT_GLOBAL)
        snprintf(path, sizeof(path), "lang/%s/txt_gl.bin", LANG_DIR[cur_lang]);
    else
        snprintf(path, sizeof(path), "lang/%s/txt_%02u.bin", LANG_DIR[cur_lang], (unsigned)cur_map);
    uint16_t idx = id & 0x7FFF;

    uint8_t hdr[NTXT_HDR];
    if(!read_at(path, 0, hdr, NTXT_HDR) || memcmp(hdr, "NTXT", 4) != 0) return -1;
    if(hdr[6] != 2 || hdr[7] != LANG_ID[cur_lang]) return -1;
    uint32_t cs = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) | ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    if(font_ok && cs != font_charset_id()) return -1; // codes would map to the wrong glyphs
    uint16_t n = (uint16_t)(hdr[4] | (hdr[5] << 8));
    if(idx >= n) return -1;
    uint8_t o[4];
    if(!read_at(path, NTXT_HDR + 2u * idx, o, 4)) return -1;
    uint16_t a = (uint16_t)(o[0] | (o[1] << 8)), b = (uint16_t)(o[2] | (o[3] << 8));
    if(b < a || ((b - a) & 1)) return -1;
    uint32_t base = NTXT_HDR + 2u * (n + 1); // strings area start
    int len = (b - a) / 2;
    if(len > cap - 1) len = cap - 1;
    // read the bytes into buf, then widen them in place (little-endian, independent of the host)
    uint8_t* raw = (uint8_t*)buf;
    if(len > 0 && !read_at(path, base + a, raw, 2u * (uint32_t)len)) return -1;
    for(int i = 0; i < len; i++) buf[i] = (tchar)(raw[2 * i] | (raw[2 * i + 1] << 8));
    // stored strings end with TX_END; cut at the first one
    int l = 0;
    while(l < len && buf[l] != TX_END) l++;
    buf[l] = 0;
    return l;
}

int text_next_page(const tchar* s, int pos) {
    while(s[pos] != TX_END && s[pos] != TX_PAGE) pos++;
    if(s[pos] == TX_PAGE) return pos + 1;
    return -1;
}

int text_line_len(const tchar* s, int pos) {
    int n = 0;
    while(s[pos + n] != TX_END && s[pos + n] != TX_NL && s[pos + n] != TX_PAGE) n++;
    return n;
}

int text_line_start(const tchar* s, int pos, int n) {
    for(int i = 0; i < n; i++) {
        pos += text_line_len(s, pos);
        if(s[pos] != TX_NL) return -1;
        pos++;
    }
    return pos;
}

static int name_len(const tchar* nm) {
    int n = 0;
    while(nm && nm[n]) n++;
    return n;
}

int text_line_glyphs(const tchar* s, int pos) {
    int len = text_line_len(s, pos), g = 0;
    for(int i = 0; i < len; i++) {
        const tchar* t = token_text(s[pos + i]);
        g += t ? name_len(t) : 1;
    }
    return g;
}

int text_line_px(const tchar* s, int pos) {
    int len = text_line_len(s, pos), w = 0;
    for(int i = 0; i < len; i++) {
        const tchar* t = token_text(s[pos + i]);
        if(!t) {
            w += font_adv(s[pos + i]);
            continue;
        }
        for(; *t; t++) w += font_adv(*t);
    }
    return w;
}

#ifndef NUCLEO_DEVICE
int text_expand(const tchar* s, int pos, tchar* out, int cap) {
    int len = text_line_len(s, pos), n = 0;
    for(int i = 0; i < len; i++) {
        const tchar* t = token_text(s[pos + i]);
        if(!t) {
            if(n < cap - 1) out[n++] = s[pos + i];
            continue;
        }
        for(; *t && n < cap - 1; t++) out[n++] = *t;
    }
    out[n] = 0;
    return n;
}
#endif

int text_draw_line(const tchar* s, int pos, int x, int y, int max_glyphs) {
    int len = text_line_len(s, pos), g = 0;
    for(int i = 0; i < len && g < max_glyphs; i++) {
        tchar c = s[pos + i];
        const tchar* nm = token_text(c);
        if(nm) {
            for(; *nm && g < max_glyphs; nm++, g++) {
                font_draw_glyph(*nm, x, y, 1);
                x += font_adv(*nm);
            }
        } else {
            font_draw_glyph(c, x, y, 1);
            x += font_adv(c);
            g++;
        }
    }
    return g;
}

// ---- prefetch: the glyphs of the substituted names / variables must be resident too (SD-backed
// font). Strings that use tokens are passed to the font together with the token texts, in ONE
// font_prefetch_many call (one open of the font file).
#define PF_MAX_STR 12

// Appends to out[*n] the token texts that the codes s[0..len) (len < 0: up to END) reference.
static void add_used_tokens(const tchar* s, int len, const tchar** out, int* n, int cap) {
    for(int i = 0; (len < 0 ? s[i] != TX_END : i < len); i++) {
        const tchar* t = token_text(s[i]);
        if(!t || !t[0]) continue;
        int dup = 0;
        for(int k = 0; k < *n; k++) dup |= (out[k] == t);
        if(!dup && *n < cap) out[(*n)++] = t;
    }
}

// A page that uses names / variables needs its own END-terminated copy next to the token texts. The copy (0.4 KB) is on the
// stack of this helper only, not in .bss: a stack frame costs nothing while exploring (contract 5, Memory).
static __attribute__((noinline)) void prefetch_page_tokens(const tchar* page, int n, const tchar** strs, int cnt) {
    tchar copy[TB_BUF];
    if(n > TB_BUF - 1) n = TB_BUF - 1;
    memcpy(copy, page, (size_t)n * sizeof(tchar));
    copy[n] = 0;
    strs[0] = copy;
    font_prefetch_many(strs, cnt);
}

void text_prefetch_page(const tchar* s, int pos) {
    int n = 0;
    while(s[pos + n] != TX_END && s[pos + n] != TX_PAGE) n++;
    const tchar* strs[1 + 2 + TEXT_VARS];
    int cnt = 1;
    add_used_tokens(s + pos, n, strs, &cnt, 1 + 2 + TEXT_VARS); // tokens first (slot 0 is the page)
    if(cnt == 1) { // no token: the plain path
        font_prefetch(s + pos, n);
        return;
    }
    prefetch_page_tokens(s + pos, n, strs, cnt);
}

void text_prefetch(const tchar* s) {
    const tchar* strs[1 + 2 + TEXT_VARS];
    strs[0] = s;
    int cnt = 1;
    add_used_tokens(s, -1, strs, &cnt, 1 + 2 + TEXT_VARS);
    if(cnt == 1)
        font_prefetch(s, -1);
    else
        font_prefetch_many(strs, cnt);
}

void text_prefetch_many(const tchar* const* strs, int count) {
    const tchar* all[PF_MAX_STR + 2 + TEXT_VARS];
    int cnt = 0;
    for(int j = 0; j < count && cnt < PF_MAX_STR; j++) all[cnt++] = strs[j];
    int base = cnt;
    for(int j = 0; j < base; j++) add_used_tokens(all[j], -1, all, &cnt, PF_MAX_STR + 2 + TEXT_VARS);
    font_prefetch_many(all, cnt);
}
