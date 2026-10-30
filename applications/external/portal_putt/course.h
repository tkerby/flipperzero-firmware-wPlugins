#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Playfield: the whole screen below a 9 px status bar, inside a 1 px frame.
#define FIELD_X0 1
#define FIELD_Y0 10
#define FIELD_X1 126 // last playable column
#define FIELD_Y1 62 // last playable row

#define HOLE_COUNT 9
#define MAX_THINGS 14

typedef enum {
    ThemeFantasy,
    ThemeSciFi,
} Theme;

typedef struct {
    int16_t x, y;
    uint8_t w, h;
} Rect;

typedef enum {
    ThingTree, // round obstacle r=3, drawn as a tree (fantasy)
    ThingPod, // round obstacle r=3, drawn as a pod (sci-fi)
    ThingBumper, // round obstacle r=a, bouncy, boosts the ball
    ThingPortal, // x,y centre; a = pair id; b = exit direction 0 R 1 D 2 L 3 U
    ThingWell, // gravity well; a = radius; b = strength (px/s^2 at the rim side)
    ThingLava, // rect a*b at x,y; ball inside is lost (penalty)
    ThingSlime, // rect a*b; heavy friction
    ThingConveyor, // rect a*b; c = direction, pushes the ball
    ThingMover, // square a*a at x,y sliding b px along axis c (0 x, 1 y)
    ThingWind, // constant push: x, y are the acceleration / 4 px/s^2
} ThingType;

typedef struct {
    uint8_t type;
    int16_t x, y;
    uint8_t a, b, c;
} Thing;

typedef struct {
    const char* name;
    const char* lost_msg; // shown when the ball is lost
    uint8_t theme;
    uint8_t par;
    uint8_t ball_x, ball_y;
    uint8_t cup_x, cup_y;
    const Rect* walls;
    uint8_t wall_count;
    const Thing* things;
    uint8_t thing_count;
} Hole;

extern const Hole holes[HOLE_COUNT];

// Current rectangle of a mover at a given tick.
Rect mover_rect(const Thing* t, uint32_t tick);
bool rect_contains(const Rect* r, float x, float y);
