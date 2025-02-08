#include "draw_animations.h"
#include "compiled/assets_icons.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>

void draw_base(Canvas* canvas, uint32_t ticks) {
    uint32_t frame_index = (ticks / 1000) % A_base_animation.frame_count;

    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_base_animation.width,
        A_base_animation.height,
        A_base_animation.frames[frame_index]);
}

// void draw_petting(Canvas* canvas, uint32_t ticks) {

// }
void draw_happy_idle(Canvas* canvas, uint32_t ticks) {
    canvas_set_bitmap_mode(canvas, 1);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        I_happy_face_mask.width,
        I_happy_face_mask.height,
        I_happy_face_mask.frames[0]);

    const uint32_t cycle_duration_ms = 10000;
    const uint32_t frame0_duration_ms  = 9000;
    uint32_t cycle_time = ticks % cycle_duration_ms;

    // Choose the frame based on the elapsed time in the cycle.
    uint8_t happy_frame = (cycle_time < frame0_duration_ms) ? 0 : 1;

    // Draw the overlay with the chosen frame.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_happy_idle.width,
        A_happy_idle.height,
        A_happy_idle.frames[happy_frame]);
}