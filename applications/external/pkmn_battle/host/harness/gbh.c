#include "gbh.h"

#include <Core/gb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "boot_stub.h"

static uint32_t rgb_encode(GB_gameboy_t* gb, uint8_t r, uint8_t g, uint8_t b) {
    (void)gb;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void on_vblank(GB_gameboy_t* gb, GB_vblank_type_t type) {
    (void)type;
    Gbh* h = GB_get_user_data(gb);
    h->frames++;
}

static void on_log(GB_gameboy_t* gb, const char* string, GB_log_attributes_t attributes) {
    (void)gb;
    (void)string;
    (void)attributes;
}

static void load_boot(GB_gameboy_t* gb, GB_boot_rom_t type) {
    static uint8_t stub[BOOT_STUB_SIZE];
    (void)type;
    boot_stub_build(stub);
    GB_load_boot_rom_from_buffer(gb, stub, sizeof(stub));
}

static void on_exec(GB_gameboy_t* gb, uint16_t addr, uint8_t opcode) {
    (void)opcode;
    Gbh* h = GB_get_user_data(gb);
    if(!(h->hook_map[addr >> 3] & (1u << (addr & 7)))) return;
    uint8_t bank = addr < 0x4000 ? 0 : gbh_rom_bank(h);
    for(int i = 0; i < h->nhooks; i++) {
        if(h->hooks[i].addr == addr && h->hooks[i].bank == bank)
            h->hooks[i].fn(h, h->hooks[i].ctx);
    }
}

bool gbh_open(Gbh* h, const char* rom_path, const SymTable* syms, const char* name) {
    memset(h, 0, sizeof(*h));
    h->syms = syms;
    h->name = name;
    GB_random_seed(0x504B42); /* power-on RAM and other SameBoy randomness */
    h->gb = GB_init(GB_alloc(), GB_MODEL_CGB_E);
    GB_set_user_data(h->gb, h);
    GB_set_log_callback(h->gb, on_log);
    GB_set_boot_rom_load_callback(h->gb, load_boot);
    GB_set_rgb_encode_callback(h->gb, rgb_encode);
    GB_set_pixels_output(h->gb, h->pixels);
    GB_set_vblank_callback(h->gb, on_vblank);
    GB_set_execution_callback(h->gb, on_exec);
    GB_set_emulate_joypad_bouncing(h->gb, false);
    GB_set_rtc_mode(h->gb, GB_RTC_MODE_ACCURATE);
    if(GB_load_rom(h->gb, rom_path)) {
        fprintf(stderr, "cannot load ROM %s\n", rom_path);
        return false;
    }
    GB_reset(h->gb);
    return true;
}

void gbh_close(Gbh* h) {
    if(h->gb) {
        GB_free(h->gb);
        GB_dealloc(h->gb);
    }
    h->gb = NULL;
}

/* A 32 KiB save may carry a 48-byte VBA-style RTC footer: 5 current and 5
 * latched registers (u32 each: S, M, H, DL, DH) and a u64 Unix time. We
 * clear DH's day-carry and halt bits (a set carry makes Crystal ask to reset
 * the clock) and stamp it "now" so SameBoy adds no elapsed time. */
#define RTC_FOOTER 48
bool gbh_load_battery(Gbh* h, const char* path) {
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)len);
    bool ok = fread(buf, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    if(ok && len == 0x8000 + RTC_FOOTER) {
        uint8_t* rtc = buf + 0x8000;
        rtc[16] &= 0x01;
        rtc[36] &= 0x01;
        uint64_t now = (uint64_t)time(NULL);
        for(int i = 0; i < 8; i++)
            rtc[40 + i] = (uint8_t)(now >> (8 * i));
    }
    if(ok) GB_load_battery_from_buffer(h->gb, buf, (size_t)len);
    free(buf);
    return ok;
}

bool gbh_load_state(Gbh* h, const char* path) {
    if(GB_load_state(h->gb, path)) {
        fprintf(stderr, "cannot load state %s\n", path);
        return false;
    }
    GB_set_rtc_mode(h->gb, GB_RTC_MODE_ACCURATE);
    return true;
}

bool gbh_save_state(Gbh* h, const char* path) {
    return GB_save_state(h->gb, path) == 0;
}

uint8_t* gbh_state_to_buffer(Gbh* h, size_t* size) {
    *size = GB_get_save_state_size(h->gb);
    uint8_t* buf = malloc(*size);
    GB_save_state_to_buffer(h->gb, buf);
    return buf;
}

bool gbh_state_from_buffer(Gbh* h, const uint8_t* buf, size_t size) {
    if(GB_load_state_from_buffer(h->gb, buf, size)) return false;
    GB_set_rtc_mode(h->gb, GB_RTC_MODE_ACCURATE);
    return true;
}

unsigned gbh_step(Gbh* h) {
    unsigned t = GB_run(h->gb);
    h->time += t;
    return t;
}

