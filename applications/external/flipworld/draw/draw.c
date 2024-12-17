#include <draw/draw.h>

// Global variables to store camera position
int camera_x = 0;
int camera_y = 0;

// Draw a line of icons (16 width)
void draw_icon_line(Canvas* canvas, Vector pos, int amount, bool horizontal, const Icon* icon) {
    for(int i = 0; i < amount; i++) {
        if(horizontal) {
            // check if element is outside the world
            if(pos.x + (i * 17) > WORLD_WIDTH) {
                break;
            }

            canvas_draw_icon(canvas, pos.x + (i * 17) - camera_x, pos.y - camera_y, icon);
        } else {
            // check if element is outside the world
            if(pos.y + (i * 17) > WORLD_HEIGHT) {
                break;
            }

            canvas_draw_icon(canvas, pos.x - camera_x, pos.y + (i * 17) - camera_y, icon);
        }
    }
}
// Draw a half section of icons (16 width)
void draw_icon_half_world(Canvas* canvas, bool right, const Icon* icon) {
    for(int i = 0; i < 10; i++) {
        if(right) {
            draw_icon_line(canvas, (Vector){WORLD_WIDTH / 2 + 6, i * 19 + 2}, 11, true, icon);
        } else {
            draw_icon_line(canvas, (Vector){0, i * 19 + 2}, 11, true, icon);
        }
    }
}
