#include "debug.h"
#include <furi.h>
#include <furi_hal.h>
#include <stm32wbxx.h> // DWT / CoreDebug (CMSIS)
#include <stdio.h>
#include "assets.h"
#include "battle_host.h"
#include "render.h"

#ifdef NUCLEO_DEBUG // contract 70: the timing counters and the overlay's formatters exist only in the DEV build (debug_log_stack is in both: the serial `log` lines)
static uint32_t t_render, t_compose;
static volatile uint32_t r_ms, c_ms;
static volatile uint32_t flips;
static uint32_t win_start, win_flips, fps;
static uint32_t q_t0, q_acc, q_n;
static volatile uint32_t q_us;
static volatile bool q_slow;
static volatile uint32_t main_stack_free =
    0xFFFFFFFFu; // main (flip loop) thread: bytes never used, refreshed every Q window
#define Q_WINDOW 32 // flips averaged per Q update

void debug_render_begin(void) {
    t_render = furi_get_tick();
}
void debug_render_end(void) {
    r_ms = furi_get_tick() - t_render;
}
void debug_compose_begin(void) {
    t_compose = furi_get_tick();
}
void debug_compose_end(void) {
    c_ms = furi_get_tick() - t_compose;
}
void debug_flip(void) {
    flips++;
}

// Flip cost from the DWT cycle counter (64 MHz core clock). Counter is enabled defensively; the
// firmware already runs it for furi_delay_us.
void debug_flip_begin(void) {
    if(!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    q_t0 = DWT->CYCCNT;
}
void debug_flip_end(void) {
    q_acc += DWT->CYCCNT - q_t0; // unsigned wrap-safe
    if(++q_n >= Q_WINDOW) {
        uint32_t ipus = furi_hal_cortex_instructions_per_microsecond();
        q_us = q_acc / q_n / (ipus ? ipus : 64);
        q_acc = 0;
        q_n = 0;
        main_stack_free =
            furi_thread_get_stack_space(furi_thread_get_current_id()); // flip loop = main thread
    }
}
void debug_set_fast_fb(bool fast) {
    q_slow = !fast;
}

uint32_t debug_render_ms(void) {
    return r_ms;
}
uint32_t debug_compose_ms(void) {
    return c_ms;
}

uint32_t debug_flips_per_sec(void) {
    uint32_t now = furi_get_tick();
    if(now - win_start >= 1000) {
        uint32_t f = flips;
        fps = (f - win_flips) * 1000 / (now - win_start);
        win_flips = f;
        win_start = now;
    }
    return fps;
}

int debug_format_mem(char* out, size_t cap) {
    HeapStats h;
    heap_stats_now(&h);
    return heap_format(&h, out, cap);
}

int debug_format_stack(char* out, size_t cap) {
    return snprintf(
        out,
        cap,
        "S%u T%u P%d",
        (unsigned)furi_thread_get_stack_space(furi_thread_get_current_id()),
        (unsigned)main_stack_free,
        (int)plugin_kind()); // (contract 73: the H field, the slowest card read of the hero / stream loads, is gone: -0.2 KB of the DEV build)
}

int debug_format(char* out, size_t cap) {
    return snprintf(
        out,
        cap,
        "R%u C%u F%u Q%u%s",
        (unsigned)r_ms,
        (unsigned)c_ms,
        (unsigned)debug_flips_per_sec(),
        (unsigned)q_us,
        q_slow ? "x" : "");
}
#endif

void debug_log_stack(const char* tag, uint32_t stack_size) {
    FURI_LOG_I(
        "Nucleo",
        "%s stack: %u of %u bytes never used",
        tag,
        (unsigned)furi_thread_get_stack_space(furi_thread_get_current_id()),
        (unsigned)stack_size);
}
