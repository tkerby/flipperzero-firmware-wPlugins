#pragma once
// Scene stack (max MAX_SCENES). No furi.h (host-compilable); timing lives in game_loop.c.
//
// Scenes: TITLE, WORLD, DIALOG, MENU, FADE. Every stack change calls input_flush() (keys held
// across the change are ignored until released) and marks D_WORLD.
//
// Per frame the driver calls: scene_input() for each queued event (top scene only),
// scene_update() (top scene, then downward while the scene above does not pause_below), and
// scene_render() when dirty. Rendering: the topmost scene that has draw_world draws the world
// into lvl; then frame_compose(ctx->fade); then draw_overlay of that scene and every scene above
// it, bottom to top (unless ctx->hide_overlay); then publish.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "game.h"
#include "input.h"
#include "font.h" // tchar

enum {
    D_WORLD = 1,
    D_COMPOSE = 2
};
#define SCENE_NO_WAKE 0xFFFFFFFFu

struct SceneCtx;

typedef struct SceneOps {
    void (*enter)(struct SceneCtx* c, void* data);
    void (*leave)(struct SceneCtx* c, void* data); // owns freeing `data` if it was heap-allocated
    void (*input)(struct SceneCtx* c, void* data, const InputEv* ev);
    void (*update)(struct SceneCtx* c, void* data, uint32_t dt_ms);
    void (*draw_world)(struct SceneCtx* c, void* data); // render into c->frame->lvl
    void (*draw_overlay)(struct SceneCtx* c, void* data); // after frame_compose(): ov_* / font
    bool pauses_below; // scenes below are neither updated nor receive input
} SceneOps;

typedef struct {
    const SceneOps* ops;
    void* data;
} SceneEntry;

typedef struct SceneCtx {
    // (contract 77: the small hot fields come first: a byte / half-word load or store with an offset below 32 / 64 is a 16-bit Thumb instruction, above it a 32-bit one; `dirty` alone was 47 wide stores in the battle plugin)
    uint8_t n;
    uint8_t dirty; // D_* bits; cleared by scene_render()
    uint8_t fade; // 0..3, passed to frame_compose (set by the FADE scene)
    bool hide_overlay; // FADE scene: overlays hidden
    uint8_t held; // effective held keys, set by the driver before scene_update()
    uint8_t
        stone_to; // C4-B (contract 36): a stone was used from the bag on a creature that evolves with it: the species it becomes (checked by scene_party.c) ...
    uint16_t
        stone_evo; // ... and the hand-over: bits 15..8 the item (given back if the scene cannot open), 7 = 1, 2..0 the party slot; scene_menu.c's update opens the evolution scene
        // once the bag and party screens are gone (stone_to + stone_evo fit in the padding after `held`: SceneCtx does not grow)
    uint32_t wake_ms; // min ms until a scene wants another update (SCENE_NO_WAKE = none)
    Game* game;
    Frame* frame;
    struct World* world;
    SceneEntry st[MAX_SCENES];
} SceneCtx;

// (Contract 70: the V1 A/B switch of the DEV build - world-anchored vs screen-anchored dither, world frame cap from the start vs the end of the render - is gone: the "new" modes are the only ones.)

void scene_ctx_init(SceneCtx* c, Game* g);
bool scene_push(SceneCtx* c, const SceneOps* ops, void* data);
void scene_pop(SceneCtx* c);
void scene_replace(SceneCtx* c, const SceneOps* ops, void* data);
// Inserts a scene one place BELOW the top one (the top scene stays on top and keeps running). Use it
// to enter a scene from under a FADE: fade covers the screen, the new scene is already in place when
// the fade pops. `ops->enter` is called (the fade's own state is untouched; do not reset c->fade there).
bool scene_insert_below_top(SceneCtx* c, const SceneOps* ops, void* data);
void scene_clear(SceneCtx* c); // pops everything (leave called on each)

static inline void scene_dirty(SceneCtx* c, uint8_t flags) {
    c->dirty |= flags;
}
// Request an update() call within `ms` ms (call from update/enter; min over scenes is kept).
static inline void scene_wake(SceneCtx* c, uint32_t ms) {
    if(ms < c->wake_ms) c->wake_ms = ms;
}
static inline const SceneOps* scene_top_ops(const SceneCtx* c) {
    return c->n ? c->st[c->n - 1].ops : (const SceneOps*)0;
}

void scene_input(SceneCtx* c, const InputEv* ev);
void scene_update(SceneCtx* c, uint32_t dt_ms);
// Renders / composes / publishes according to c->dirty (no-op when clean). Returns true if a
// frame was published.
bool scene_render(SceneCtx* c);