void gbh_run_frame(Gbh* h) {
    uint64_t f = h->frames;
    while(h->frames == f)
        gbh_step(h);
}

void gbh_run_frames(Gbh* h, unsigned n) {
    while(n--)
        gbh_run_frame(h);
}

void gbh_set_rendering(Gbh* h, bool on) {
    GB_set_rendering_disabled(h->gb, !on);
}

void gbh_set_keys(Gbh* h, uint8_t keys) {
    h->keys = keys;
    GB_set_key_mask(h->gb, (GB_key_mask_t)keys);
}

static uint8_t* mem_ptr(Gbh* h, uint8_t bank, uint16_t addr) {
    size_t size;
    uint16_t cur;
    if(addr >= 0xC000 && addr < 0xD000) {
        return (uint8_t*)GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_RAM, &size, &cur) +
               (addr - 0xC000);
    }
    if(addr >= 0xD000 && addr < 0xE000) {
        uint8_t* ram = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_RAM, &size, &cur);
        if(!bank) bank = (uint8_t)cur;
        return ram + bank * 0x1000 + (addr - 0xD000);
    }
    if(addr >= 0xFF80 && addr < 0xFFFF) {
        return (uint8_t*)GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_HRAM, &size, &cur) +
               (addr - 0xFF80);
    }
    return NULL;
}

uint8_t gbh_peek(Gbh* h, uint8_t bank, uint16_t addr) {
    uint8_t* p = mem_ptr(h, bank, addr);
    return p ? *p : GB_safe_read_memory(h->gb, addr);
}

void gbh_poke(Gbh* h, uint8_t bank, uint16_t addr, uint8_t value) {
    uint8_t* p = mem_ptr(h, bank, addr);
    if(p) {
        *p = value;
    } else {
        GB_write_memory(h->gb, addr, value);
    }
}

uint8_t gbh_rd(Gbh* h, const char* sym, unsigned offset) {
    const Sym* s = sym_get(h->syms, sym);
    return gbh_peek(h, s->bank, (uint16_t)(s->addr + offset));
}

void gbh_wr(Gbh* h, const char* sym, unsigned offset, uint8_t value) {
    const Sym* s = sym_get(h->syms, sym);
    gbh_poke(h, s->bank, (uint16_t)(s->addr + offset), value);
}

uint16_t gbh_rd16be(Gbh* h, const char* sym, unsigned offset) {
    return (uint16_t)(gbh_rd(h, sym, offset) << 8 | gbh_rd(h, sym, offset + 1));
}

void gbh_hook_addr(Gbh* h, uint8_t bank, uint16_t addr, GbhHookFn fn, void* ctx) {
    if(h->nhooks == GBH_MAX_HOOKS) {
        fprintf(stderr, "too many hooks\n");
        exit(2);
    }
    if(addr < 0x4000) bank = 0;
    h->hooks[h->nhooks++] = (GbhHook){bank, addr, fn, ctx};
    h->hook_map[addr >> 3] |= (uint8_t)(1u << (addr & 7));
}

void gbh_hook(Gbh* h, const char* sym, GbhHookFn fn, void* ctx) {
    const Sym* s = sym_get(h->syms, sym);
    gbh_hook_addr(h, s->bank, s->addr, fn, ctx);
}

uint16_t gbh_pc(Gbh* h) {
    return GB_get_registers(h->gb)->pc;
}

uint16_t gbh_sp(Gbh* h) {
    return GB_get_registers(h->gb)->sp;
}

uint16_t gbh_de(Gbh* h) {
    return GB_get_registers(h->gb)->de;
}

void gbh_snapshot(Gbh* h, uint8_t wram[0x2000], uint8_t hram[0x80]) {
    size_t size;
    uint16_t bank;
    const uint8_t* ram = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_RAM, &size, &bank);
    memcpy(wram, ram, 0x1000);
    memcpy(wram + 0x1000, ram + 0x1000, 0x1000); /* bank 1 */
    const uint8_t* hr = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_HRAM, &size, &bank);
    memcpy(hram, hr, 0x7F);
    hram[0x7F] = GB_safe_read_memory(h->gb, 0xFFFF);
}

uint8_t gbh_rom_bank(Gbh* h) {
    size_t size;
    uint16_t bank;
    GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_ROM, &size, &bank);
    return (uint8_t)bank;
}

bool gbh_write_ppm(Gbh* h, const char* path) {
    FILE* f = fopen(path, "wb");
    if(!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", GBH_W, GBH_H);
    for(int i = 0; i < GBH_W * GBH_H; i++) {
        uint8_t rgb[3] = {
            (uint8_t)(h->pixels[i] >> 16), (uint8_t)(h->pixels[i] >> 8), (uint8_t)h->pixels[i]};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}

double gbh_seconds(const Gbh* h) {
    return (double)h->time / (double)GBH_TICKS_PER_SEC;
}

uint32_t gbh_ms(const Gbh* h) {
    return (uint32_t)(h->time * 1000 / GBH_TICKS_PER_SEC);
}
