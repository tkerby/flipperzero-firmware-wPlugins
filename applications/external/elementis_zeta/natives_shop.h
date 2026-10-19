#pragma once
// The `shop ID` script native (id 12, contract section 17). No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
struct World;

enum {
    NAT_SHOP = 12,
    NAT_PC = 22
}; // 22 (M5-C, contract 25): `pc` = the PC box screen, no operands. With a first operand != 0 (C4-C, contract 37) it is the CASINO native: a = op << 4 | VAR
    // (CAS_BUY 1 `coins_buy VAR`, CAS_SLOTS 2 `slots VAR`, CAS_PRIZES 3 `prizes ID VAR`, CAS_GET 4 `coins_get VAR`; casino_host.h), b = the operand.
    // IR LINK (contract 45): ops 9 `link trade VAR`, 10 `link battle VAR TRAINER`, 11 `link selftest VAR`, 12 the evolution after a trade (link_host.h LK_OP_*).

// The two natives, in the native table of natives.c (C3-A: the table is const, nothing registers them). The scripts reach them as native 12 / 22 (22 is also the casino, see above).
int nat_shop(struct World* w, uint8_t a, uint8_t b);
int nat_pc(struct World* w, uint8_t a, uint8_t b);
// C5-B (contract 43): the Reserva Salvaje's per-step hook (the ONE line world.c needs: `if(reserva_on_step(w)) return true;` in on_step_arrival, before the encounter roll). True = a scene
// took over (time ran out / no crystals left). Ops 5 .. 8 of native 22: see casino_host.h (`reserva_start VAR`, `reserva_end VAR`, `reserva_left VAR` = native pc 80 / 96 / 112 + VAR).
bool reserva_on_step(struct World* w);
#ifndef NUCLEO_DEVICE
void natives_shop_init(
    void); // host tests only: registers both in the VM's table (natives_init() installs the whole table anyway)
#endif
// shop_load (the stock of a shop from shops.bin) lives in the menu plugin since the C4 memory recovery (contract 38.3): scene_shop.h, included here so the host tests keep finding it.
#include "scene_shop.h"
