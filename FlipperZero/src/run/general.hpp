#pragma once
#include <furi.h>

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define MAX_WORLD_OBJECTS 25

typedef enum
{
    TitleIndexStory = 0, // switch to story mode
    TitleIndexPvE = 1,   // switch to PvE mode
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
    LevelUnknown = -1,    // Unknown level
    LevelHomeWoods = 0,   // Home Woods level
    LevelRockWorld = 1,   // Rock World level
    LevelForestWorld = 2, // Forest World level
} LevelIndex;

typedef enum
{
    ICON_ID_INVALID = -1,
    ICON_ID_HOUSE,
    ICON_ID_PLANT,
    ICON_ID_TREE,
    ICON_ID_FENCE,
    ICON_ID_FLOWER,
    ICON_ID_ROCK_LARGE,
    ICON_ID_ROCK_MEDIUM,
    ICON_ID_ROCK_SMALL,
} IconID;

typedef struct
{
    IconID id;
    const uint8_t *icon;
    Vector pos;  // position at which to draw the icon
    Vector size; // dimensions for centering
} IconSpec;

typedef struct
{
    int count;       // number of icons in this group
    IconSpec *icons; // pointer to an array of icon specs
} IconGroupContext;

inline float atof_(const char *nptr) { return (float)strtod(nptr, NULL); }
inline bool toggleToBool(ToggleState state) noexcept { return state == ToggleOn; }
inline const char *toggleToString(ToggleState state) noexcept { return state == ToggleOn ? "On" : "Off"; }