// Scene implementations
extern const SceneOps scene_title_ops;
extern const SceneOps scene_world_ops;
extern const SceneOps scene_fade_ops;
extern const SceneOps scene_dialog_ops;
extern const SceneOps scene_choice_ops;
extern const SceneOps scene_menu_ops;
extern const SceneOps scene_lang_ops;
extern const SceneOps scene_options_ops;
extern const SceneOps scene_mon_ops;
extern const SceneOps scene_name_ops;
extern const SceneOps scene_list_ops; // C4-A: the elevator's floor list (UI plugin)
extern const SceneOps scene_kb_ops; // task K: naming keyboard
extern const SceneOps scene_nick_ops; // task K: nickname prompt

// Debug overlay (DEV builds, NUCLEO_DEBUG=1 ufbt ...; no-ops in the release build), shown while World.dbg_on (Left + Right + OK in the world): map /
// hero, R C F Q timings, heap F / M / m (free, largest block, minimum since boot, KB), game stack headroom. The WORLD
// scene calls these; the battle scene (a plugin, the world is not drawn under it) calls them too, so the numbers can be
// read during a battle. update: from the scene's update(); draw: from its draw_overlay().
void scene_debug_overlay_update(SceneCtx* c);
void scene_debug_overlay_draw(SceneCtx* c);
// Draws the world as it is now into lvl (what the WORLD scene's draw_world does), for scenes drawn over a frozen world.
void world_draw_frozen(SceneCtx* c);

// ---- B2 scene helpers (scene_fade.c / scene_dialog.c / scene_menu.c)
// Fade out -> load map `map` and put the hero on (ti,tj,facing) -> fade in. Stops the hero.
// Same map: the hero is only teleported. Pushes the FADE scene (returns false if the stack is full).
bool scene_fade_warp(SceneCtx* c, uint8_t map, uint8_t ti, uint8_t tj, uint8_t facing);
// Title -> world: fade out, world_new_game(), swap the title below for the world scene, fade in.
bool scene_fade_new_game(SceneCtx* c);
// Title -> world, "Continuar": same, but save_continue() restores the saved game (a save that has become invalid in the
// meantime starts a new game instead). Contract section 12.
bool scene_fade_continue(SceneCtx* c);
// Text box for text id `text_id` (bit 0x8000 = global). push_back < 0: nothing; else, when the box
// closes normally, the hero takes one forced step of that facing. Returns false (and pushes
// nothing) when the text cannot be loaded or the stack is full.
bool scene_dialog_open(SceneCtx* c, uint16_t text_id, int8_t push_back);
#ifndef NUCLEO_DEVICE
const tchar* scene_dialog_text(
    const void*
        state); // host tests: the text a dialog scene shows = its state's buffer (contract 77: the buffer is no longer the first member of the state, the tests ask here)
#endif
// Same, then a YES/NO box over the last page (scene_choice.c): `*out` = 1 yes / 0 no (Back = no).
// The dialog closes by itself when the choice is made. Returns false (nothing pushed) on failure.
bool scene_ask_open(SceneCtx* c, uint16_t text_id, uint8_t* out);
// The YES/NO box alone (pushed above whatever is on screen); *out is written when it closes.
bool scene_choice_open(SceneCtx* c, uint8_t* out);
// Script screen fades (scene_fade.c): FADE_OUT fades to white and HOLDS the white screen
// (World.fade_hold) after popping; FADE_IN fades back. Return false when the stack is full.
bool scene_fade_out(SceneCtx* c);
bool scene_fade_in(SceneCtx* c);
// Right-side pause menu (Equipo / Mochila / Guardar / Opciones / Salir).
bool scene_menu_open(SceneCtx* c);
// What "Equipo" does (the party screen, a plugin: menu_host.h; set by natives_init() to menu_open_team). Returns false
// (nothing opened) -> the menu falls back to the "Pronto..." dialog; NULL (host tests without the party) does the same.
extern bool (*scene_menu_team_hook)(SceneCtx* c);
// Re-reads the menu labels in the active language (the options screen calls it after a switch).
void scene_menu_reload(SceneCtx* c);
// The save screen (UI plugin) covers the menu box: it hides it while it is on (enter / leave).
void scene_menu_set_hidden(bool hidden);
// Loaded label k of the menu (END-terminated codes) or NULL (tests).
const tchar* scene_menu_label(int k);
#ifndef NUCLEO_DEVICE
size_t scene_menu_heap(
    void); // host tests: the heap block of the open pause menu (contract 69), 0 when it is closed
#endif

