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

void draw_hand(Canvas* canvas, uint32_t ticks) {
    uint32_t frame_index = (ticks / 1000) % A_petting_hand.frame_count;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_petting_hand_mask.width,
        A_petting_hand_mask.height,
        A_petting_hand_mask.frames[frame_index]);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_petting_hand.width,
        A_petting_hand.height,
        A_petting_hand.frames[frame_index]);
}

void draw_petting_face(Canvas* canvas, uint32_t ticks, uint32_t petting_duration) {
    const uint32_t petting_phase_location = ticks * 100 / petting_duration;
    const uint32_t frame1_start = 60;
    const uint32_t frame1_end = 90;
    const uint8_t frame_index =
        petting_phase_location >= frame1_start && petting_phase_location < frame1_end ? 1 : 0;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_petting_face_mask.width,
        A_petting_face_mask.height,
        A_petting_face_mask.frames[frame_index]);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_petting_face.width,
        A_petting_face.height,
        A_petting_face.frames[frame_index]);
}

void draw_petting(Canvas* canvas, uint32_t ticks, uint32_t petting_duration) {
    draw_petting_face(canvas, ticks, petting_duration);
    draw_hand(canvas, ticks);
}

void draw_happy_idle(Canvas* canvas, uint32_t ticks) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        I_happy_face_mask.width,
        I_happy_face_mask.height,
        I_happy_face_mask.frames[0]);

    const uint32_t cycle_duration_ms = 10000;
    const uint32_t frame0_duration_ms = 9000;
    uint32_t cycle_time = ticks % cycle_duration_ms;

    // Choose the frame based on the elapsed time in the cycle.
    uint8_t happy_frame = (cycle_time < frame0_duration_ms) ? 0 : 1;

    // Draw the overlay with the chosen frame.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas, 0, 0, A_happy_idle.width, A_happy_idle.height, A_happy_idle.frames[happy_frame]);
}

void draw_neutral_idle(Canvas* canvas, uint32_t ticks) {
    const uint32_t cycle_duration_ms = 10000;
    const uint32_t frame0_duration_ms = 9000;
    uint32_t cycle_time = ticks % cycle_duration_ms;

    // Choose the frame based on the elapsed time in the cycle.
    uint8_t frame_index = (cycle_time < frame0_duration_ms) ? 0 : 1;

    // Draw the overlay with the chosen frame.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        A_neutral_idle.width,
        A_neutral_idle.height,
        A_neutral_idle.frames[frame_index]);
}

void draw_sad_idle(Canvas* canvas, uint32_t ticks) {
    uint32_t frame_index = (ticks / 1000) % A_sad_idle.frame_count;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas, 0, 0, I_sad_face_mask.width, I_sad_face_mask.height, I_sad_face_mask.frames[0]);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas, 0, 0, A_sad_idle.width, A_sad_idle.height, A_sad_idle.frames[frame_index]);
}

void draw_prompt_box(Canvas* canvas) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 96, 32, 32, 32);
    canvas_set_color(canvas, ColorBlack);
}

void draw_pet_prompt(Canvas* canvas, uint32_t ticks, bool pet_limit_reached) {
    bool draw = (ticks / 2000) % 2;

    if(draw) {
        draw_prompt_box(canvas);
        canvas_draw_str(canvas, 100, 41, "Press");
        canvas_draw_str(canvas, 106, 50, "OK");
        canvas_draw_str(canvas, 100, 59, "to pet");
    }

    if(pet_limit_reached) {
        draw_prompt_box(canvas);
        canvas_draw_str(canvas, 106, 41, "Out");
        canvas_draw_str(canvas, 110, 50, "of");
        canvas_draw_str(canvas, 105, 59, "pets");
    }
}

void draw_speech_bubble(Canvas* canvas) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_bitmap(
        canvas,
        0,
        0,
        I_speech_bubble_mask.width,
        I_speech_bubble_mask.height,
        I_speech_bubble_mask.frames[0]);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(
        canvas, 0, 0, I_speech_bubble.width, I_speech_bubble.height, I_speech_bubble.frames[0]);
}
