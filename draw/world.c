#include <draw/world.h>

void draw_world_example(Canvas *canvas)
{
    // Draw other elements adjusted by camera offset
    // Static Dot at (72, 40)
    canvas_draw_dot(canvas, 72 - camera_x, 40 - camera_y);

    // Static Circle at (16, 16) with radius 4
    canvas_draw_circle(canvas, 16 - camera_x, 16 - camera_y, 4);

    // Static 8x8 Rectangle Frame at (96, 48)
    canvas_draw_frame(canvas, 96 - camera_x, 48 - camera_y, 8, 8);

    // Static earth icon at (112, 56)
    canvas_draw_icon(canvas, 112 - camera_x, 56 - camera_y, &I_icon_earth);

    // static home icon at (128, 24)
    canvas_draw_icon(canvas, 128 - camera_x, 24 - camera_y, &I_icon_home);

    // static menu icon at (144, 24)
    canvas_draw_icon(canvas, 144 - camera_x, 24 - camera_y, &I_icon_info);

    // static man icon at (160, 56)
    canvas_draw_icon(canvas, 160 - camera_x, 56 - camera_y, &I_icon_man);

    // static woman icon at (208, 56)
    canvas_draw_icon(canvas, 168 - camera_x, 56 - camera_y, &I_icon_woman);

    // static plant icon at (168, 32)
    canvas_draw_icon(canvas, 168 - camera_x, 32 - camera_y, &I_icon_plant);

    // tree world
    draw_icon_half_world(canvas, true, &I_icon_tree);

    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}