#pragma once
// SHOP scene (contract section 17): the mart. A framed white screen: "Comprar / Vender / Salir", the BUY list (name,
// price), the SELL list (the bag, tiny xN), a quantity picker and a YES/NO question (scene_ask_open). Lives in the MENU plugin on the
// device (ez_menu.fal, menu_host.h); the host harness links it in. Opened by the script native `shop ID` (natives_shop.c).
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"

#define SHOP_MAX_ITEMS 16 // shops.bin: at most 16 items per shop

extern const SceneOps scene_shop_ops;

// Pushes the shop screen for the stock `items[0..n)` (item ids, copied) and opens the greeting. False (nothing pushed) when the stack
// is full, a shop screen is already open, the stock is empty or the state cannot be allocated.
bool scene_shop_open(SceneCtx* c, const uint8_t* items, uint8_t n);
// The stock of shop `id` from shops.bin (section 17.1) into items[0..*n); cap = items capacity (>= 16). False (nothing written) when the file is missing or invalid, the id is out of range or
// the shop is empty (or a prize shop). Part of the MENU plugin since the C4 memory recovery (contract 38.3); the host tests call it directly.
bool shop_load(uint8_t id, uint8_t* items, uint8_t cap, uint8_t* n);
// The plugin's entry for the script native: shop_load(id) + scene_shop_open. False when either fails (nothing pushed).
bool scene_shop_open_id(SceneCtx* c, uint8_t id);
// True while the screen's state exists (the loader unloads the plugin once the party, bag and shop screens are all gone).
bool scene_shop_active(void);

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only): mode 0 menu, 1 buy list, 2 sell list, 3 quantity; list cursor (row index, last = Cancelar), first
// visible row, quantity, main-menu cursor; blocks the screen holds on the heap.
int scene_shop_mode(void);
int scene_shop_cur(void);
int scene_shop_top(void);
int scene_shop_qty(void);
int scene_shop_main_cur(void);
int scene_shop_last_msg(void); // text id of the last message dialog opened (gl.shop.* / gl.bag.empty)
typedef struct { // contract 72: where the last buy-list / quantity frame put each item name and its price (x of the price; names: x and drawn width)
    int n, nx[8], nw[8], px[8], full[8]; // the buy list's visible rows (full: the name's whole width)
    int qn, qpx;                // the quantity screen's name width (it starts at x 3) and price x
} ShopGeom;
extern ShopGeom g_sh_geom;
void scene_shop_heap(uint32_t* bytes, uint32_t* blocks);
#endif
