// Save game file (contract section 12), UI plugin side: format, validation, save_peek / save_write / save_load. The play time, the log hook
// and save_continue() (which loads the map after save_load) stay resident in save.c. See save.h.
#include "save.h"
#include "assets.h"
#include "settings.h" // settings_crc32 (IEEE CRC32, shared with settings.bin)
#include "party.h"
#include "bag.h"
#include "text.h"
#include "gen/maps_gen.h"
#include "names.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save_enc.h" // SAVE_TAIL, SAVE_LEN, put32 / get32, save_encode (shared with the PC box code)

// The formats this build reads (contract 12.1 / 33.1 / 40.4): the format byte, its PartyState version and size, its header size (the flags fill it: hdr - 32 bytes). An older PartyState is the unchanged prefix of the next; the rest is zeroed.
static const struct {
    uint8_t fmt, pver, hdr;
    uint16_t psize;
} FMTS[] = {
    {SAVE_FMT_V1, 2, SAVE_HDR_V1, PARTY_STATE_V2_SIZE},
    {SAVE_FMT_V2, 3, SAVE_HDR_V1, PARTY_STATE_V3_SIZE},
    {SAVE_FMT_V3, 4, SAVE_HDR_V1, PARTY_STATE_V4_SIZE},
    {SAVE_FMT_V4, PARTY_STATE_VER, SAVE_HDR_V1, sizeof(PartyState)},
    {SAVE_FMT_VER, PARTY_STATE_VER, SAVE_HDR_SIZE, sizeof(PartyState)}};
#define N_FMTS 5

// Total file size of format entry i.
static uint32_t fmt_len(unsigned i) {
    return FMTS[i].hdr + FMTS[i].psize + SAVE_TAIL;
}
static bool known_len(uint32_t len) {
    for(unsigned i = 0; i < N_FMTS; i++)
        if(len == fmt_len(i)) return true;
    return false;
}
// The index of the format byte b[4] in FMTS, N_FMTS = unknown.
static unsigned fmt_of(const uint8_t* b) {
    unsigned i = 0;
    while(i < N_FMTS && FMTS[i].fmt != b[4])
        i++;
    return i;
}

// Checks a whole file image; fills *info. Returns 0 when valid, else the SAVE_WHY_* reason.
static int validate(const uint8_t* b, uint32_t len, SaveInfo* info) {
    if(!known_len(len)) return SAVE_WHY_SIZE;
    if(memcmp(b, "NSAV", 4) != 0) return SAVE_WHY_MAGIC;
    const unsigned f = fmt_of(b);
    if(f >= N_FMTS) return SAVE_WHY_FORMAT;
    if(len != fmt_len(f)) return SAVE_WHY_SIZE;
    if(b[9] != FMTS[f].pver || (uint16_t)(b[10] | (b[11] << 8)) != FMTS[f].psize)
        return SAVE_WHY_PARTY;
    if(get32(b + len - SAVE_TAIL) != settings_crc32(b, len - SAVE_TAIL)) return SAVE_WHY_CRC;
    if(b[8] > 7 || get32(b + 12) > SAVE_MAX_SECS) return SAVE_WHY_RANGE;
    char path[16];
    snprintf(path, sizeof(path), "map_%02u.bin", (unsigned)b[5]);
    if(file_size(path) <= 0) return SAVE_WHY_MAP;
    g_peek_aff =
        b[FMTS[f].hdr +
          offsetof(
              PartyState,
              aff)]; // contract 66 (inside every format's PartyState prefix: aff sits before name_u)
    if(info) {
        info->map = b[5];
        info->ti = b[6];
        info->tj = b[7];
        info->facing = b[8];
        info->secs = get32(b + 12);
        info->gid = (uint16_t)(b[FMTS[f].hdr + offsetof(PartyState, gid)] |
                               b[FMTS[f].hdr + offsetof(PartyState, gid) + 1]
                                   << 8); // (inside every format's PartyState prefix)
    }
    return 0;
}

// Reads + validates the save into a heap buffer (caller frees); NULL = no (valid) save.
static uint8_t* read_valid(SaveInfo* info) {
    int32_t n = file_size(SAVE_PATH);
    if(n < 0) return NULL; // no file: the normal case
    const bool ok_len = n > 0 && known_len((uint32_t)n);
    uint8_t* b = ok_len && heap_can_alloc((uint32_t)n) ? malloc((size_t)n) : NULL;
    int why;
    if(!b)
        why = ok_len ? SAVE_WHY_IO : SAVE_WHY_SIZE;
    else if(!read_at(SAVE_PATH, 0, b, (uint32_t)n))
        why = SAVE_WHY_IO;
    else
        why = validate(b, (uint32_t)n, info);
    if(why) {
        save_log_why(why);
        free(b);
        return NULL;
    }
    return b;
}

bool save_peek(SaveInfo* info) {
    uint8_t* b = read_valid(info);
    bool ok = b != NULL;
    free(b);
    return ok;
}

bool save_write(const World* w) {
    uint8_t* b = heap_can_alloc(SAVE_LEN) ? malloc(SAVE_LEN) : NULL;
    if(!b) {
        save_log_why(SAVE_WHY_IO);
        return false;
    }
    save_encode(b, w);
    bool ok = write_file(SAVE_PATH, b, SAVE_LEN);
    free(b);
    if(!ok) save_log_why(SAVE_WHY_IO);
    return ok;
}

// Reads the save into the game state: flags, vars, PartyState (an older file: the missing tail zeroed, a format-1 file: names migrated), the play time; *in = where to put the hero. The
// resident save_continue() loads the map after this. False (nothing changed) when there is no valid save.
bool save_load(World* w, SaveInfo* in) {
    uint8_t* b = read_valid(in);
    if(!b) return false;
    const unsigned f = fmt_of(b), hdr = FMTS[f].hdr;
    memset(
        w->flags,
        0,
        WORLD_FLAG_BYTES); // (formats 1..4 have 32 flag bytes: flags 256..1023 come up clear)
    memcpy(w->flags, b + 16, hdr - 16 - SCRIPT_VARS);
    memcpy(w->vars, b + hdr - SCRIPT_VARS, SCRIPT_VARS);
    const bool v1 = b[4] == SAVE_FMT_V1;
    memset(
        party_state(),
        0,
        sizeof(
            PartyState)); // (a format-1 file has no names / nicknames, format 1 and 2 have no day care, formats 1..3 have no coins: they stay empty / 0)
    memcpy(party_state(), b + hdr, FMTS[f].psize);
    free(b);
    DayCare* dc =
        &party_state()
             ->dc; // a day-care creature that cannot be one (the CRC cannot tell a bad writer) is no creature
    if(dc->mon.species &&
       ((party_data() && dc->mon.species >= party_data()->n_species) || !dc->mon.level ||
        dc->mon.level > 100 || !dc->mon.max_hp || dc->mon.hp > dc->mon.max_hp))
        memset(dc, 0, sizeof(*dc));
    dc->nick_u[PARTY_NICK_CAP - 1] = 0;
    if(party_state()->coins > COINS_MAX)
        party_state()->coins =
            COINS_MAX; // (formats 1..3 come up with 0 coins: the tail is zeroed)
    if(v1)
        party_migrate_names(); // the preset indexes of the old names -> name strings, in the language in use
    if(party_state()->aff > AFF_LUNAR) party_state()->aff = AFF_NONE;
    if(party_state()->aff == AFF_NONE)
        party_state()->aff =
            g_aff_choice; // contract 66: an older save: what the title's Continuar prompt asked (0 when nothing was asked)
    g_aff_choice = 0; // (used up)
    names_apply();
    return true;
}
