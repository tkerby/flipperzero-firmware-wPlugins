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

typedef enum
{
    ToggleOn,  // On
    ToggleOff, // Off
} ToggleState;

typedef enum
{
    LevelHomeWoods = 0,   // Home Woods level
    LevelRockWorld = 1,   // Rock World level
    LevelForestWorld = 2, // Forest World level
} LevelIndex;

inline bool toggleToBool(ToggleState state) noexcept { return state == ToggleOn; }
inline const char *toggleToString(ToggleState state) noexcept { return state == ToggleOn ? "On" : "Off"; }