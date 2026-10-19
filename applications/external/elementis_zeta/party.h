#pragma once
// Party and game state (contract section 9): the player's party, the starters, the chosen names and the
// battle data (species / moves / trainers) loaded once. No furi.h (host-compilable; files go through read_at()).
//
// Everything the M3 save has to store lives in ONE plain struct, PartyState (no pointers, fixed size):
// copy it into / out of save.bin verbatim (record sizeof(PartyState) + PARTY_STATE_VER in the save header). Names (task K) are Unicode in it.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "battle_core.h"
#include "font.h" // tchar

#define PARTY_STATE_VER 5
// 2 (M3-C): + money, the bag and the Registro bits (appended; the v1 prefix is unchanged); 3 (task K): + the names / nicknames as Unicode;
// 4 (C3, contract 33): + the day care (one creature, appended; the v3 prefix is unchanged);
// 5 (C4-C, contract 37): + the casino coins (appended; the v4 prefix is unchanged)
#define PARTY_STATE_V2_SIZE \
    388 // sizeof(PartyState) at version 2 (the save.bin of format 1 holds exactly that; its prefix is unchanged in v3 and v4)
#define PARTY_STATE_V3_SIZE \
    552 // ... at version 3 (the save.bin of format 2; its prefix is unchanged in v4)
#define PARTY_STATE_V4_SIZE \
    620 // ... at version 4 (the save.bin of format 3; its prefix is unchanged in v5)
#define AFF_NONE \
    0 // contract 66: PartyState.aff (the version split inside one game: nucleo/affinity.h, assets/data/affinity.toml)
#define AFF_SOLAR       1
#define AFF_LUNAR       2
#define COINS_MAX       9999u // the casino coins (contract 37): cap
#define PARTY_NAME_NONE 0xFF // name_player / name_rival: not chosen yet
#define PARTY_NAME_CUSTOM \
    0xFE // typed on the keyboard (0..3 = the suggested name of that index, which is also in name_u)
#define PARTY_NAME_CAP 8 // player / rival name: 7 characters + END
#define PARTY_NICK_CAP \
    11 // creature nickname: 10 characters + END; the per-language limits are LANG_MAX_NAME / LANG_MAX_NICK (gen/langs_gen.h)

#define FLAG_BADGE_FIRST \
    80 // M5-C (contract 25.4): world flags 80..87 = the eight badges (the trainer card shows slot k filled when flag FLAG_BADGE_FIRST + k is set;
#define BADGE_COUNT \
    8 // a script grants one with `setf` on the number, or by a name declared as 80 + k)

#define FLAG_DEV_SHINY \
    819 // contract 65: DEV / host builds only (NUCLEO_DEBUG or the host): while this story flag is set every wild creature is made a variocolor (Def = Spd = Spc = 10); a release build never reads it. 800 .. 819 are B1's range

#define BAG_MAX_KINDS 20 // different items the bag can hold
#define BAG_MAX_COUNT 99 // of each
#define REG_BYTES     32 // 256 species, one bit each

typedef tchar
    NickRow[PARTY_NICK_CAP]; // one nickname as codes of the active language (END-terminated)

typedef struct {
    uint8_t item; // item id (items.bin), 0 = empty slot
    uint8_t count; // 1..BAG_MAX_COUNT
} BagSlot;

// The day care (C3, contract 33, daycare.h): ONE creature left with the lady. species 0 = empty. The Mon is stored as it was DEPOSITED except for `exp`, which counts up one
// per step (daycare_on_step); its level / stats / moves are settled lazily when it is withdrawn (daycare.c). The nickname travels with it (Unicode, like nick_u).
typedef struct {
    Mon mon;
    uint16_t nick_u[PARTY_NICK_CAP];
    uint8_t pad[2];
} DayCare;

