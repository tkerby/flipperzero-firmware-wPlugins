#pragma once
// BATTLE scene (contract section 10): battle flow with the hybrid composition (normal view + focus shots),
// code-driven animations and per-type effects. Drives battle_core.h; natives 4 / 5 open it (natives.c).
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "battle_core.h"
#include "battle_host.h" // BATTLE_* results

extern const SceneOps scene_battle_ops;

// Trainer battle: the enemy party comes from trainers.bin (bc_trainer_party with the player's starter, so the rival's
// counter pick agrees with the Crystal he took). Pushes the BATTLE scene (transition, intro, menus ...) and returns true;
// false (nothing pushed) when the data / heap / stack is missing. When the battle ends `*result_out` (a script
// variable, may be NULL) receives BATTLE_WON / BATTLE_LOST and the scene fades back to the world.
// Wild battle (entry point for M3: no trigger exists yet): a single enemy of `species` / `level`, no trainer
// pictures, "run" allowed; `*result_out` also gets BATTLE_RAN when the player escaped.
// The transition flashes / wipes the world's last frame, which the caller (battle_open.c) left in `c->frame->lvl` before it
// freed the map's sprite bank (no copy). When the battle ends the scene calls battle_scene_closed() (battle_host.h).
bool scene_battle_trainer(SceneCtx* c, uint8_t trainer, uint8_t* result_out);
bool scene_battle_wild(SceneCtx* c, uint8_t species, uint8_t level, uint8_t* result_out);

// ---- test hooks (host harness only; the device build defines NUCLEO_DEVICE and leaves them out)
const Battle* scene_battle_core(
    void); // NULL when no battle is on (the plugin entry uses it on the device too)
#ifndef NUCLEO_DEVICE
typedef enum {
    BUI_TRANSITION = 0,
    BUI_RUNNING,
    BUI_MENU,
    BUI_FIGHT,
    BUI_EXIT,
    BUI_BAG,
    BUI_PARTY
} BattleUiMode;
BattleUiMode scene_battle_mode(void);
// Summary of the allocations the scene holds right now (bytes, malloc'd blocks): pictures, platforms, Battle.
void scene_battle_heap(uint32_t* bytes, uint32_t* blocks);
// Drawn view of the last frame: 0 normal, 1 focus, 2 intro, 3 black; the focused side in *side.
int scene_battle_view(int* side);
int scene_battle_shiny(
    int side); // contract 65: the fighter of `side` (0 player, 1 enemy) is drawn as a variocolor (1 / 0, -1 no battle)
int scene_battle_pic_ok(
    int side); // contract 65: its medium picture is the species' own picture, tones swapped exactly when it is a variocolor (1 / 0, -1 nothing to compare)
extern unsigned g_sb_sparkles; // contract 65: the variocolor sparkles queued so far
typedef struct { // contract 72: the move list's geometry as draw_fight last drew it (the movescan scenario and `?fightgeom` check it)
    int vert, bx, by, bw, bh, cw, ix, iy, iw, ih;
    int nx[4], ny[4], nw[4]; // each drawn name: its x, y, width
    int drawn, sel_drawn; // bit k: slot k's name was drawn; the selected slot's name was drawn
} FightGeom;
extern FightGeom g_sb_fg;
int scene_battle_cpy(
    void); // contract 77 (host tests): Echoing is choosing (the move list shows the foe's moves) 1 / 0, -1 no battle
int scene_battle_drawn(
    int side); // contract 85: the fighter of `side` is drawn (not faint, not hidden by Dig / Fly), -1 no battle
int scene_battle_fsp(
    int side); // contract 77: the species the fighter of `side` is drawn as (Transform changes it), -1 no battle
extern char g_sb_ppstr[24]; // contract 77: the "PP 20/28" text draw_fight built last
int scene_battle_ecl(
    int what); // contract 67 (the Eclipse Form): 0 the chip is offered, 1 the cursor is on it, 2 the form was taken in this battle, 3 / 4 the form of the player's / the enemy's creature (the core), 5 / 6 the picture form of each fighter (-1 no battle)
int scene_battle_anim(
    int* t,
    int* dur); // the running animation: its kind (0 none), ms into it and its length
int scene_battle_ask(
    void); // the forget-a-move question showing (typed): 0 none, 1 forget? (cursor on YES), 2 forget? (NO), 3 stop learning? (YES), 4 stop learning? (NO), 5 change creatures? (YES), 6 (NO) (contract 75)
int scene_battle_hp(
    int side,
    int* hp,
    int* hp_to); // contract 75: the HP the plate of `side` shows and the value it drains to; returns 1 when that creature is on screen, 0 hidden, -1 no battle
int scene_battle_typed(
    void); // a message is on screen and fully typed (the sheets' snapshots wait for it)
int scene_battle_learning(void); // the move id waiting to be learned (the dialog is on), 0 none
int scene_battle_uses(
    void); // how many "X used Y!" lines have started (the animation dumps stop before the second one)
void scene_battle_dump(void); // prints the runner state (debugging)
uint32_t scene_battle_state_size(
    void); // sizeof the heap-allocated scene state (the Battle struct is separate)
const tchar* scene_battle_label(
    int k); // C5-B: the text of the battle menu's entry k (0..3, the 2 x 2 grid): Fight / Bag / Party / Run, or Crystal xN / Rock / Bait / Run in the Reserva Salvaje
extern void (*scene_battle_msg_hook)(
    const tchar*
        buf); // every message that starts (its codes; {vN} is expanded by text_expand at that moment)
#endif
