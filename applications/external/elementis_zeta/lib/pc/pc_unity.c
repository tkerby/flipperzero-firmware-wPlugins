// Unity build of the PC plugin (ez_pc.fal): ONE translation unit compiled with -fno-function-sections and -fwhole-program (application.fam), like the battle and UI
// plugins (lib/battle/battle_unity.c, lib/ui/ui_unity.c; the reasons are in lib/ui/ui_unity.c and contract 44): fewer relocation entries in the load-time data, what the
// plugin never calls is dropped. The sources are unchanged: the host harness compiles them one by one; the entry point is marked externally_visible.
// Relocation thinning (memory recovery of the C7 / C9 merge): every call of an APP function from this unit is a relocation entry of 3 bytes in the load-time data (+13 or
// +9 for the target once), and the load transient (`pc_open`) pays for it. The most-called app functions go through a one-instruction static wrapper (a tail call: ONE
// reference each); the macros below rewrite the calls of the sources that follow. Headers first (they declare the real functions); a noinline wrapper cannot be folded back.
#include "../../font.h"
#include "../../frame.h"
#include "../../party.h"
#include "../../assets.h"
#include "../../text.h"
#include "../../sound.h"
// (naked: the compiler options of the plugin switch sibling-call optimisation off, a plain C wrapper would be push / bl / pop = 6 bytes + a stack frame; the arguments are
// already in r0 - r3 / on the stack exactly as the callee wants them, so one `b.w` is the whole body)
#define PCW(R, N, P, A) static __attribute__((naked, noinline)) R N##_w P { __asm__ volatile("b.w " #N); __builtin_unreachable(); }
#define PCV(N, P, A) static __attribute__((naked, noinline)) void N##_w P { __asm__ volatile("b.w " #N); __builtin_unreachable(); }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
PCV(font_draw_text, (const tchar* s, int x, int y), (s, x, y))
PCW(int, font_pitch, (void), ())
PCW(PartyState*, party_state, (void), ())
PCW(int, font_text_width, (const tchar* s), (s))
PCW(bool, read_at, (const char* path, uint32_t o, void* b, uint32_t n), (path, o, b, n))
PCV(ov_bitmap, (int x, int y, int w, int h, const uint8_t* rows, int bpr, uint8_t c), (x, y, w, h, rows, bpr, c))
PCW(int, text_load, (uint16_t id, tchar* buf, int cap), (id, buf, cap))
PCW(int, font_cell_w, (void), ())
PCW(int, font_baseline, (void), ())
PCW(Party*, party_player, (void), ())
PCW(const BcData*, party_data, (void), ())
PCV(ov_fill, (int x, int y, int w, int h, uint8_t c), (x, y, w, h, c))
PCV(font_draw_glyph, (tchar ch, int x, int y, uint8_t c), (ch, x, y, c))
PCV(text_prefetch_many, (const tchar* const* s, int n), (s, n))
PCV(ov_frame, (int x, int y, int w, int h, uint8_t c), (x, y, w, h, c))
PCV(sound_play, (uint16_t id), (id))
// The same for the three libc calls the plugin makes most (D1, contract 68: the Registro's silhouette entry needed ~130 B and the DEV `pc_open` row had 159 B): a firmware function is out of
// `b.w` reach (flash), so the wrapper loads its address from ONE literal word (one relocation) and `bx`es to it, instead of every call site carrying its own literal word + relocation
// (~7 B of code and load-time data each: memset 9 sites, free 8, memcpy 5).
#define PCL(R, N, P) static __attribute__((naked, noinline)) R N##_l P { __asm__ volatile("ldr.w r12, 1f\n\tbx r12\n\t.align 2\n1:\t.word " #N); __builtin_unreachable(); }
PCL(void*, memset, (void* d, int c, size_t n))
PCL(void*, memcpy, (void* d, const void* s, size_t n))
PCL(void, free, (void* p))
#pragma GCC diagnostic pop
#define memset memset_l
#define memcpy memcpy_l
#define free free_l
#define font_draw_text font_draw_text_w
#define font_pitch font_pitch_w
#define party_state party_state_w
#define font_text_width font_text_width_w
#define read_at read_at_w
#define ov_bitmap ov_bitmap_w
#define text_load text_load_w
#define font_cell_w font_cell_w_w
#define font_baseline font_baseline_w
#define party_player party_player_w
#define party_data party_data_w
#define ov_fill ov_fill_w
#define font_draw_glyph font_draw_glyph_w
#define text_prefetch_many text_prefetch_many_w
#define ov_frame ov_frame_w
#define sound_play sound_play_w
#include "../../scene_box.c"
#include "../../scene_card.c"
#include "../../scene_tcard.c"
#include "../../box.c"
#include "../../daycare_pc.c"
#include "../../box_fs.c"
#include "../../plugin_fs.c"
#include "../../hud_gfx.c"
#include "../../pc_plugin.c"