typedef struct {
    Party party; // the player's party (Mon = battle_core.h)
    uint8_t
        name_player; // v1/v2: index 0..3 of the chosen preset (gl.name.p0..p3); v3: that index when a suggested name was picked, PARTY_NAME_CUSTOM
    // when typed, PARTY_NAME_NONE = not chosen (the text is in name_u either way)
    uint8_t name_rival; // same for gl.name.r0..r3
    uint8_t starter; // species of the player's starter (0 none)
    uint8_t rival_starter; // species the rival took (0 none)
    uint8_t rv_map, rv_ti, rv_tj,
        rv_face; // C5-B (contract 43): the Reserva Salvaje session's way home (the gate: map, tile, facing); was `reserved[4]` (always 0 in every older save)
    // ---- v2 (M3-C), see bag.h
    uint32_t money;
    BagSlot
        bag[BAG_MAX_KINDS]; // the first n_bag slots are used, in the order the items were first added
    uint8_t n_bag;
    uint8_t
        aff; // B2 (contract 66): the SOLAR / LUNAR affinity (AFF_*; 0 = not chosen yet). It was `pad[3]` (always 0 in every older save): PartyState and the save format did not change
    union {
        uint16_t
            gid; // contract 81: this game's id (a new game draws one, never 0; 0 = a save of an older build): the "Guardar" screen asks before it overwrites a save file that holds ANOTHER game ("There is already a saved game"). It was `pad[2]` (always 0 in every older save): PartyState / the save format did not change
        uint8_t pad[2];
    };
    uint8_t reg_seen[REG_BYTES]; // Registro: bit (species & 7) of byte (species >> 3)
    uint8_t reg_caught[REG_BYTES];
    // ---- v3 (task K, contract 9.2): the names as UNICODE (u16, 0 = END; ASCII is itself), so a save survives a font rebuild and a language
    // switch (the font codes of the active language are derived: party_apply_names). Empty = no name / no nickname.
    uint16_t name_u[2][PARTY_NAME_CAP]; // [0] player, [1] rival
    uint16_t nick_u
        [BC_MAX_PARTY]
        [PARTY_NICK_CAP]; // per party slot (swapped with the creature); slots >= party.count are empty. Stored creatures
    // (PC boxes, later) carry the same uint16_t[PARTY_NICK_CAP] field.
    // ---- v4 (C3, contract 33): the day care
    DayCare dc;
    // ---- v5 (C4-C, contract 37): the casino coins (0..COINS_MAX; only held with the Coin Purse, casino.h)
    uint16_t coins;
    union {
        uint16_t
            rv_steps; // C5-B (contract 43): the Reserva Salvaje steps left, 0 = not inside; these 2 bytes were `pad2[2]` (always 0 in every older save). PartyState / the save format did not change
        uint8_t pad2[2]; // (the old name: casino_check.c still reads it)
    };
} PartyState;
_Static_assert(
    offsetof(PartyState, name_u) == PARTY_STATE_V2_SIZE,
    "the v2 prefix of PartyState must not move (old saves)");
_Static_assert(
    offsetof(PartyState, dc) == PARTY_STATE_V3_SIZE,
    "the v3 prefix of PartyState must not move (old saves)");
_Static_assert(
    offsetof(PartyState, coins) == PARTY_STATE_V4_SIZE,
    "the v4 prefix of PartyState must not move (old saves)");
_Static_assert(sizeof(DayCare) % 4 == 0, "DayCare is saved verbatim");
_Static_assert(
    sizeof(PartyState) % 4 == 0,
    "PartyState is saved verbatim: keep its size a multiple of 4");

// Loads reg.bin / moves.bin / trainers.bin (about 1 KB of heap) through read_at(), clears the state and seeds the
// creature RNG (any value; the firmware passes furi_get_tick()). Returns false when the data cannot be loaded
// (the getters then return NULL / 0 and every function below is a safe no-op). Call once at game start.
bool party_init(uint32_t seed);
void party_shutdown(void); // frees the data (game thread end)
// New game: empty party, no names, no starters (the data stays loaded). Called by world_new_game() through
// world_new_game_hook (installed by natives_init()).
void party_new_game(void);

const BcData* party_data(void); // loaded battle data, NULL if party_init failed
// Post-M5 (contract 27): the tables are no longer released across the battle plugin's load (the species / move / trainer records are read on demand
// through small caches: 0.8 KB of heap in all, was 1.9 KB in six blocks), so there is nothing to give back and read again.
// The type chart (246 B) is only read by the battle: battle_open.c loads it right after the battle plugin (false: the battle does not open) and frees it
// when the plugin is gone again or the open failed, so it is not in the exploring heap (contract 27).
bool party_chart(bool load);
PartyState* party_state(void); // the save-able state (never NULL)
Party* party_player(void); // &party_state()->party
uint32_t*
    party_rng(void); // the shared creature RNG (bc_mon_create / bc_trainer_party / bc_init seeds)
uint8_t party_starter(void); // species of the player's starter, 0 = none
uint8_t party_rival_species(void); // species the rival took (counter of the starter), 0 = none yet

