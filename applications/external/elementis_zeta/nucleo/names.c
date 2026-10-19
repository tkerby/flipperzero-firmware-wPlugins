// Names (task K, contract 9.2), UI plugin side. See names.h and party.h.
//
// PartyState keeps every name as UNICODE (u16; ASCII is itself) so a save survives a font rebuild and a language switch. The text system
// wants the font codes of the ACTIVE language: ASCII is the same code in every font, the rest comes from kb.bin (mkfont.py writes it
// per language: the keyboard's keys, the `hidden` characters and the characters of the suggested names, as (code, Unicode) pairs).
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "names.h"
#include "party.h"
#include "text.h"
#include "assets.h"
#include "gen/maps_gen.h"
#include "gen/langs_gen.h"

#define NAME_PRESETS 4

// ---------------------------------------------------------------- kb.bin
static inline uint16_t kb_rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool kb_load(Kb* k, uint8_t lang) {
    k->raw = NULL;
    if(lang >= LANG_COUNT) return false;
    char path[28];
    snprintf(path, sizeof(path), "lang/%s/kb.bin", LANG_DIR[lang]);
    int32_t n = file_size(path);
    if(n < KB_HDR || n > 4096 || !heap_can_alloc((uint32_t)n)) return false;
    uint8_t* b = malloc((size_t)n);
    if(!b) return false;
    if(!read_at(path, 0, b, (uint32_t)n) || memcmp(b, "NKBD", 4) != 0 || b[4] != 1 || b[5] != LANG_ID[lang]) goto bad;
    k->pages = b[6];
    k->rows = b[7];
    k->cols = b[8];
    k->top = b[9] & 1;
    k->max_name = b[10];
    k->max_nick = b[11];
    k->n_chars = kb_rd16(b + 12);
    if(!k->pages || k->pages > KB_MAX_PAGES || !k->rows || !k->cols || k->max_name >= PARTY_NAME_CAP || k->max_nick >= PARTY_NICK_CAP) goto bad;
    if((uint32_t)n != KB_HDR + 4u * k->n_chars + 2u * k->pages * k->rows * k->cols) goto bad;
    if(lang == text_language() && kb_rd16(b + 16) + ((uint32_t)kb_rd16(b + 18) << 16) != font_charset_id()) goto bad;
    k->raw = b;
    return true;
bad:
    free(b);
    return false;
}

void kb_free(Kb* k) {
    free(k->raw);
    k->raw = NULL;
}

tchar kb_cell(const Kb* k, uint8_t page, uint8_t r, uint8_t c) {
    const uint8_t* cells = k->raw + KB_HDR + 4u * k->n_chars;
    return kb_rd16(cells + 2u * (((uint32_t)page * k->rows + r) * k->cols + c));
}

uint16_t kb_c2u(const Kb* k, uint16_t code) {
    if(code < 0x7F) return code;
    for(uint16_t i = 0; i < k->n_chars; i++)
        if(kb_rd16(k->raw + KB_HDR + 4u * i) == code) return kb_rd16(k->raw + KB_HDR + 4u * i + 2);
    return '?';
}

uint16_t kb_u2c(const Kb* k, uint16_t u) {
    if(u < 0x7F) return u;
    for(uint16_t i = 0; i < k->n_chars; i++)
        if(kb_rd16(k->raw + KB_HDR + 4u * i + 2) == u) return kb_rd16(k->raw + KB_HDR + 4u * i);
    return '?';
}

// In-place typecast of `n` flat values between Unicode and the codes of language `lang`; a character without an entry (or no table at
// all) becomes '?'.
static void convert(bool c2u, uint8_t lang, uint16_t* v, uint16_t n) {
    bool wide = false;
    for(uint16_t i = 0; i < n; i++)
        if(v[i] > 0x7E) wide = true;
    if(!wide) return;
    Kb k;
    const bool ok = kb_load(&k, lang);
    for(uint16_t i = 0; i < n; i++) {
        if(v[i] < 0x7F) continue;
        v[i] = ok ? (c2u ? kb_c2u(&k, v[i]) : kb_u2c(&k, v[i])) : '?';
    }
    if(ok) kb_free(&k);
}

