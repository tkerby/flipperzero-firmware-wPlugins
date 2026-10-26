#pragma once
// Per-frame timing counters (milliseconds) for on-screen debug text.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef NUCLEO_DEBUG
void debug_render_begin(void);
void debug_render_end(void);
void debug_compose_begin(void);
void debug_compose_end(void);
void debug_flip(void); // call from the flip loop after every canvas_commit
// Flip loop: bracket the (memcpy + canvas_commit) of one flip to measure its cost (Q field).
void debug_flip_begin(void);
void debug_flip_end(void);
void debug_set_fast_fb(bool fast); // false = canvas_draw_xbm fallback in use (Q gets an 'x')
#else
// Contract 70: the RELEASE build has no timing counters (they only feed the DEV overlay's R C F Q fields): the calls compile to nothing.
#define debug_render_begin()    ((void)0)
#define debug_render_end()      ((void)0)
#define debug_compose_begin()   ((void)0)
#define debug_compose_end()     ((void)0)
#define debug_flip()            ((void)0)
#define debug_flip_begin()      ((void)0)
#define debug_flip_end()        ((void)0)
#define debug_set_fast_fb(fast) ((void)(fast))
#endif

uint32_t debug_render_ms(void);
uint32_t debug_compose_ms(void);
uint32_t debug_flips_per_sec(void); // sub-frames/s (real image rate = value / 3)
// Formats "R20 C9 F61 Q2900" (render ms, compose ms, flips/s, us per flip; "Q2900x" when the
// slow xbm fallback is active). Returns chars written.
int debug_format(char* out, size_t cap);
// Formats the heap like the title screen: "F57.1 M40.0 m55.0" = free KB, largest free block KB, minimum free since boot KB.
int debug_format_mem(char* out, size_t cap);
// Formats "S2300 T2900 P1": S = bytes of the CALLING thread's stack that were never used (high-water mark; call it from the
// game thread), T = the same for the main (flip loop) thread (refreshed every 32 flips), P = loaded plugin (0 none, 1 battle,
// 2 menu), (the H field, the slowest hero.bin facing load, was dropped in contract 73).
int debug_format_stack(char* out, size_t cap);
// Logs (serial CLI `log`) the stack headroom of the calling thread under `tag` (game thread: "game stack: ...").
void debug_log_stack(const char* tag, uint32_t stack_size);
