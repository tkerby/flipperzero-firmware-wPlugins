#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pkb/team.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Serial_ExchangeBytes block sizes for a Gen 2 Colosseum session. */
#define PKB_RN_PREAMBLE_LEN 7
#define PKB_RNS_LEN 10
#define PKB_RN_BLOCK_LEN (PKB_RN_PREAMBLE_LEN + PKB_RNS_LEN) /* 17 */
#define PKB_PARTY_PREAMBLE_LEN 6
#define PKB_PARTY_BLOCK_LEN 450 /* wLinkSendParty .. wLinkSendPartyEnd */
#define PKB_PATCH_BLOCK_LEN 200 /* SERIAL_PATCH_LIST_LENGTH */
#define PKB_PATCH_PREAMBLE_LEN 3

#define PKB_PARTY_STRUCT_LEN 48
#define PKB_NAME_LEN 11
#define PKB_PATCH_REGION_LEN (2 + PKB_PARTY_STRUCT_LEN * PKB_PARTY_LENGTH) /* 290 */

#define PKB_SERIAL_PREAMBLE_BYTE 0xFD
#define PKB_SERIAL_NO_DATA_BYTE 0xFE
#define PKB_SERIAL_PATCH_TERMINATOR 0xFF

/* Byte layout identical to wLinkPlayerPartyData (441 bytes). */
typedef struct {
    uint8_t player_name[PKB_NAME_LEN];
    uint8_t count;
    uint8_t species[PKB_PARTY_LENGTH + 1];
    uint8_t player_id[2]; /* big endian; start of the patched region */
    uint8_t mons[PKB_PARTY_LENGTH][PKB_PARTY_STRUCT_LEN];
    uint8_t ot_names[PKB_PARTY_LENGTH][PKB_NAME_LEN];
    uint8_t nicknames[PKB_PARTY_LENGTH][PKB_NAME_LEN];
} PkbLinkPartyData;

#define PKB_LINK_PARTY_DATA_LEN 441

/* Build one 48-byte party_struct (big-endian multi-byte fields). */
void pkb_party_struct_build(const PkbTeamMon* mon, uint16_t ot_id, uint8_t out[PKB_PARTY_STRUCT_LEN]);

/* Build the RN block we send: 7 x $FD then 10 random numbers below $FD. */
void pkb_rn_block_build(const uint8_t rns[PKB_RNS_LEN], uint8_t out[PKB_RN_BLOCK_LEN]);

/*
 * Build the party block and patch list we send, as Link_PrepPartyData_Gen2
 * plus FixDataForLinkTransfer would. Trainer and OT names are `trainer_name`
 * (ASCII), nicknames are species names. Returns false if the patch list would
 * not fit in the 200 transmitted bytes (cannot happen for real teams).
 */
bool pkb_party_block_build(
    const PkbTeam* team,
    const char* trainer_name,
    uint8_t block[PKB_PARTY_BLOCK_LEN],
    uint8_t patch[PKB_PATCH_BLOCK_LEN]);

/*
 * Receiving side, as Gen2ToGen2LinkComms does it: skip leading 0/$FD/$FE,
 * copy 441 bytes dropping every $FE, then apply the patch list. Returns false
 * if `rx` runs out first.
 */
bool pkb_party_block_parse(
    const uint8_t* rx,
    size_t rx_len,
    const uint8_t* patch_rx,
    size_t patch_len,
    PkbLinkPartyData* out);

/* Link_CopyRandomNumbers (slave side): first 10 bytes that are not $FD/$FE. */
bool pkb_rn_block_parse(const uint8_t* rx, size_t rx_len, uint8_t rns[PKB_RNS_LEN]);

/*
 * FixDataForLinkTransfer patch list over `region` (the trainer ID and party
 * structs): every $FE becomes $FF and its 1-based index is recorded, in two
 * parts of SERIAL_PATCH_DATA_SIZE ($FC) bytes. Returns false on overflow.
 */
bool pkb_patch_list_build(uint8_t* region, size_t region_len, uint8_t patch[PKB_PATCH_BLOCK_LEN]);

/* Apply a received patch list to `region` (restores the $FE bytes). */
void pkb_patch_list_apply(const uint8_t* patch, size_t patch_len, uint8_t* region, size_t region_len);

#ifdef __cplusplus
}
#endif
