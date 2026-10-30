#pragma once
// PC screen (contract section 25): the "PC". Opened by the script native `pc` (natives_pc, id 22): "¡Encendiste el PC!", then
// the menu Sacar / Dejar / Liberar / Cambiar caja / Dejar obj. / Sacar obj. / Salir over twelve boxes of 20 creatures and the item storage (box.h). Leaving saves the game and the boxes together.
// Lives in the PC plugin on the device (ez_pc.fal, pc_host.h); the host harness links it in.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"

extern const SceneOps scene_box_ops;

// Pushes the PC screen (and its greeting). False (nothing pushed) when the stack has no room for the dialogs above it, a PC screen is already open, or
// the working copy of the boxes cannot be made (card error / no heap).
bool scene_box_open(SceneCtx* c);
// The pending capture (party.h) into the current box, or the next one with room, and the game is saved; shows "¡X fue enviado a la Caja N!" (or the
// all-boxes-full / save-failed message) and closes by itself. False (nothing pushed, the creature stays pending) when none is pending or the stack
// is full or the boxes cannot be opened.
bool scene_box_store_open(SceneCtx* c);
// True while the screen's state exists (the loader unloads the plugin once the PC, card and store screens are all gone).
bool scene_box_active(void);

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only): mode 0 menu, 1 Sacar list, 2 Dejar list, 3 Liberar list, 4 Cambiar caja, 5 the store scene waiting on its message, 6 / 7 the item lists Dejar obj. / Sacar obj., 8 the quantity box;
// menu cursor / list cursor (row index, last = Cancelar) / first visible row; the id of the last message dialog opened; blocks the state holds on the heap.
int scene_box_mode(void);
int scene_box_mcur(void);
int scene_box_cur(void);
int scene_box_top(void);
int scene_box_qty(void); // ... and the quantity box's number
int scene_box_nmenu(void); // the main menu's rows (6, or 7 with the Hall of Legacy)
int scene_box_last_msg(void);
void scene_box_heap(uint32_t* bytes, uint32_t* blocks);
#endif
