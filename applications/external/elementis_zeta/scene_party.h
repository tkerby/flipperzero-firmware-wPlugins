#pragma once
// PARTY scene (contract section 11): the START -> creature flow of the pause menu's "Equipo": the party list, the
// Datos / Cambiar / Cancelar option box and the summary pages of one Elementi. Lives in the MENU plugin on the device (ez_menu.fal,
// menu_host.h); the host harness links it in.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"

extern const SceneOps scene_party_ops;

// Pushes the party screen (list) on top of the stack. False (nothing pushed) when the stack is full, a party screen is
// already open or the state cannot be allocated.
bool scene_party_open(SceneCtx* c);
// The same list as the target of a healing item `item` (items.bin id), opened by the bag screen above it: OK on a row uses
// the item (refused on a fainted / full-HP creature: gl.bt.no_effect, the item is kept), the HP bar drains up to the new
// value, gl.bt.healed, then the screen closes; Back closes it without using anything.
bool scene_party_open_use(SceneCtx* c, uint8_t item);
// C3-A (contract 32): the list as a picker for a script (native 23 `pick`). mode 0: OK on a row writes its slot into *out; mode 1: the in-game trade (World.tr via
// c->world: the chosen creature must be the wanted species, it is replaced by the given species at the same level with the fixed nickname; *out = 1 traded / 0 wrong
// species). Back writes 255. The screen closes after the choice (OK) or Back.
bool scene_party_open_pick(SceneCtx* c, uint8_t mode, uint8_t* out);
// True while the screen's state exists (between scene_party_open and the scene's leave): the loader unloads the plugin
// once it is false.
bool scene_party_active(void);
// The Exit Wisp's effect (contract 29; the bag's Usar on a rope and, since contract 81, the field move Dig): every scene above the world closes (the caller's own state is freed: touch nothing of it afterwards) and the fade
// takes the hero to the last Centro (world_go_respawn).
void scene_escape(SceneCtx* c);

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only): screen mode 0 list, 1 option box, 2 summary, 3 Cambiar (pick the second row); summary page 0..2; cursor / summary mon / first visible row;
// bytes and blocks the screen holds on the heap right now (state + picture).
int scene_party_mode(void);
int scene_party_page(void);
int scene_party_sel(void);
int scene_party_cur(void);
int scene_party_top(void); // list: first visible row
void scene_party_heap(uint32_t* bytes, uint32_t* blocks);
int scene_party_opt(void); // contract 81: the option box's cursor row (the field moves come first)
int scene_party_nfm(void); // ... how many field moves (Dig / Teleport / Hearty Broth) it lists
void scene_party_box(int* x, int* y, int* w, int* h); // ... its rectangle
#endif