// ---- I2 (M1.5): languages and settings
// The UI plugin's screens (ui_host.h): on the device they live in ez_ui.fal and the app opens them through ui_open_*();
// scene_*_open() below are the plugin side (and what the host tests call directly).
// Title: gray bands, logo, "Pulsa OK" or the Continuar / Nueva partida list. Pushes the scene.
bool scene_title_open(SceneCtx* c);
// First-run language picker (shown instead of the title while settings.bin is missing / invalid).
// OK: stores the choice (settings.bin), fades out and swaps in the title. Pushes the scene.
bool scene_lang_open(SceneCtx* c);
// Options screen above the pause menu: "Idioma" row, Left / Right switches the language live.
bool scene_options_open(SceneCtx* c);
// Language picker -> title: fade out, swap the scene below for the title, fade in.
bool scene_fade_to_title(SceneCtx* c);

// ---- M2 (S2): lab content
// Elementi picture popup (scene_mon.c): a framed box with the species' front picture ABOVE the text box, drawn into
// the gray planes. The scene closes itself the first time it is on top again, i.e. when the dialog (and YES/NO
// box) opened AFTER it closed: push it, then open the dialog (SAY / ASK). Returns false (nothing pushed) when the
// picture cannot be loaded or the stack is full.
bool scene_mon_open(SceneCtx* c, uint8_t species);
// Contract 66: `who` 2 = the AFFINITY picker (Solar / Lunar): gl.aff.ask as a dialog, then a 2-row list (gl.aff.solar, gl.aff.lunar); OK stores PartyState.aff (and g_aff_choice, which save_io.c applies to a save
// being continued that has none yet: the title's Continuar prompt) and pops. The intro script asks it with `name_select 2` after the two names.
extern uint8_t g_aff_choice;
extern uint8_t
    g_peek_aff; // contract 66 (save_io.c): the PartyState.aff of the file the last save_peek / read_valid looked at (0 = an older save that has not chosen yet; SaveInfo is not extended: save_continue is resident)
// Name picker (scene_name.c): `who` 0 = the player, 1 = the rival. Shows gl.name.ask_player / ask_rival as a dialog over a plain
// background, then the list "NUEVO NOMBRE" (gl.kb.new) + the first three suggestions (gl.name.p0..p2 / r0..r2): OK on a suggestion stores
// it (party_set_name), OK on "NUEVO NOMBRE" opens the naming keyboard (scene_kb_open), whose stored result ends the scene; Back on the empty
// keyboard shows the list again. Back on the list does nothing (a name is mandatory). Returns false when the stack is full.
bool scene_name_open(SceneCtx* c, uint8_t who);
// C4-A (contract 35.5): the elevator's floor list (scene_card.c, PC plugin): a framed box at the top right with 2..5 rows whose texts are World.tr's text ids; Up / Down move, OK writes the row
// (0..n-1) to script variable `var` and pops (the script resumes), Back writes 255. Returns false when the stack is full / the row count is not 2..5.
bool scene_list_open(SceneCtx* c, uint8_t var);
#ifndef NUCLEO_DEVICE
bool scene_list_probe(
    const SceneCtx* c,
    int* n,
    int* sel,
    int* top,
    int* vis,
    const tchar** lab); // host tests: label r of the visible window is lab + r * scene_list_stride()
void scene_list_set_vis(uint8_t vis);
int scene_list_stride(void); // host tests: codes between two labels of the list's window (LIST_CAP)
#endif

// ---- task K: naming keyboard and nicknames (UI plugin, scene_kb.c; contract 11.6)
// naming keyboard (grid of the language's keys from kb.bin, name slots, page and end keys). kind 0 = the player's name, 1 = the
// rival's, 2 = the nickname of party slot `slot`. It stores the result itself (party_name_set / party_nick_set) when the player ends it
// with at least one character (a nickname may also end empty: back to the species name) and pops; Back on an empty line pops without
// storing. scene_kb_done() tells the scene below which of the two happened. Returns false when the stack is full / the table is missing.
bool scene_kb_open(SceneCtx* c, uint8_t kind, uint8_t slot);
bool scene_kb_done(void);
// "Do you want to give a nickname to {v0}?" YES / NO, then the keyboard for party slot `slot`; the creature's display name is left in
// {v0} afterwards. Pops itself and wakes the scene below (the world / a script waiting in native 21).
bool scene_nick_open(SceneCtx* c, uint8_t slot);
#ifndef NUCLEO_DEVICE
// Host tests (keyboard_check): the keyboard on top of the stack. probe: false when it is not; the cursor state and the typed codes. goto: puts
// the cursor on grid cell (grow, col) of `page` (grow < 0: on command `col` of the command row), as if the keys had been pressed.
bool scene_kb_probe(
    const SceneCtx* c,
    int* page,
    int* vrow,
    int* col,
    int* ci,
    int* len,
    const tchar** txt);
void scene_kb_goto(SceneCtx* c, int page, int grow, int col);
#endif
