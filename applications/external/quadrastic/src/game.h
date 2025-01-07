#pragma once

#include <stdint.h>

#include "engine/engine.h" // IWYU pragma: keep

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define GAME_NAME "Quadrastic"

typedef enum {
    DifficultyEasy,
    DifficultyNormal,
    DifficultyHard,
    DifficultyInsane,
    DifficultyCount,
} Difficulty;

typedef enum {
    StateOff,
    StateOn,
    StateCount,
} State;

typedef enum {
    GameEventStopAnimation,
} GameEvent;

typedef struct {
    Level* menu;
    Level* game;
    Level* game_over;
    Level* settings;
    Level* about;
} Levels;

typedef struct NotificationApp NotificationApp;

typedef struct {
    NotificationApp* notification;

    Levels levels;
    Difficulty difficulty;

    uint32_t best_score;
    uint32_t score;

    State sound;
    State vibro;
    State led;

} GameContext;
