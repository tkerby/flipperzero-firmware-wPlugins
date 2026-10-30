#pragma once

/*
 * Shadow engine: our battle engine runs alongside the *slave* Game Boy of a
 * twin link, from the same perspective, so its WRAM must equal the slave's
 * byte for byte.
 *
 * At the slave's DoBattle the engine is seeded with a copy of the slave's
 * WRAM/HRAM. Its choices are the ones the slave made (read when the slave
 * enters each exchange), the master's actions are what the slave received.
 * Before every exchange and at the end of the battle the engine's memory is
 * compared with the slave's snapshot.
 */

#include <pkb/engine.h>

#include "gbh.h"
#include "sym.h"

#define SHADOW_MAX_EXCHANGES 512

typedef struct {
    uint8_t wram[0x2000];
    uint8_t hram[0x80];
    uint8_t player_action; /* wBattlePlayerAction */
    uint8_t move_num; /* wCurMoveNum */
    uint8_t party_mon; /* wCurPartyMon */
} ShadowSnap;

typedef struct Shadow Shadow;
typedef void (*ShadowStep)(void* ctx);

struct Shadow {
    Gbh* gb[2];
    int slave; /* index of the slave GB, -1 until a battle starts */
    const SymTable* syms;
    ShadowStep step; /* advance the twin by one frame */
    void* step_ctx;
    uint32_t step_limit; /* frames to wait for the slave before giving up */

    /* slave events of the current battle */
    bool started;
    ShadowSnap start;
    ShadowSnap* entry; /* [SHADOW_MAX_EXCHANGES] */
    uint32_t entries;
    uint8_t exit_action[SHADOW_MAX_EXCHANGES];
    uint8_t exit_sent[SHADOW_MAX_EXCHANGES];
    uint32_t exits;
    bool ended;
    ShadowSnap end;

    /* engine side */
    PkbEngine E;
    PkbEngineIo io;
    uint32_t engine_exchanges;
    bool running;

    /* results */
    uint32_t battles;
    uint32_t total_exchanges;
    uint32_t diverged_battles;
    uint32_t unsupported_battles;
    uint32_t
        game_desync_battles; /* PKB_ENGINE_GAME_DESYNC: the two ROMs disagree, the engine still follows the slave */
    bool verbose;
    bool trace; /* print every exchange */
    bool use_init; /* seed the engine with pkb_engine_init() instead of the slave's WRAM */
    char first_divergence[2048];
};

void shadow_init(Shadow* sh, Gbh* a, Gbh* b, const SymTable* syms, ShadowStep step, void* step_ctx);
void shadow_free(Shadow* sh);
/* Call once per twin frame; runs a whole engine battle when the slave starts one. */
void shadow_poll(Shadow* sh);
