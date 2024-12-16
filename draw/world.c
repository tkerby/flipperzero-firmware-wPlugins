#include <draw/world.h>

void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

void draw_example_world(Level *level)
{
    spawn_icon(level, &I_icon_earth, 112, 56);
    spawn_icon(level, &I_icon_home, 128, 24);
    spawn_icon(level, &I_icon_info, 144, 24);
    spawn_icon(level, &I_icon_man, 160, 56);
    spawn_icon(level, &I_icon_woman, 168, 56);
    spawn_icon(level, &I_icon_plant, 168, 32);
}

void draw_tree_world(Level *level)
{
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 22; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + j * 17, 2 + i * 17); // Horizontal lines
        }
        for (int j = 0; j < 11; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + j * 17); // Vertical lines
        }
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        for (int j = 0; j < 22; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + j * 17, 2 + i * 17); // Horizontal lines
        }
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + i * 17, 50 + j * 17); // Vertical lines
        }
    }

    // Spawn labyrinth lines
    // Third line (14 left, 3 middle, 0 right) - exit line
    for (int i = 0; i < 14; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 2 * 17);
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 16 * 17 + i * 17, 2 + 2 * 17);
    }

    // Fourth line (3 left, 6 middle, 4 right)
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 3 * 17); // 3 left
    }
    for (int i = 0; i < 6; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 3 * 17); // 6 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 3 * 17); // 4 right
    }

    // Fifth line (6 left, 7 middle, 0 right)
    for (int i = 0; i < 6; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 4 * 17); // 6 left
    }
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 4 * 17); // 7 middle
    }

    // Sixth line (5 left, 6 middle, 7 right)
    for (int i = 0; i < 5; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 5 * 17); // 5 left
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 5 * 17); // 3 middle
    }
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 5 * 17); // 7 right
    }

    // Seventh line (0 left, 7 middle, 4 right)
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 6 * 17 + i * 17, 2 + 6 * 17); // 7 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 14 * 17 + i * 17, 2 + 6 * 17); // 4 right
    }

    // Eighth line (4 left, 3 middle, 4 right)
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 7 * 17); // 4 left
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 7 * 17); // 3 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 7 * 17); // 4 right
    }

    // Ninth line (3 left, 2 middle, 3 right)
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 8 * 17); // 3 left
    }
    for (int i = 0; i < 1; i++) // 2 middle
    {
        spawn_icon(level, &I_icon_tree, 5 + 5 * 17 + i * 17, 2 + 8 * 17);
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 11 * 17 + i * 17, 2 + 8 * 17); // 3 right
    }
}