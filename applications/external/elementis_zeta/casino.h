#pragma once
// The casino plugin's internals (contract section 37): the Coin Purse rules, the slot machine's rules (reel strip, lines, pay table: the tests check them against the
// documented odds), and the two scenes. Plugin side + host tests. No furi.h.
#include <stdint.h>
#include <stdbool.h>
#include "casino_host.h"
#include "scene.h"

// ---- coins (casino.c). PartyState.coins, 0..COINS_MAX (9999); held only with the Coin Purse (the key item `estuche`, IT_ESTUCHE).
bool casino_has_case(void);
uint16_t casino_coins(void);
uint16_t casino_coins_add(
    uint16_t
        n); // returns how many were really added (the cap is COINS_MAX; without the case nothing is added)
bool casino_coins_take(uint16_t n); // false (nothing taken) when there are fewer

// ---- the slot machine's rules (scene_slots.c). Three reels with the SAME strip of SL_N symbols; each reel shows 3 rows (top = the symbol at its stop position `pos`, then
// pos + 1, pos + 2, wrapping). A bet of 1 coin plays the MIDDLE row; 2 coins: the three rows; 3 coins: the three rows + the two diagonals. Every line that
// pays pays the table amount whatever the bet (so the more lines, the better the value per coin). Each reel stops at an independent uniformly random strip position.
#define SL_N 18 // the strip: Zero 1, Vertice 2, Bolt 3, Crystal 3, Bell 4, Berry 5
#define SL_LINES \
    5 // 0 middle, 1 top, 2 bottom, 3 diagonal top-left -> bottom-right, 4 diagonal bottom-left -> top-right
enum {
    SY_BOLT = 0,
    SY_CRYSTAL,
    SY_ZERO,
    SY_BERRY,
    SY_BELL,
    SY_VERT,
    SY_COUNT
};
// What one line pays (coins): three equal symbols: Zero 300, Vertice 100, Bolt / Crystal / Bell 10, Berry 8; two Berries on the left two reels followed by anything else: 1.
#define PAY_ZERO   300
#define PAY_VERT   100
#define PAY_TRIO   10
#define PAY_BERRY3 8
#define PAY_BERRY2 1
extern const uint8_t SL_STRIP[SL_N];
int slots_line_pay(uint8_t a, uint8_t b, uint8_t c); // the three symbols of a line, left to right
uint8_t slots_lines_for_bet(
    int bet); // bitmask of the lines a bet plays (bit k = line k): 1 -> 0x01, 2 -> 0x07, 3 -> 0x1F
uint8_t
    slots_cell(const uint8_t pos[3], int reel, int row); // the symbol shown at (reel, row 0..2)
int slots_pay(
    const uint8_t pos[3],
    int bet,
    uint8_t*
        paying); // total coins of a spin; *paying = bitmask of the lines that paid (may be NULL)
uint8_t slots_stop_pos(uint32_t* rng); // a uniformly random stop position 0..SL_N-1 (party_rng)

// ---- scenes (plugin side; the host links them directly)
bool scene_slots_open(
    SceneCtx* c); // the slot machine (needs the case and >= 1 coin; the caller checks)
bool scene_slots_active(void);
bool scene_prize_open(
    SceneCtx* c,
    uint8_t
        shop_id); // the prize counter of shops.toml shop `shop_id` (a `coins = true` shop); false when the shop is not one / cannot be read
bool scene_prize_active(void);
extern const SceneOps scene_slots_ops, scene_prize_ops;
// A prize of the counter (shops.bin, contract 37.4): kind 0 = an item (id = items.bin id, level = how many), 1 = a creature (id = species, level = its level); the price in coins.
typedef struct {
    uint8_t kind, id, level;
    uint16_t price;
} Prize;
#define MAX_PRIZES 8
// Reads the prize record of shop `id` of shops.bin into out[0..*n) (out holds MAX_PRIZES). False (nothing written) for a missing / damaged file, an id out of range, an ordinary item shop or
// an entry that is not valid (kind, id out of range, level, price outside 1..COINS_MAX). Exposed so the tests can fuzz it.
bool prize_load(uint8_t id, Prize* out, uint8_t* n);

#ifndef NUCLEO_DEVICE
// Host test hooks. slots: mode (0 idle, 1 spinning, 2 win flash, 3 out of coins), bet, the three stop positions, the reel states (0 landed, 1 spinning, 2 stopping, 3 bouncing),
// the last win, the paying-lines mask; heap held. prize: cursor, entry count, entry k (kind 0 item / 1 creature, id, level, price).
int scene_slots_mode(void);
int scene_slots_bet(void);
int scene_slots_pos(int reel);
int scene_slots_rstate(int reel);
int scene_slots_win(void);
int scene_slots_lines(void);
int scene_prize_cur(void);
int scene_prize_count(void);
int scene_prize_entry(int k, uint8_t* kind, uint8_t* id, uint8_t* level, uint16_t* price);
int scene_prize_last_msg(void);
#endif
