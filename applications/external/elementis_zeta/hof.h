#pragma once
// The Hall of Legacy file (contract section 50.6): /ext/apps_data/elementis_zeta/hof.bin (next to save.bin: the path is derived from SAVE_PATH), NOT part of the save. A ring of HOF_MAX runs, newest first: each run is the party that
// beat the champion (up to 6 creatures: species, level, Unicode nickname) and the play time. Ending-plugin side (hof.c; the host links it directly). No furi.h (host-compilable).
//
// File (little-endian, always HOF_SIZE = 1,556 bytes):
//   0  "NHOF"  4 u8 version (1)  5 u8 count of valid runs 0..HOF_MAX  6 u8 head = the slot of the NEWEST run  7 u8 HOF_MAX  8 u16 record size (HOF_REC)  10 u16 0  12 u32 CRC32 (IEEE) of bytes 0..11
//   16 + slot * HOF_REC : record = u32 play time (seconds)  u8 n (1..6)  u8 0  6 x { u8 species  u8 level  u16 nick[HOF_NICK] (Unicode, END-terminated, empty = the species name) }  u32 CRC32 of the 150 bytes before it
// A new run goes to slot (head + 1) % HOF_MAX (the first one to slot 0), so once HOF_MAX runs are in it the OLDEST is overwritten. A file whose size, magic, version, sizes or header CRC are wrong is "no runs"; a record
// whose CRC or fields (species 0, level 0 / > 100, n 0 / > 6, a nickname without its END) are wrong is refused on its own (hof_get false), never read half-way. Nothing here ever crashes on a damaged file.
#include <stdint.h>
#include <stdbool.h>
#include "party.h"

#define HOF_MAX  10
#define HOF_TEAM 6
#define HOF_NICK PARTY_NICK_CAP
#define HOF_VER  1
#define HOF_HDR  16
#define HOF_REC  154
#define HOF_SIZE (HOF_HDR + HOF_MAX * HOF_REC)

#ifndef HOF_PATH_SUFFIX
#define HOF_PATH_SUFFIX "hof.bin"
#endif

typedef struct {
    uint8_t species, level;
    uint16_t nick[HOF_NICK]; // Unicode, END-terminated; empty = show the species name
} HofMon;

typedef struct {
    uint32_t secs; // play time when the league was won
    uint8_t n; // creatures 1..HOF_TEAM
    HofMon mon[HOF_TEAM];
} HofRun;

// How many valid runs the file holds (0 = missing / damaged); *head (when not NULL) = the slot of the newest.
uint8_t hof_count(uint8_t* head);
// Run k (0 = the newest, 1 = the one before ...). False: no such run, a damaged file or a damaged record.
bool hof_get(uint8_t k, HofRun* r);
// Records `r` as the newest run (the file is read, one record replaced, the whole file written again through write_file's tmp + rename). A missing or damaged file starts a new ring with this run as its only one.
// False: the run is invalid or the card could not be written (the old file is untouched).
bool hof_add(const HofRun* r);
// The player's party as a run (the creatures, their nicknames, the play time). False: an empty party.
bool hof_from_party(HofRun* r);
// "<folder of save.bin>hof.bin"
void hof_path(char* out, int cap);
