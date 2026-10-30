#pragma once

/*
 * The device's configuration in the emulator: the Game Boy (master) against
 * our session with the battle engine behind it, as the Flipper app runs
 * them. The engine posts its actions through the session and waits for the
 * GB's; at the start of every turn its state is mirror-compared with the
 * GB's (host/sim/mirror.c).
 */

#include <pkb/engine.h>

#include "driver.h"
#include "flipper.h"
#include "gbh.h"
#include <pkb/ai.h>

typedef struct {
    Gbh* gb;
    GbDriver* d;
    FlipperModel* fm;
    uint32_t wait_limit; /* frames to wait for the GB at any point */
    unsigned switch_one_in; /* engine's random switches (0 = never) */
    bool use_ai; /* the Flipper's AI chooses (as on the device) */
    PkbAi ai;
    const uint8_t* script; /* replay: our nibble per exchange, for the first battle */
    uint32_t script_len;
    const char* dump;
    unsigned dump_every;
    unsigned frame;

    PkbEngine E;
    PkbEngineIo io;
    PkbPolicyRng rng;
    uint32_t posted;
    uint32_t engine_turns;

    /* GB snapshots at BattleTurn */
    uint8_t (*turn_wram)[0x2000];
    uint32_t gb_turns;

    uint32_t battles, exchanges, diverged, unsupported;
    uint32_t game_desyncs; /* battles with PKB_ENGINE_GAME_DESYNC (not counted as diverged) */
    unsigned battle_div;
    char first_divergence[2048];
} EngineSession;

void es_init(EngineSession* es, Gbh* gb, GbDriver* d, FlipperModel* fm, uint64_t seed);
void es_free(EngineSession* es);
/* One emulated frame (driver, GB, app model). */
void es_step(EngineSession* es);
/* Call after each es_step: runs a whole engine battle once the blocks are in. */
void es_poll(EngineSession* es);