// ---------------------------------------------------------------- PartyState names -> codes
// `n` strings of `cap` u16 each, cut to `maxlen` characters and END-terminated.
static void cut_copy(tchar* dst, const uint16_t* src, uint8_t cap, uint8_t n, uint8_t maxlen) {
    for(uint8_t s = 0; s < n; s++, dst += cap, src += cap) {
        uint8_t i = 0;
        for(; i < maxlen && i < cap - 1 && src[i]; i++) dst[i] = src[i];
        for(; i < cap; i++) dst[i] = 0;
    }
}

void names_apply(void) {
    PartyState* ps = party_state();
    NickRow* nc = party_nick_cache();
    const uint8_t lg = text_language();
    const uint8_t cnt = ps->party.count < BC_MAX_PARTY ? ps->party.count : BC_MAX_PARTY;
    for(uint8_t k = 0; k < BC_MAX_PARTY; k++) { // sanitise what a save may hold
        ps->nick_u[k][PARTY_NICK_CAP - 1] = 0;
        if(k >= cnt) ps->nick_u[k][0] = 0;
    }
    ps->name_u[0][PARTY_NAME_CAP - 1] = ps->name_u[1][PARTY_NAME_CAP - 1] = 0;
    cut_copy(nc[0], ps->nick_u[0], PARTY_NICK_CAP, BC_MAX_PARTY, LANG_MAX_NICK[lg]);
    convert(false, lg, nc[0], BC_MAX_PARTY * PARTY_NICK_CAP);
    tchar nm[2 * PARTY_NAME_CAP];
    cut_copy(nm, ps->name_u[0], PARTY_NAME_CAP, 2, LANG_MAX_NAME[lg]);
    convert(false, lg, nm, 2 * PARTY_NAME_CAP);
    text_set_names(nm[0] ? nm : NULL, nm[PARTY_NAME_CAP] ? nm + PARTY_NAME_CAP : NULL);
}

static void put_name(uint16_t* dst, const uint16_t* u, uint8_t cap) {
    uint8_t i = 0;
    for(; i < cap - 1 && u[i]; i++) dst[i] = u[i];
    for(; i < cap; i++) dst[i] = 0;
}

void party_name_set(bool rival, const uint16_t* u) {
    PartyState* ps = party_state();
    put_name(ps->name_u[rival ? 1 : 0], u, PARTY_NAME_CAP);
    if(rival) ps->name_rival = PARTY_NAME_CUSTOM;
    else ps->name_player = PARTY_NAME_CUSTOM;
    names_apply();
}

void party_nick_set(uint8_t slot, const uint16_t* u) {
    PartyState* ps = party_state();
    if(slot >= ps->party.count || slot >= BC_MAX_PARTY) return;
    put_name(ps->nick_u[slot], u, PARTY_NICK_CAP);
    names_apply();
}

// Suggested name `idx` of the active language -> Unicode.
static bool preset_u(bool rival, uint8_t idx, uint16_t* u) {
    tchar c[PARTY_NAME_CAP];
    if(idx >= NAME_PRESETS || text_load((uint16_t)((rival ? TXT_GL_NAME_R0 : TXT_GL_NAME_P0) + idx), c, PARTY_NAME_CAP) < 0) return false;
    for(uint8_t i = 0; i < PARTY_NAME_CAP; i++) u[i] = c[i];
    convert(true, text_language(), u, PARTY_NAME_CAP);
    return true;
}

void party_set_name(bool rival, uint8_t idx) {
    PartyState* ps = party_state();
    uint16_t u[PARTY_NAME_CAP];
    if(preset_u(rival, idx, u)) {
        put_name(ps->name_u[rival ? 1 : 0], u, PARTY_NAME_CAP);
        if(rival) ps->name_rival = idx;
        else ps->name_player = idx;
    }
    names_apply();
}

void party_migrate_names(void) {
    PartyState* ps = party_state();
    for(uint8_t w = 0; w < 2; w++) {
        uint8_t idx = w ? ps->name_rival : ps->name_player;
        uint16_t u[PARTY_NAME_CAP];
        memset(ps->name_u[w], 0, sizeof(ps->name_u[w]));
        if(preset_u(w, idx, u)) put_name(ps->name_u[w], u, PARTY_NAME_CAP);
    }
}
