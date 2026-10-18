#pragma once
// PC box storage (contract section 25, 74), PC plugin side (box.c). TWELVE boxes of 20 creatures (12 x 20) and the item storage (50 slots, 99 each) in /ext/apps_data/elementis_zeta/box.bin (next to save.bin).
// The file is NEVER loaded as a whole: every call reads or writes one header / one record (read_at / write_at). A PC session is a TRANSACTION on
// a copy: box_begin() copies box.bin to box.wrk, the screens edit the copy, box_commit() writes save.bin.new and renames both over the real files
// (and puts the old ones back if the second rename fails), box_abort() throws the copy away. Missing / corrupt box.bin = twelve empty boxes and no items. A box.bin of the old format (version 1: four boxes, 5,296 bytes) is migrated
// by box_begin() into the new layout (the copy is the new file; the old one is replaced at the first commit that has something to save).
// No furi.h (host-compilable).
#include <stdint.h>
#include <stdbool.h>
#include "battle_core.h"
#include "party.h"
#include "world.h"

#define BOX_COUNT 12
#define BOX_CAP 20
#define BOX_VER 2
#define BOX_HDR 32
#define BOX_REC 66 // sizeof(Mon) 44 + u16 nickname[PARTY_NICK_CAP] 22
#define BOX_IT_N 50   // the item storage's slots (PC holds 50), each {item id, count 1..99} (2 bytes) after the last record
#define BOX_IT_OFF (BOX_HDR + BOX_COUNT * BOX_CAP * BOX_REC)
#define BOX_SIZE (BOX_IT_OFF + 2 * BOX_IT_N) // 15,972 bytes, always
#define BOX_V1_COUNT 4 // the old layout (version 1): 16-byte header, 4 boxes, 5,296 bytes
#define BOX_V1_HDR 16
#define BOX_V1_SIZE (BOX_V1_HDR + BOX_V1_COUNT * BOX_CAP * BOX_REC)

// One stored creature: the Mon of the party + its nickname as Unicode (empty = the species name).
typedef struct {
    Mon mon;
    uint16_t nick[PARTY_NICK_CAP];
} BoxRec;

// Opens a session: makes the working copy. False (nothing changed on disk) on an I/O error.
bool box_begin(void);
// Ends it without saving (deletes the copy).
void box_abort(void);
// Saves the game and the boxes together: save.bin and box.bin are replaced by the session's versions, or neither is (false). The session stays open
// after a failure (the caller may retry or box_abort()); after a success the copy is gone.
bool box_commit(const World* w);

uint8_t box_cur(void);                // current box 0..11
bool box_set_cur(uint8_t b);
uint8_t box_n(uint8_t b);             // creatures in box b
bool box_get(uint8_t b, uint8_t i, BoxRec* r); // false: I/O error or a record that is not a creature (a corrupt record: only Liberar can remove it)
// (box_scan_shiny, the Registro's variocolor scan of box.bin, is scene_reg.c's own since contract 76: the Registro lives in the casino plugin now and must not pull box.c's whole session code with it)
bool box_add(uint8_t b, const BoxRec* r);      // appends; false when the box is full or on an I/O error
bool box_remove(uint8_t b, uint8_t i);         // the later ones move up

// Party <-> box (the party's Unicode nicknames travel with the creature; the code cache is re-derived).
void box_from_party(BoxRec* r, uint8_t slot);
void box_party_drop(uint8_t slot);             // removes party slot `slot` (the later ones move up)
bool box_to_party(const BoxRec* r);            // appends to the party; false when it is full

// The pending capture (party.h) into the current box, or the next one with room (that box becomes the current one). Clears the pending slot.
// Returns the box number 1..12, 0 when every box is full (the creature is dropped: the caller says so), -1 on an I/O error (nothing changed).
int box_store_pending(void);

// The display name of a stored creature as codes of the active language (cap incl. END): the nickname, else the species name.
int box_name(const BoxRec* r, tchar* out, int cap);

// The item storage (contract 74, PC "Withdraw / Deposit / Toss item"): up to BOX_IT_N kinds, 99 of each, in the order they were first stored; the session's copy is written to the working file at every change.
// An I/O error is -1 (add) / false (take), as for the boxes: the caller ends the visit.
uint8_t box_it_n(void);                                     // kinds stored
bool box_it_get(uint8_t i, uint8_t* item, uint8_t* count);  // false: no such slot
uint8_t box_it_count(uint8_t item);                         // how many of `item` (0 = none)
int box_it_add(uint8_t item, uint8_t n);                    // stores up to n (cap 99 a slot, BOX_IT_N kinds); returns how many went in (0 = no room), -1 on an I/O error
bool box_it_take(uint8_t item, uint8_t n);                  // removes n (refused when fewer are stored); false = not enough or an I/O error

// Everything a session may change in RAM, to put it back when the commit fails for good.
typedef struct {
    BagSlot bag[BAG_MAX_KINDS]; // contract 74: the bag (the item storage moves items between it and the box file)
    uint8_t n_bag;
    Party party;
    uint16_t nick[BC_MAX_PARTY][PARTY_NICK_CAP];
    tchar cache[BC_MAX_PARTY][PARTY_NICK_CAP]; // the nickname codes of the active language
    PartyPending pend;
} BoxSnap;
void box_snap_take(BoxSnap* s);
void box_snap_restore(const BoxSnap* s);
