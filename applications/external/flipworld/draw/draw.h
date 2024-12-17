#pragma once
#include "engine/engine.h"
#include "flip_world.h"
#include "flip_world_icons.h"

typedef enum {
    // system draw objects
    DRAW_DOT, // canvas_draw_dot
    DRAW_LINE, // canvas_draw_line
    DRAW_BOX, // canvas_draw_box
    DRAW_FRAME, // canvas_draw_frame
    DRAW_CIRCLE, // canvas_draw_circle
    DRAW_XBM, // canvas_draw_xbm
        // custom draw objects
    DRAW_ICON_EARTH, // 	canvas_draw_icon
    DRAW_ICON_HOME, // 	canvas_draw_icon
    DRAW_ICON_INFO, // 	canvas_draw_icon
    DRAW_ICON_MAN, // 	canvas_draw_man
    DRAW_ICON_PLANT, // 	canvas_draw_icon
    DRAW_ICON_TREE, // 	canvas_draw_icon
    DRAW_ICON_WOMAN, // 	canvas_draw_icon
} FlipWorldDrawObjects;

// Global variables to store camera position
extern int camera_x;
extern int camera_y;

void draw_icon_line(Canvas* canvas, Vector pos, int amount, bool horizontal, const Icon* icon);
void draw_icon_half_world(Canvas* canvas, bool right, const Icon* icon);

// create custom icons at https://lopaka.app/sandbox
