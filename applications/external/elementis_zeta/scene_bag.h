#pragma once
// BAG scene (contract section 15): the pause menu's "Mochila", the START -> ITEM flow: the item list (name, tiny xN, Cancelar),
// OK -> box Usar / Tirar / Cancelar, Usar = a healing item opens the party list as its target (scene_party_open_use) and a
// catch item says it is not the time, Tirar = quantity picker + YES/NO. Lives in the MENU plugin on the device (ez_menu.fal,
// menu_host.h); the host harness links it in.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"

extern const SceneOps scene_bag_ops;

// Pushes the bag screen. False (nothing pushed) when the stack is full, a bag screen is already open or the state cannot be
// allocated.
bool scene_bag_open(SceneCtx* c);
// True while the screen's state exists (the loader unloads the plugin once the party screen and this one are both gone).
bool scene_bag_active(void);

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only): mode 0 list, 1 Usar / Tirar box, 2 quantity; list cursor (row index, kinds = Cancelar), first
// visible row, quantity; blocks the screen holds on the heap.
int scene_bag_mode(void);
int scene_bag_cur(void);
int scene_bag_top(void);
int scene_bag_qty(void);
int scene_bag_rows(void); // C3-A: rows of the list = item kinds + owned bracelet modules + Cancelar
int scene_bag_row_item(
    int row); // the item id of a row (0 = Cancelar, 0xE0 + k = bracelet module k)
void scene_bag_heap(uint32_t* bytes, uint32_t* blocks);
#endif
