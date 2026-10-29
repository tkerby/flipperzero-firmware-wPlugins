#include <string.h>
#include <pkb/party_block.h>
#include <pkb/charset.h>
#include "pb_tables.h"

_Static_assert(sizeof(PkbLinkPartyData) == PKB_LINK_PARTY_DATA_LEN, "wLinkPlayerPartyData layout");
_Static_assert(PARTYMON_STRUCT_LENGTH == PKB_PARTY_STRUCT_LEN, "party_struct size");
_Static_assert(SERIAL_PATCH_LIST_LENGTH == PKB_PATCH_BLOCK_LEN, "patch list size");
_Static_assert(
    PKB_PARTY_PREAMBLE_LEN + PKB_LINK_PARTY_DATA_LEN + SERIAL_PADDING_LENGTH == PKB_PARTY_BLOCK_LEN,
    "wLinkSendParty size");

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

void pkb_party_struct_build(const PkbTeamMon* mon, uint16_t ot_id, uint8_t out[PKB_PARTY_STRUCT_LEN]) {
    memset(out, 0, PKB_PARTY_STRUCT_LEN);
    out[MON_SPECIES] = mon->species;
    out[MON_ITEM] = mon->item;
    memcpy(&out[MON_MOVES], mon->moves, NUM_MOVES);
    put16(&out[MON_OT_ID], ot_id);
    out[MON_EXP + 0] = (uint8_t)(mon->exp >> 16);
    out[MON_EXP + 1] = (uint8_t)(mon->exp >> 8);
    out[MON_EXP + 2] = (uint8_t)mon->exp;
    for(int s = 0; s < NUM_EXP_STATS; s++) put16(&out[MON_STAT_EXP + 2 * s], mon->stat_exp);
    out[MON_DVS + 0] = mon->dvs[0];
    out[MON_DVS + 1] = mon->dvs[1];
    memcpy(&out[MON_PP], mon->pp, NUM_MOVES);
    out[MON_HAPPINESS] = mon->happiness;
    out[MON_LEVEL] = mon->level;
    out[MON_STATUS] = 0;
    put16(&out[MON_HP], mon->max_hp);
    put16(&out[MON_MAXHP], mon->max_hp);
    for(int s = 0; s < PKB_NUM_BATTLE_STATS; s++) put16(&out[MON_STATS + 2 * s], mon->stats[s]);
}

void pkb_rn_block_build(const uint8_t rns[PKB_RNS_LEN], uint8_t out[PKB_RN_BLOCK_LEN]) {
    memset(out, PKB_SERIAL_PREAMBLE_BYTE, PKB_RN_PREAMBLE_LEN);
    for(int i = 0; i < PKB_RNS_LEN; i++) {
        /* FixDataForLinkTransfer rerolls values >= SERIAL_PREAMBLE_BYTE */
        out[PKB_RN_PREAMBLE_LEN + i] = rns[i] < PKB_SERIAL_PREAMBLE_BYTE ? rns[i] : (uint8_t)(rns[i] - 3);
    }
}

bool pkb_party_block_build(
    const PkbTeam* team,
    const char* trainer_name,
    uint8_t block[PKB_PARTY_BLOCK_LEN],
    uint8_t patch[PKB_PATCH_BLOCK_LEN]) {
    PkbLinkPartyData d;

    memset(&d, 0, sizeof(d));
    pkb_gb_encode(trainer_name, d.player_name, PKB_NAME_LEN);
    d.count = team->count;
    memset(d.species, 0xFF, sizeof(d.species));
    put16(d.player_id, team->trainer_id);
    for(int i = 0; i < team->count; i++) {
        const uint8_t* name;
        size_t len = pkb_species_name_gb(team->mon[i].species, &name);

        d.species[i] = team->mon[i].species;
        pkb_party_struct_build(&team->mon[i], team->trainer_id, d.mons[i]);
        memcpy(d.ot_names[i], d.player_name, PKB_NAME_LEN);
        memset(d.nicknames[i], PKB_GB_TERMINATOR, PKB_NAME_LEN);
        if(len > PKB_NAME_LEN - 1) len = PKB_NAME_LEN - 1;
        memcpy(d.nicknames[i], name, len);
    }

    memset(block, 0, PKB_PARTY_BLOCK_LEN);
    memset(block, PKB_SERIAL_PREAMBLE_BYTE, PKB_PARTY_PREAMBLE_LEN);
    memcpy(block + PKB_PARTY_PREAMBLE_LEN, &d, sizeof(d));
    /* padding (SERIAL_PADDING_LENGTH) stays zero, as after ClearLinkData */

    return pkb_patch_list_build(
        block + PKB_PARTY_PREAMBLE_LEN + offsetof(PkbLinkPartyData, player_id),
        PKB_PATCH_REGION_LEN,
        patch);
}

bool pkb_party_block_parse(
    const uint8_t* rx,
    size_t rx_len,
    const uint8_t* patch_rx,
    size_t patch_len,
    PkbLinkPartyData* out) {
    uint8_t* dst = (uint8_t*)out;
    size_t i = 0, n = 0;

    /* Link_FindFirstNonControlCharacter_SkipZero */
    while(i < rx_len && (rx[i] == 0 || rx[i] == PKB_SERIAL_PREAMBLE_BYTE ||
                         rx[i] == PKB_SERIAL_NO_DATA_BYTE))
        i++;
    /* Link_CopyOTData */
    for(; i < rx_len && n < PKB_LINK_PARTY_DATA_LEN; i++) {
        if(rx[i] != PKB_SERIAL_NO_DATA_BYTE) dst[n++] = rx[i];
    }
    if(n < PKB_LINK_PARTY_DATA_LEN) return false;

    pkb_patch_list_apply(patch_rx, patch_len, out->player_id, PKB_PATCH_REGION_LEN);
    return true;
}

bool pkb_rn_block_parse(const uint8_t* rx, size_t rx_len, uint8_t rns[PKB_RNS_LEN]) {
    size_t n = 0;
    for(size_t i = 0; i < rx_len && n < PKB_RNS_LEN; i++) {
        if(rx[i] == PKB_SERIAL_PREAMBLE_BYTE || rx[i] == PKB_SERIAL_NO_DATA_BYTE) continue;
        rns[n++] = rx[i];
    }
    return n == PKB_RNS_LEN;
}
