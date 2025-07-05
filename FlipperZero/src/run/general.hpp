#pragma once
#include <furi.h>

typedef enum
{
    TitleIndexStory = 0, // switch to story mode
    TitleIndexPvP = 1,   // switch to PvP mode
    TitleIndexPvE = 2,   // switch to PvE mode
} TitleIndex;

typedef enum
{
    MenuIndexProfile = 0, // profile
    MenuIndexAbout = 1,   // about
} MenuIndex;

typedef enum
{
    GameStatePlaying = 0,         // Game is currently playing
    GameStateMenu = 1,            // Game is in menu state
    GameStateSwitchingLevels = 2, // Game is switching levels
    GameStateLeavingGame = 3,     // Game is leaving
} GameState;