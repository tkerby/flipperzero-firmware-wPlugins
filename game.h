#pragma once
#include "engine/engine.h"

// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

// from https://github.com/jamisonderek/flipper-zero-tutorials/blob/main/vgm/apps/air_labyrinth/walls.h
typedef struct
{
    bool horizontal;
    int x;
    int y;
    int length;
} Wall;

#define WALL(h, y, x, l)   \
    (Wall)                 \
    {                      \
        h, x * 2, y * 2, l \
    }

typedef struct
{
    uint32_t score;
} GameContext;

typedef enum
{
    // system draw objects
    DRAW_DOT,        // canvas_draw_dot
    DRAW_LINE,       // canvas_draw_line
    DRAW_BOX,        // canvas_draw_box
    DRAW_FRAME,      // canvas_draw_frame
    DRAW_CIRCLE,     // canvas_draw_circle
    DRAW_XBM,        // canvas_draw_xbm
                     // custom draw objects
    DRAW_ICON_EARTH, // 	canvas_draw_icon
    DRAW_ICON_HOME,  // 	canvas_draw_icon
    DRAW_ICON_INFO,  // 	canvas_draw_icon
    DRAW_ICON_MAN,   // 	canvas_draw_man
    DRAW_ICON_PLANT, // 	canvas_draw_icon
    DRAW_ICON_TREE,  // 	canvas_draw_icon
    DRAW_ICON_WOMAN, // 	canvas_draw_icon
} FlipWorldDrawObjects;