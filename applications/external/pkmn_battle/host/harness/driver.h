#pragma once

/*
 * Plays the Game Boy side from the fixture (facing the Colosseum
 * receptionist): talks through the receptionist, sits at the console, picks
 * battle actions and uses the console again for rematches.
 *
 * Choices are made by presetting the menu cursors when the menus open
 * (wBattleMenuCursorPosition at BattleMenu, wCurMoveNum at
 * MoveSelectionScreen, wPartyMenuCursor at the party menu) and pulsing A, so
 * the GB's own menu code makes them. The action for each exchange comes
 * from a script of nibbles (BATTLEACTION_*) or, past its end, a random
 * usable move.
 */

#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

#include "gbh.h"

/* What this GB does in one clock role (chosen when the console is used,
 * at Link_PrepPartyData_Gen2, once the role is known). */
typedef struct {
    const uint8_t* script; /* GB nibble per exchange, for the first battle */
    uint32_t script_len;
    /* optional injections (replays, party fuzzing, twin golden runs) */
    bool inject_rns; /* wLinkBattleRNs before the RN block is sent */
    uint8_t rns[PKB_RNS_LEN];
    bool inject_party; /* party (and with inject_trainer, name and ID) */
    bool inject_trainer;
    PkbLinkPartyData party;
    /* optional: a fresh party for every battle (overrides `party`) */
    void (*make_party)(void* ctx, PkbLinkPartyData* party);
    void* make_party_ctx;
} DrvProfile;

enum {
    DRV_AS_MASTER,
    DRV_AS_SLAVE
};

typedef struct {
    Gbh* h;
    int target_battles; /* use the console this many times */
    int start_delay; /* frames to wait before the first input */
    DrvProfile prof[2]; /* by DRV_AS_* */
    const DrvProfile* p; /* the one in use */
    PkbPolicyRng rng; /* random usable moves past the script */
    unsigned switch_one_in; /* random choices switch 1 in N turns (0 = never) */

    /* state */
    int battles; /* finished */
    bool in_battle;
    uint32_t exchanges; /* LinkBattleSendReceiveAction calls this battle */
    int64_t planned_for;
    uint8_t planned;
    uint32_t tick;
    uint32_t idle;
    bool done;
} GbDriver;

void drv_init(GbDriver* d, Gbh* h, int battles, uint64_t seed);
/* Set the keys for the next frame (call once per frame). */
void drv_frame(GbDriver* d);
