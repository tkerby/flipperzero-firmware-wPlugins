// Unity build of the battle plugin (ez_battle.fal): ONE translation unit, compiled with -fno-function-sections and
// -fwhole-program (application.fam, fap_private_libs). Why: with the SDK's -mlong-calls every call is `ldr rX, =func; blx rX`
// plus a literal-pool word plus a load-time relocation, and with -ffunction-sections even a call to a static function of the
// same file is one; in a single section the compiler calls its own functions with a plain `bl` (no literal, no relocation) and,
// seeing the whole program, inlines / folds more. The loader holds all relocation data in RAM while it loads, so every relocation
// left out is a few bytes less at the heap's low point (contract 10.7). Measured: .text 33,114 -> 31.7 KB, relocations 416 -> 188.
// The sources are unchanged (the host harness still compiles them one by one); only the entry point is marked externally_visible.
#include "../../blit_fx.c"
#include "../../battle_gfx.c"
#include "../../battle_fx.c"
// The scene (scene_battle.c: the state machine, menus, messages: b_update / draw_overlay / start_turn glue) and the turn engine (battle_run.c: once per turn) are not per-pixel code, so they get
// size flags through #pragma GCC optimize: the seven sub-flags of application.fam's SMALL7 (contract 27.4 / 38.4: -fno-thread-jumps alone is -300 B on b_update's switch) and, since C5-B
// (contract 43.7), five more found by a sweep of every GCC `-fno-*` size flag over the plugin: -fno-tree-forwprop (-388 B alone), -fno-reorder-blocks (-100), -fno-ipa-sra (-50),
// -fno-ipa-pure-const and -fno-tree-fre (-60 together). Measured on the plugin: 26,579 (the Reserva code, the old flags) -> 25,791 B; the old build without the Reserva was 26,010.
// Contract 44 added seven more, found by a greedy sweep over the whole unit with the region's flags already on: -fno-caller-saves, -fno-inline-small-functions, -fno-early-inlining, -fno-tree-loop-im,
// -fno-tree-ccp, -fno-ira-hoist-pressure, -fno-tree-coalesce-vars (25,795 -> 25,539 B, -256, on the .o).
// Contract 67 added -fno-ssa-phiopt and -fno-move-loop-invariants (a sweep of the 45 flags that were still on: -36 B together; every other flag was neutral or bigger).
// Contract 73 added -fno-tree-copy-prop, -fno-shrink-wrap and -fno-tree-pta (tools/flag_sweep.py over the unit: -60 B; the same flags on the painters were +88).
// The per-pixel files (blit_fx, battle_gfx, battle_fx, hud_gfx) keep the plain flags.
#pragma GCC push_options
#pragma GCC optimize("no-tree-tail-merge,no-schedule-insns2,no-if-conversion,no-optimize-sibling-calls,no-thread-jumps,no-tree-dominator-opts,no-tree-forwprop,no-reorder-blocks,no-ipa-sra,no-tree-fre,no-inline-small-functions,no-early-inlining,no-tree-loop-im,no-tree-ccp,no-ira-hoist-pressure,no-tree-coalesce-vars,no-ssa-phiopt,no-move-loop-invariants,no-tree-copy-prop,no-shrink-wrap,no-tree-pta,no-tree-slsr,no-ipa-modref,no-ipa-strict-aliasing,no-tree-scev-cprop,no-forward-propagate,no-aggressive-loop-optimizations")
#include "../../battle_run.c"
#include "../../scene_battle.c"
#pragma GCC pop_options
#include "../../hud_gfx.c"
#include "../../battle_plugin.c"
