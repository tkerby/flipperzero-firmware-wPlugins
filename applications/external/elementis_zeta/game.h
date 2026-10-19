#pragma once
// Engine types shared between main.c and the game thread.
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "frame.h"

struct World; // defined by world.h (gameplay)

typedef struct Game {
    Frame* frame;
    struct World* world;
    volatile bool quit; // set by the game thread to end the app; also ends the flip loop
} Game;

// Entry of the game thread (FuriThreadPriorityLow). Created by main.c with ctx = Game*.
// Implemented by game_loop.c (gameplay) -- boot_test.c provides a temporary one for gate G1.
// Must set game->quit = true before returning.
int32_t game_thread(void* ctx);

// Blocks (yielding the CPU) until the flip loop has adopted the previous publish, i.e.
// frame_ready(). Call before composing. Implemented in main.c.
void game_wait_ready(Frame* f);
