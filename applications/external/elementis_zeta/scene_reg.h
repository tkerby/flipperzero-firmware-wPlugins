#pragma once
// The Registro (contract section 30): pause menu -> Registro. A screen of the PC plugin (ez_pc.fal) on the device, linked in on the
// host. List of every species of the Registro (rank number, name or "-----", a code-drawn crystal mark: filled = caught, hollow = seen), header "Visto N
// Capturado N"; OK on a seen entry opens its page: the medium front picture, name, number and types (+ page 2 of a caught entry: height, weight and a
// short description). The data is files/reginfo.bin (tools/regc.py), read on demand; the seen / caught bits are PartyState.reg_seen / reg_caught.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "text.h"

extern const SceneOps scene_reg_ops;

// Pushes the Registro on top of the stack (the pause menu stays below). False (nothing pushed) when the stack is full, one is open, or no heap.
bool scene_reg_open(SceneCtx* c);
// True while the screen's state exists.
bool scene_reg_active(void);

#ifndef NUCLEO_DEVICE
// Test hooks (host harness only); -1 / false when the screen is not open.
void reg_scan_shiny(
    uint8_t*
        bits); // contract 76 (was box_scan_shiny in box.c): sets bit (species & 7) of byte (species >> 3) for every variocolor in box.bin on the card
int scene_reg_mode(void); // 0 list, 1 entry
int scene_reg_cur(void); // list cursor (0-based rank)
int scene_reg_top(void); // first visible row
int scene_reg_page(void); // entry page (0 / 1)
int scene_reg_count(void); // entries in the list
int scene_reg_vis(void); // rows that fit the screen
int scene_reg_species(int k); // species id at rank k (0 = none / out of range)
bool scene_reg_star(
    int species); // contract 65: the screen shows a variocolor star for that species (the player owns one: party, day care or PC box)
bool scene_reg_has_info(
    void); // reginfo.bin was read (else the fallback list: species ids in order, no height / weight / description)
void scene_reg_counts(int* seen, int* caught);
void scene_reg_heap(uint32_t* bytes, uint32_t* blocks);
const tchar* scene_reg_row_name(
    int v); // contract 68: the name codes the visible row v draws ("???" for the rumored Genesa, "-----" for an unseen entry), NULL out of range
int scene_reg_areas(
    int* area,
    int* how,
    int max); // contract 75: the AREA page of the open entry: fills up to `max` (area index, how bits 1 grass / 2 surf / 4 fishing), returns the count (-1 not open)
int scene_reg_area_top(void); // the first AREA row shown
int scene_reg_area_rows(void); // AREA rows that fit the screen
const tchar* scene_reg_area_name(int v); // the name codes of the visible AREA row v
int scene_reg_entry_black(
    void); // contract 68: the open entry's picture is a silhouette (every pixel byte 0xFF): 1 / 0, -1 no picture
#endif
