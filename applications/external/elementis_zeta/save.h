#pragma once
// Save game (contract section 12): /ext/apps_data/elementis_zeta/save.bin, the Title "Continuar" / "Nueva partida" list and the
// pause menu's "Guardar" screen. No furi.h (host-compilable); files go through assets.h (read_at / file_size /
// write_file, a handle is never kept open).
//
// File (little-endian, 164 + sizeof(PartyState) bytes since format 5; formats 1..4 had a 64-byte header (68 + sizeof(PartyState)), see below; format 1 had PartyState v2 = 388 bytes, format 2 v3 = 552, format 3 v4 = 620 and format 4 v5 = 624, all still read):
//   0  "NSAV"            4  u8 format version (SAVE_FMT_VER)   5  u8 map id        6  u8 hero ti     7  u8 hero tj
//   8  u8 hero facing    9  u8 PARTY_STATE_VER                10 u16 sizeof(PartyState)
//   12 u32 play time, seconds (capped at 999:59:59)
//   16 u8 flags[128]     144 u8 vars[16]                      160 PartyState, verbatim
//   160 + sizeof(PartyState): u32 CRC32 (IEEE, settings_crc32) of every byte before it
// (formats 1..4: flags[32] at 16, vars[16] at 48, PartyState at 64: the header is `SAVE_HDR_V1` = 64 bytes; flags[] always fills the header between the play time and vars[16])
// Format 2 (task K) = PartyState v3: + the names and nicknames as Unicode. A format-1 file (PartyState v2, 456 bytes in all) loads too:
// the v2 prefix of PartyState is unchanged, the rest is zeroed and the preset indexes of the old names become name strings
// (party_migrate_names); the next save writes format 2.
// Format 3 (C3, contract 33) = PartyState v4: + the day care (68 B appended). A format-1 or format-2 file loads too (the unchanged prefix, the rest zeroed = an empty day care);
// the next save writes format 3.
// Format 4 (C4-C, contract 37) = PartyState v5: + the casino coins (4 B appended). Formats 1..3 load too (the unchanged prefix, the rest zeroed = 0 coins); the next save writes format 4.
// Format 5 (contract 40, "more story flags") = the same PartyState v5, but the flag bitset grew from 256 to 1,024 flags: the header is 160 bytes (flags[128]). Formats 1..4 load too (their 32 flag bytes land in
// the first 32 bytes of the bitset, flags 256..1023 come up clear); the next save writes format 5.
// Anything that does not match exactly (size, magic, versions, PartyState size / version, CRC, field ranges, a map file
// that is not there) is "no save": it is logged through save_log_fn and ignored, never a crash.
#include <stdint.h>
#include <stdbool.h>
#include "scene.h"
#include "world.h"

#ifndef SAVE_PATH // the host harness overrides it with -DSAVE_PATH=...
#define SAVE_PATH "/ext/apps_data/elementis_zeta/save.bin"
#endif
#define SAVE_FMT_VER 5
#define SAVE_FMT_V1  1 // PartyState v2 (388 bytes)
#define SAVE_FMT_V2  2 // PartyState v3 (552 bytes)
#define SAVE_FMT_V3  3 // PartyState v4 (620 bytes)
#define SAVE_FMT_V4 \
    4 // PartyState v5 (624 bytes); the last format with the 64-byte header (32 flag bytes)
#define SAVE_HDR_V1   64 // header of formats 1..4: flags[32], vars[16]
#define SAVE_HDR_SIZE 160 // header of format 5: flags[128] (WORLD_FLAG_BYTES), vars[16]
#define SAVE_MAX_SECS (999u * 3600u + 59u * 60u + 59u)

typedef struct {
    uint8_t map, ti, tj, facing;
    uint16_t gid; // contract 81: PartyState.gid of the file (0 = a file of an older build)
    uint32_t secs; // play time
} SaveInfo;

// "save: rejected (reason N)" lines (a file that was ignored, a write that failed); NULL = not installed (host tests).
// N: 1 size / missing parts, 2 magic, 3 format version, 4 PartyState version or size, 5 CRC, 6 field out of range (facing, play
// time, hero tile), 7 map file missing / map load failed, 8 I/O (read, write or memory).
enum {
    SAVE_WHY_SIZE = 1,
    SAVE_WHY_MAGIC,
    SAVE_WHY_FORMAT,
    SAVE_WHY_PARTY,
    SAVE_WHY_CRC,
    SAVE_WHY_RANGE,
    SAVE_WHY_MAP,
    SAVE_WHY_IO
};
extern void (*save_log_fn)(const char* msg);

// ---- file (save_io.c, UI plugin side; the host links it directly)
// True when SAVE_PATH holds a valid save (fills *info when not NULL). Reads and closes the file; the missing file is
// the normal "no save" case (silent), everything else invalid is logged.
bool save_peek(SaveInfo* info);
// Writes the current game (map / hero tile / facing, flags, vars, PartyState, play time) with write_file() (tmp + rename).
bool save_write(const World* w);
// Applies the save to the game state: flags, vars, PartyState (+ names), and says where the hero goes in *in (including the play time, which
// the caller sets). False (nothing changed) when there is no valid save. Called through the UI plugin (ui_save_load) by save_continue().
bool save_load(World* w, SaveInfo* in);
// Resident: save_load() through the UI plugin, the play time, then world_load_map() at the saved tile. False (nothing changed, or the map
// cannot be loaded) -> the caller starts a new game. Used by the FADE scene's continue job (the title scene keeps the plugin loaded then) and
// by the boot without the UI plugin (which then simply cannot continue).
bool save_continue(World* w);
// Logs "save: rejected (reason N)" through save_log_fn (resident; the plugin side calls it through the app's symbol table).
void save_log_why(int why);

// ---- play time (counted while the game runs: the world scene is the bottom of the stack)
void save_time_tick(const SceneCtx* c, uint32_t dt_ms); // game_loop.c, once per loop turn
uint32_t save_time_secs(void);
void save_time_set(uint32_t secs); // new game: 0

// ---- pause menu: "Guardar" (info box, question, YES / NO, save, confirmation). Pushes the SAVE scene above the
// menu; on success it closes the menu too, No returns to the menu.
bool scene_save_open(SceneCtx* c);
extern const SceneOps scene_save_ops;
