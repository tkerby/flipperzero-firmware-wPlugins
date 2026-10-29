#include "sym.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_name(const void* a, const void* b) {
    return strcmp(((const Sym*)a)->name, ((const Sym*)b)->name);
}

bool sym_load(SymTable* t, const char* path) {
    char def[1024];
    memset(t, 0, sizeof(*t));
    if(!path) {
        snprintf(def, sizeof(def), "%s/tools/gen/.cache/symbols/pokecrystal.sym", PKB_SOURCE_DIR);
        path = def;
    }
    FILE* f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "cannot open %s (run tools/gen/fetch_pokecrystal.py)\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    t->text = malloc((size_t)len + 1);
    if(fread(t->text, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        return false;
    }
    fclose(f);
    t->text[len] = 0;

    unsigned cap = 1;
    for(char* p = t->text; *p; p++)
        cap += *p == '\n';
    t->syms = calloc(cap, sizeof(Sym));
    for(char* line = strtok(t->text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        unsigned bank, addr;
        int n = 0;
        if(line[0] == ';' || sscanf(line, "%x:%x %n", &bank, &addr, &n) != 2 || !line[n]) continue;
        t->syms[t->count++] = (Sym){line + n, (uint8_t)bank, (uint16_t)addr};
    }
    qsort(t->syms, t->count, sizeof(Sym), cmp_name);
    return true;
}

void sym_free(SymTable* t) {
    free(t->syms);
    free(t->text);
    memset(t, 0, sizeof(*t));
}

const Sym* sym_find(const SymTable* t, const char* name) {
    Sym key = {(char*)name, 0, 0};
    return bsearch(&key, t->syms, t->count, sizeof(Sym), cmp_name);
}

const Sym* sym_get(const SymTable* t, const char* name) {
    const Sym* s = sym_find(t, name);
    if(!s) {
        fprintf(stderr, "symbol %s not found\n", name);
        exit(2);
    }
    return s;
}

const Sym* sym_by_addr(const SymTable* t, uint8_t bank, uint16_t addr) {
    const Sym* best = NULL;
    if(addr >= 0x8000) return NULL;
    if(addr < 0x4000) bank = 0;
    for(unsigned i = 0; i < t->count; i++) {
        const Sym* s = &t->syms[i];
        if(s->bank != bank || s->addr > addr || s->addr >= 0x8000) continue;
        if(!best || s->addr > best->addr) best = s;
    }
    return best;
}
