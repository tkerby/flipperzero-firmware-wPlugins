#pragma once
// Bag, money and the Registro (the Registro bits). All of it lives in PartyState (party.h) so the save stores it with the
// party. Resident in the app (the battle plugin reaches it through the symbol table). No furi.h (host-compilable).
// Item ids are items.bin ids (reg_gen.h IT_*). Contract section 14.
#include <stdint.h>
#include <stdbool.h>

// ---- bag: at most BAG_MAX_KINDS different items, BAG_MAX_COUNT of each
uint8_t bag_count(uint8_t item);               // how many of `item` (0 = none / unknown)
uint8_t bag_add(uint8_t item, uint8_t n);      // returns how many were really added (cap 99 per item, 20 kinds)
bool bag_take(uint8_t item);                   // removes one; false when there is none. An emptied slot is removed.
uint8_t bag_kinds(void);                       // number of different items held (iteration: 0 .. bag_kinds() - 1)
bool bag_get(uint8_t i, uint8_t* item, uint8_t* count);

// ---- money. Shared by the battle (prize money), the shop and the world (blackout, natives).
#define MONEY_MAX 999999u
uint32_t money_get(void);
uint32_t money_add(uint32_t n);                // returns how much was really added (cap MONEY_MAX)
bool money_take(uint32_t n);                   // false (and nothing taken) when there is not enough
uint32_t money_lose_half(void);                // blackout: loses half (rounded down), returns the amount lost

// ---- Registro: species 1..255 (0 is ignored)
void reg_set_seen(uint8_t species);
void reg_set_caught(uint8_t species);          // also marks it seen
bool reg_seen(uint8_t species);
bool reg_caught(uint8_t species);
uint8_t reg_caught_count(void);
uint8_t reg_seen_count(void);

// ---- debug (DEV builds, NUCLEO_DEBUG=1 ufbt ...; the world scene's Left + Right + Up combo while the debug overlay is on): 5 Crystals
// and 2 Salves. Returns true when something was added.
bool bag_debug_give(void);
