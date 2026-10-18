#pragma once
// Game natives of the script VM (contract section 8.4 / 9 / 10): replaces the logging stubs of ids 1 - 8 with the
// party / naming / popup / battle logic (4 battle TRAINER VAR, 5 heal_party). No furi.h.
#include <stdint.h>
#include <stdbool.h>

// Registers the natives and installs world_new_game_hook (party_new_game) and text_lang_hook (party_apply_names).
// Call once after party_init(), before the first script runs. Idempotent.
void natives_init(void);

// M3-B natives (ids not in script.h's NAT_* enum; contract 8.4): `wild SPECIES LEVEL VAR` is a mapc macro for
// `wild_set SPECIES LEVEL` + `wild_go VAR`.
enum { NAT_WILD_SET = 9, NAT_WILD_GO = 10 };
// M3-E: `give_item ITEM N` (id 11): adds N of the item to the bag; var 0 receives how many were really added.
enum { NAT_GIVE_ITEM = 11 };
// M4-C (ids 13..20; 12 is `shop`, natives_shop.h): bag / money helpers, the trainer sight sequence, the blackout, item names.
//  13 take_item ITEM VAR: removes one of the item, VAR = 1 when the bag had it.     14 has_item ITEM VAR: VAR = 1 when it has one or more.
//  15 money_add N: +N*10 yen (N is a u8, 0..255 = up to 2550).    16 money_take N VAR: -N*10 yen; VAR = 1 paid, 0 not enough (nothing taken).
//  17 trainer_spot NPC: var 0 = distance to the hero (1 = adjacent / not in line); when > 1 the NPC shows the "!" balloon.
//  18 trainer_come NPC: starts the NPC's walk towards the hero until adjacent (not blocking: the script follows with wait_idle).
//  19 blackout: cuts the script and starts the blackout (13.7).    20 item_var N ITEM: text variable {vN} = the item's name.
enum {
    NAT_TAKE_ITEM = 13,
    NAT_HAS_ITEM = 14,
    NAT_MONEY_ADD = 15,
    NAT_MONEY_TAKE = 16,
    NAT_TRAINER_SPOT = 17,
    NAT_TRAINER_COME = 18,
    NAT_BLACKOUT = 19,
    NAT_ITEM_VAR = 20
};
// Task K (contract 9.2 / 11.6): 21 nick_prompt SLOT: "do you want to give a nickname to X?" YES / NO, then the naming keyboard, for party slot SLOT
// (255 = the creature added last); yields until it is done; {v0} = the creature's display name afterwards. A wild battle that ends with a
// capture opens the same prompt for the new party slot (world_pb_hook PBH_NICK).
enum { NAT_NICK_PROMPT = 21 };
extern uint8_t g_wild_count_before; // the party's size when the last wild battle opened (a host test that replaces world_wild_hook sets it)
// C3-A (contract 32): 23 world = the world systems of chapter 3 in ONE native. `a` = operation << 4 | VAR (the script variable it works on), `b` = an operand. mapc's macros hide the
// encoding (`pick_party`, `trade`, `daycare_*`, `rand`, `copyv`, `addvv`, `sfx`). Yields only for the pickers.
//  op 0 C3A_PICK       the party picker: VAR = the chosen slot 0..5, 255 = cancelled (Back, or the screen could not open).                                        [yields]
//  op 1 C3A_PICK_TRADE the trade picker (World.tr = wanted species, given species, nickname text id u16 LE): the chosen creature is traded when it is the wanted species:
//                      VAR = 1 traded, 0 wrong species, 255 cancelled.                                                                                          [yields]
//  op 2 C3A_TRADE_ARG  World.tr[VAR & 3] = b (the trade macro stores the four arguments before the picker).
//  op 3 C3A_DEPOSIT    daycare_deposit(slot in VAR): VAR = 1 done, 0 refused (care full / the last creature); 255 (the picker was cancelled) stays 255.
//  op 4 C3A_WITHDRAW   daycare_withdraw: VAR = 1 done (the fee was charged), 0 the party is full / nothing in care, 2 not enough money (nothing charged).
//  op 5 C3A_QUERY      VAR, VAR+1, VAR+2 = occupied, species, levels gained (VAR <= 13); text variables {v1} = the fee, {v2} = the levels (the script names the species: name_var 0 VAR+1).
//  op 6 C3A_RAND       VAR = random 0..b-1 (b = 0 gives 0).      op 7 C3A_COPY  VAR = var b.      op 8 C3A_ADD  VAR += var b (mod 256).
//  op 9 C3A_SFX        plays the sound EFFECT number b (0 = the first effect: SND_FX0 + b; mapc's `sfx NAME` takes the number from the `fx` lines of assets/music/index.txt).
//  op 10 C3A_MUSIC     plays the music TRACK with id b (mapc's `music NAME`: the position among the `music` lines of assets/music/index.txt = its SND_* id): the track replaces what plays.
//  op 12 C4A_CAUGHT    (C4-A, contract 35.5) b = 0: VAR = the number of species caught in the Registro (bag.c reg_caught_count; 0..255); b = N: VAR = 1 when at least N are caught, else 0 (mapc `caught_count VAR`, `caught_ge VAR N`).
//  op 13 C4A_ELEVATOR  (C4-A) the floor list: the rows are World.tr (mapc's `elevator VAR KEY1..KEY5` stores them: tr[2k], tr[2k + 1] = the text id of row k, tr[10] = 2..5 rows); VAR = the chosen row 0..n-1, 255 = Back. [yields; PC plugin, scene_card.c scene_list_open]
//  op 11 C3A_MUSIC_MAP plays the loaded map's own track again (`music_map`: sound_play(SND_PICK + SND_TAB_MAP + map id), what world_load_map does).
enum { NAT_WORLD = 23 };
enum { C3A_PICK = 0, C3A_PICK_TRADE, C3A_TRADE_ARG, C3A_DEPOSIT, C3A_WITHDRAW, C3A_QUERY, C3A_RAND, C3A_COPY, C3A_ADD, C3A_SFX, C3A_MUSIC, C3A_MUSIC_MAP, C4A_CAUGHT, C4A_ELEVATOR };
