#include <draw/world.h>

static void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

void draw_example_world(Canvas *canvas)
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

    // Draw the bounds
    draw_bounds(canvas);
}

void draw_tree_world(Canvas *canvas)
{
    // two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        draw_icon_line(canvas, (Vector){5, 2 + i * 17}, 22, true, &I_icon_tree);  // horizontal
        draw_icon_line(canvas, (Vector){5 + i * 17, 2}, 11, false, &I_icon_tree); // vertical
    }

    // two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        draw_icon_line(canvas, (Vector){5, 2 + i * 17}, 22, true, &I_icon_tree); // horizontal
    }
    // two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        draw_icon_line(canvas, (Vector){5 + i * 17, 50}, 8, false, &I_icon_tree); // vertical
    }

    // draw labyinth line-by-line

    // third line (14 left, 3 middle, 0 right) - exit line
    draw_icon_line(canvas, (Vector){5, 2 + 2 * 17}, 14, true, &I_icon_tree);          // 14 left
    draw_icon_line(canvas, (Vector){5 + 16 * 17, 2 + 2 * 17}, 3, true, &I_icon_tree); // 3 middle

    // fourth line (3 left, 6 middle, 4 right)
    draw_icon_line(canvas, (Vector){5, 2 + 3 * 17}, 3, true, &I_icon_tree);           // 3 left
    draw_icon_line(canvas, (Vector){5 + 7 * 17, 2 + 3 * 17}, 6, true, &I_icon_tree);  // 6 middle
    draw_icon_line(canvas, (Vector){5 + 15 * 17, 2 + 3 * 17}, 4, true, &I_icon_tree); // 4 right

    // fifth line (6 left, 7 middle, 0 right)
    draw_icon_line(canvas, (Vector){5, 2 + 4 * 17}, 6, true, &I_icon_tree);          // 6 left
    draw_icon_line(canvas, (Vector){5 + 7 * 17, 2 + 4 * 17}, 7, true, &I_icon_tree); // 7 middle

    // sixth line (5 left, 6 middle, 7 right)
    draw_icon_line(canvas, (Vector){5, 2 + 5 * 17}, 5, true, &I_icon_tree);           // 5 left
    draw_icon_line(canvas, (Vector){5 + 7 * 17, 2 + 5 * 17}, 3, true, &I_icon_tree);  // 3 middle
    draw_icon_line(canvas, (Vector){5 + 15 * 17, 2 + 5 * 17}, 7, true, &I_icon_tree); // 4 right

    // seventh line (0 left, 7 middle, 4 right)
    draw_icon_line(canvas, (Vector){5 + 6 * 17, 2 + 6 * 17}, 7, true, &I_icon_tree);  // 7 middle
    draw_icon_line(canvas, (Vector){5 + 14 * 17, 2 + 6 * 17}, 4, true, &I_icon_tree); // 4 right

    // eighth line (4 left, 3 middle, 4 right)
    draw_icon_line(canvas, (Vector){5, 2 + 7 * 17}, 4, true, &I_icon_tree);           // 4 left
    draw_icon_line(canvas, (Vector){5 + 7 * 17, 2 + 7 * 17}, 3, true, &I_icon_tree);  // 3 middle
    draw_icon_line(canvas, (Vector){5 + 15 * 17, 2 + 7 * 17}, 4, true, &I_icon_tree); // 4 right

    // ninth line (3 left, 2 middle, 3 right)
    draw_icon_line(canvas, (Vector){5, 2 + 8 * 17}, 3, true, &I_icon_tree);           // 3 left
    draw_icon_line(canvas, (Vector){5 + 5 * 17, 2 + 8 * 17}, 1, true, &I_icon_tree);  // 2 middle
    draw_icon_line(canvas, (Vector){5 + 11 * 17, 2 + 8 * 17}, 3, true, &I_icon_tree); // 3 right

    // Draw the bounds
    draw_bounds(canvas);
}