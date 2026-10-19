#pragma once
// Names (task K, contract 9.2 / 11.6), UI plugin side: the kb.bin table of a language (the naming keyboard's grid pages and the
// code <-> Unicode pairs of the characters a name can hold) and the functions that turn the Unicode names of PartyState into the
// font codes of the language in use. Plugin-internal header (scene_kb.c, names.c, scene_name.c). No furi.h.
#include <stdint.h>
#include <stdbool.h>
#include "font.h" // tchar

#define KB_HDR       20 // kb.bin header bytes
#define KB_MAX_PAGES 4 // page labels gl.kb.pg0..pg3

typedef struct {
    uint8_t* raw; // the whole file (heap), NULL = not loaded
    uint8_t pages, rows, cols, top, max_name, max_nick;
    uint16_t n_chars;
} Kb;

// Reads + validates lang/<dir>/kb.bin of language index `lang` (<= 1.2 KB of heap, freed by kb_free). The charset id is only checked for
// the ACTIVE language (its font is loaded). False: missing / stale / bad.
bool kb_load(Kb* k, uint8_t lang);
void kb_free(Kb* k);
tchar kb_cell(
    const Kb* k,
    uint8_t page,
    uint8_t r,
    uint8_t c); // font code of a grid cell, 0 = empty
uint16_t kb_c2u(const Kb* k, uint16_t code); // font code -> Unicode ('?' = unknown)
uint16_t kb_u2c(const Kb* k, uint16_t u); // Unicode -> font code ('?' = unknown)

// Derives the codes of the active language from PartyState's Unicode names (text_set_names + the nickname cache); see party.h
// party_apply_names (the resident wrapper that loads the plugin for it). Also the UiPlugin.apply_names entry.
void names_apply(void);