// Adds a creature (random DVs, moves for that level) to the party. Returns its slot or -1 (party full,
// bad species, data not loaded). The first creature added becomes the starter when none is set. It is registered
// as seen + caught in the Registro (bag.h reg_set_caught).
int party_give(uint8_t species, uint8_t level);
// The species that beats `mine` according to the rival trainer's counter table (trainers.bin counter_from /
// counter_to of TR_RIVAL_LAB: fire -> water -> plant -> fire is "the rival takes the counter"); stores it as the
// rival's starter and returns it (0 when `mine` is not in the table).
uint8_t party_rival_pick(uint8_t mine);
// Full HP / PP / status for every creature of the party (battle helper; the heal_party native 5 may call it).
void party_heal_all(void);

// ---- names (task K, contract 9.2). Stored as Unicode (PartyState.name_u / nick_u); the font codes of the ACTIVE language are derived.
// The suggested names are the global texts gl.name.p0..p2 / r0..r3, so they follow the language.
// Derives the codes of the active language: text_set_names() for {player} / {rival} and the nickname cache (names.c in the UI plugin, which this
// loads when something has to be converted). Cuts every name to the language's limit (gen/langs_gen.h LANG_MAX_NAME / LANG_MAX_NICK), a character
// the language has no code for becomes '?', PartyState is sanitised (END inside the caps, no nickname beyond the party). Run after a language
// change (text_lang_hook), a load of the save and every change of a name (the functions below do).
void party_apply_names(void);
// The nickname of party slot `slot` as codes of the active language, NULL = none (show the species name).
const tchar* party_nick(uint8_t slot);
// The name to show for a party slot: the nickname, else the species name. Codes into out (cap incl. END); returns the length (0 on error).
int party_mon_name(uint8_t slot, tchar* out, int cap);
// The nickname cache (codes of the active language, one row per party slot; names.c fills it, the party screen swaps rows with the creatures).
NickRow* party_nick_cache(void);
// UI plugin side (names.c; the host links it directly). Each ends with the equivalent of party_apply_names().
void party_set_name(
    bool rival,
    uint8_t idx); // stores suggested name idx (0..3) of the active language
void party_name_set(
    bool rival,
    const uint16_t* u); // a typed name (Unicode, END-terminated, cut to PARTY_NAME_CAP - 1)
void party_nick_set(
    uint8_t slot,
    const uint16_t*
        u); // nickname of party slot `slot` (cut to PARTY_NICK_CAP - 1); empty = back to the species name
void party_migrate_names(
    void); // v2 -> v3 (save_io.c): the preset indexes of an old save become name strings, in the active language

// ---- M5-C (contract section 25): the captured creature that did not fit in the party ("pending"). sends it to the PC box at once; here the
// battle plugin parks it in this slot (it cannot touch the box: the box code lives in the PC plugin) and, once the world is back, the PC code
// (scene_box_store_open) moves it into the current box (or the next one with room) and clears the slot. One slot only: a second capture is
// refused by the battle while one is pending. The nickname is kept apart as UNICODE (like PartyState.nick_u; empty = the species name) so the
// post-battle nickname step can fill it. Not part of save.bin (it is stored within the same frames in which it is captured; a failed store keeps it
// in RAM and the next PC session stores it).
// M5 memory recovery (contract 25.6): the slot is a plain struct (g_pending, party.c) that the plugins reach through the symbol table as a VARIABLE, and the five
// accessors are inline: five functions (and five table entries) in the resident app became ~20 B per use in the code that needs them.
typedef struct {
    Mon mon;
    uint16_t nick[PARTY_NICK_CAP];
    bool on;
} PartyPending;
extern PartyPending g_pending;
static inline void party_pending_clear(void) { // slot empty, nickname empty
    memset(&g_pending, 0, sizeof(g_pending));
}
static inline bool party_pending_get(Mon* out) { // true when one is pending (out may be NULL)
    if(g_pending.on && out) *out = g_pending.mon;
    return g_pending.on;
}
static inline bool party_pending_set(
    const Mon* m) { // false when one is already pending (nothing changes); the nickname starts empty
    if(g_pending.on || !m) return false;
    g_pending.mon = *m;
    g_pending.nick[0] = 0;
    g_pending.on = true;
    return true;
}
static inline void party_pending_nick_set(
    const uint16_t*
        u) { // nickname of the pending creature (Unicode, END-terminated, cut to PARTY_NICK_CAP - 1; NULL / empty = none)
    uint8_t i = 0;
    for(; u && u[i] && i < PARTY_NICK_CAP - 1; i++)
        g_pending.nick[i] = u[i];
    for(; i < PARTY_NICK_CAP; i++)
        g_pending.nick[i] = 0;
}
static inline const uint16_t*
    party_pending_nick(void) { // its nickname (Unicode, END-terminated, never NULL; empty = none)
    return g_pending.nick;
}

// Seed helper for hosts / tests.
void party_seed(uint32_t seed);
