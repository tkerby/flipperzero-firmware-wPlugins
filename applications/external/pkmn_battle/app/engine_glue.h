#pragma once

#include <furi.h>
#include <stdatomic.h>

#include <pkb/ai.h>
#include <pkb/engine.h>
#include <pkb/session.h>

#define PKB_ENGINE_CRC_CAP 512
#define PKB_ENGINE_STACK   (6 * 1024)

/* The battle engine on its own thread: blocks in exchange() until the link
 * session (ISR) has swapped our action with the Game Boy's. */
typedef struct {
    FuriThread* thread;
    PkbEngine* E;
    PkbEngineIo io;
    PblSession* session;
    PkbAi ai; /* chooses our moves, switches and replacements */
    atomic_bool abort;
    atomic_bool done;
    PkbBattleResult result;
    uint32_t stack_free; /* engine thread's stack high-water mark, bytes left */
    uint32_t posted;
    uint32_t crc[PKB_ENGINE_CRC_CAP]; /* state CRC at each exchange, for the log */
    atomic_uint ncrc;
    PkbLinkPartyData ours, theirs;
    uint8_t rns[PKB_RNS_LEN];
} PkbEngineGlue;

/* After PBL_EV_BLOCKS_DONE: parse our TX blocks and what the GB sent, then
 * start DoBattle as the slave. Returns false if the blocks don't parse. */
bool pkb_engine_glue_start(
    PkbEngineGlue* g,
    PblSession* s,
    const uint8_t party_tx[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch_tx[PKB_PATCH_BLOCK_LEN],
    uint64_t seed);
/* Abort (if still running) and free the thread. Safe to call twice. */
void pkb_engine_glue_stop(PkbEngineGlue* g);
/* stop, then free the engine state */
void pkb_engine_glue_free(PkbEngineGlue* g);
bool pkb_engine_glue_running(const PkbEngineGlue* g);
bool pkb_engine_glue_finished(const PkbEngineGlue* g);
