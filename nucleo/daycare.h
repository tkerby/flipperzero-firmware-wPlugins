#pragma once
// Day care (C3, contract section 33; Route 5 day care). ONE creature can be left with the day-care lady; it gains 1 exp per completed
// hero step (learning level-up moves on the way, the newest replacing the oldest when it knows 4), is stored in save.bin and comes
// back for 100 + 100 x levels gained. Implemented in daycare.c (C3 wave 1); world.c calls daycare_on_step, the script natives
// `daycare_*` (natives.c) call the rest. signatures fixed by the contract: change them only through the contract.
#include <stdbool.h>
#include <stdint.h>
#include "party.h"
#include "pc_host.h"

// Contract 65 (zero resident growth): the six one-line wrappers below were functions of daycare.c (92 B of resident code with their calls); they are header-inline now (their one caller, natives.c, expands them), which paid for
// the walking friendship of daycare_on_step. The day care's work itself is daycare_pc.c (the PC plugin), reached through menu_open_daycare.
static inline bool daycare_occupied(void) { return party_state()->dc.mon.species != 0; } // a creature is in care
static inline bool daycare_deposit(uint8_t party_slot) { return menu_open_daycare(DC_DEPOSIT, party_slot); } // moves party[slot] into care; false if care is full, the slot is empty or it is the last creature
static inline uint8_t daycare_levels_gained(void) { return (uint8_t)menu_open_daycare(DC_GAINED, 0); }        // levels gained since deposited (0 when empty)
static inline uint32_t daycare_price(void) { return menu_open_daycare(DC_PRICE, 0); }                          // 100 + 100 x levels gained (0 when empty)
static inline bool daycare_withdraw(void) { return menu_open_daycare(DC_WITHDRAW, 0); }                        // moves it back to the party (the caller charged the price); false if empty or the party is full
static inline uint8_t daycare_species(void) { return party_state()->dc.mon.species; }                          // species in care (0 when empty), for the lady's texts
void daycare_on_step(void);               // one completed hero step: +1 exp to the creature in care (no-op when empty) and +1/256 friendship to the lead creature (contract 65); cheap, no SD access
