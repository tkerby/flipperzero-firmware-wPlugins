#pragma once

/* pokecrystal.sym loader ("bb:aaaa Label" lines from the pret symbols branch). */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char* name;
    uint8_t bank;
    uint16_t addr;
} Sym;

typedef struct {
    Sym* syms; /* sorted by name */
    unsigned count;
    char* text;
} SymTable;

/* NULL path: tools/gen/.cache/symbols/pokecrystal.sym (fetch_pokecrystal.py). */
bool sym_load(SymTable* t, const char* path);
void sym_free(SymTable* t);
const Sym* sym_find(const SymTable* t, const char* name);
/* Like sym_find but aborts with a message when missing. */
const Sym* sym_get(const SymTable* t, const char* name);
/* The label at or before bank:addr in ROM (for traces); NULL if none. */
const Sym* sym_by_addr(const SymTable* t, uint8_t bank, uint16_t addr);
