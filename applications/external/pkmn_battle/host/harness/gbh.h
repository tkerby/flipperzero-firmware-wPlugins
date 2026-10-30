#pragma once

/*
 * One emulated Game Boy Color (SameBoy core) with what the harness needs:
 * deterministic setup, frame stepping, held keys, symbol-addressed memory
 * access and execution hooks on ROM addresses.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sym.h"

typedef struct GB_gameboy_s GB_gameboy_t;
typedef struct Gbh Gbh;

/* Called before the instruction at a hooked address executes. */
typedef void (*GbhHookFn)(Gbh* h, void* ctx);

#define GBH_MAX_HOOKS       32
#define GBH_W               160
#define GBH_H               144
/* SameBoy's time unit: 8 MiHz ticks (twice the single-speed CPU clock). */
#define GBH_TICKS_PER_SEC   8388608ull
/* One frame: 70224 dots at single speed = 140448 ticks. */
#define GBH_TICKS_PER_FRAME 140448ull

enum {
    GBH_KEY_RIGHT = 1 << 0,
    GBH_KEY_LEFT = 1 << 1,
    GBH_KEY_UP = 1 << 2,
    GBH_KEY_DOWN = 1 << 3,
    GBH_KEY_A = 1 << 4,
    GBH_KEY_B = 1 << 5,
    GBH_KEY_SELECT = 1 << 6,
    GBH_KEY_START = 1 << 7,
};

typedef struct {
    uint8_t bank;
    uint16_t addr;
    GbhHookFn fn;
    void* ctx;
} GbhHook;

struct Gbh {
    GB_gameboy_t* gb;
    const SymTable* syms;
    uint32_t pixels[GBH_W * GBH_H];
    uint64_t time; /* ticks since gbh_open (not saved in states) */
    uint64_t frames; /* vblanks since gbh_open */
    uint8_t keys;
    GbhHook hooks[GBH_MAX_HOOKS];
    int nhooks;
    uint8_t hook_map[0x10000 / 8];
    void* link; /* serial bridge owning this GB, if any */
    const char* name; /* for log messages */
};

/* Loads the ROM (path) and prepares a CGB with our boot stub, the RTC
 * driven by emulated time and SameBoy's RNG seeded constantly. */
bool gbh_open(Gbh* h, const char* rom_path, const SymTable* syms, const char* name);
void gbh_close(Gbh* h);

bool gbh_load_battery(Gbh* h, const char* path);
bool gbh_load_state(Gbh* h, const char* path);
bool gbh_save_state(Gbh* h, const char* path);
/* In-memory states (malloc'd buffer) for fast restarts. */
uint8_t* gbh_state_to_buffer(Gbh* h, size_t* size);
bool gbh_state_from_buffer(Gbh* h, const uint8_t* buf, size_t size);

/* Run one CPU step; returns the ticks it took. */
unsigned gbh_step(Gbh* h);
/* Run until the next vblank. */
void gbh_run_frame(Gbh* h);
void gbh_run_frames(Gbh* h, unsigned n);

void gbh_set_keys(Gbh* h, uint8_t keys);
/* Skip drawing frames (much faster; screenshots stay blank) */
void gbh_set_rendering(Gbh* h, bool on);

/* Memory by bus address; WRAM D000-DFFF uses `bank` (1-7), other regions
 * ignore it. Reads have no side effects. */
uint8_t gbh_peek(Gbh* h, uint8_t bank, uint16_t addr);
void gbh_poke(Gbh* h, uint8_t bank, uint16_t addr, uint8_t value);
/* Same, by symbol (+ offset). Aborts on an unknown symbol. */
uint8_t gbh_rd(Gbh* h, const char* sym, unsigned offset);
void gbh_wr(Gbh* h, const char* sym, unsigned offset, uint8_t value);
uint16_t gbh_rd16be(Gbh* h, const char* sym, unsigned offset);

/* Hook the ROM address of a symbol; fn runs before that instruction. */
void gbh_hook(Gbh* h, const char* sym, GbhHookFn fn, void* ctx);
void gbh_hook_addr(Gbh* h, uint8_t bank, uint16_t addr, GbhHookFn fn, void* ctx);

uint16_t gbh_pc(Gbh* h);
uint16_t gbh_sp(Gbh* h);
uint16_t gbh_de(Gbh* h);
/* WRAM $C000-$DFFF (bank 1 at $D000) and HRAM $FF80-$FFFF */
void gbh_snapshot(Gbh* h, uint8_t wram[0x2000], uint8_t hram[0x80]);
uint8_t gbh_rom_bank(Gbh* h);

bool gbh_write_ppm(Gbh* h, const char* path);

double gbh_seconds(const Gbh* h);
uint32_t gbh_ms(const Gbh* h);
