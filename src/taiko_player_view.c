#include "taiko_player_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>

#define LANE_TOP          12
#define LANE_BOTTOM       28
#define LANE_Y            20
#define DRUM_X            18
#define DRUM_Y            43
#define DRUM_INNER_RADIUS 7
#define DRUM_OUTER_RADIUS 11

static void draw_hit(Canvas* canvas, bool right, bool rim) {
    const int radius = rim ? DRUM_OUTER_RADIUS : DRUM_INNER_RADIUS;
    const int inner_radius = rim ? DRUM_INNER_RADIUS : 0;
    for(int y = -radius; y <= radius; y++) {
        int end = radius;
        while(end * end + y * y > radius * radius)
            end--;
        int start = 1;
        while(start <= end && start * start + y * y < inner_radius * inner_radius)
            start++;
        if(start > end) continue;
        int x1 = right ? DRUM_X + start : DRUM_X - end;
        int x2 = right ? DRUM_X + end : DRUM_X - start;
        canvas_draw_line(canvas, x1, DRUM_Y + y, x2, DRUM_Y + y);
    }
}

/* Clip each row explicitly so partially visible notes never wrap around the
 * canvas edge or draw into the title and status areas. */
static void lane_disc(Canvas* canvas, int x, int radius, Color color) {
    canvas_set_color(canvas, color);
    if(x >= radius && x + radius < 128) {
        canvas_draw_disc(canvas, x, LANE_Y, radius);
        return;
    }
    for(int y = -radius; y <= radius; y++) {
        int extent = radius;
        while(extent * extent + y * y > radius * radius)
            extent--;
        int left = x - extent;
        int right = x + extent;
        if(right < 0 || left > 127) continue;
        if(left < 0) left = 0;
        if(right > 127) right = 127;
        canvas_draw_line(canvas, left, LANE_Y + y, right, LANE_Y + y);
    }
}

static void draw_lane(Canvas* canvas, const TaikoPlayerView* view) {
    canvas_draw_box(canvas, 0, LANE_TOP, 128, LANE_BOTTOM - LANE_TOP + 1);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 7);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 5);
    for(size_t i = view->lane_count; i > 0; i--) {
        TaikoLaneNote note = view->lane_notes[i - 1];
        int radius = note.kind >= 3 ? TAIKO_LANE_BIG_RADIUS : TAIKO_LANE_SMALL_RADIUS;
        lane_disc(canvas, note.x, radius, ColorWhite);
        /* Thicken large-note borders inward without changing the outer radius. */
        lane_disc(canvas, note.x, radius - (note.kind == 3 ? 2 : 1), ColorBlack);
        if(note.kind == 2 || note.kind == 4)
            lane_disc(canvas, note.x, radius - (note.kind == 4 ? 3 : 2), ColorWhite);
    }
    canvas_set_color(canvas, ColorBlack);
}

void taiko_player_view_draw(Canvas* canvas, const TaikoPlayerView* view) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    FuriString* text = furi_string_alloc_set(view->title);
    elements_string_fit_width(canvas, text, 124);
    canvas_draw_str(canvas, 2, 9, furi_string_get_cstr(text));

    draw_lane(canvas, view);
    uint32_t now = furi_get_tick();
    if(view->hit_until && (int32_t)(view->hit_until - now) > 0)
        draw_hit(canvas, view->hit_right, view->hit_rim);
    canvas_draw_circle(canvas, DRUM_X, DRUM_Y, DRUM_INNER_RADIUS);
    canvas_draw_circle(canvas, DRUM_X, DRUM_Y, DRUM_OUTER_RADIUS);
    canvas_draw_line(
        canvas, DRUM_X, DRUM_Y - DRUM_OUTER_RADIUS, DRUM_X, DRUM_Y + DRUM_OUTER_RADIUS);

    char time[16];
    uint32_t seconds = view->position_ms / 1000;
    snprintf(time, sizeof(time), "%lu:%02lu", seconds / 60, seconds % 60);
    canvas_draw_str(canvas, 84, 39, time);
    furi_string_set(text, view->course);
    elements_string_fit_width(canvas, text, 42);
    canvas_draw_str(canvas, 84, 51, furi_string_get_cstr(text));
    furi_string_free(text);
    if(view->loading || view->finished)
        canvas_draw_str(canvas, 42, 45, view->loading ? "Load" : "End");
    canvas_draw_str_aligned(
        canvas,
        64,
        63,
        AlignCenter,
        AlignBottom,
        view->finished ? "OK: next   Hold Back: exit" : "Hold Back: exit");
}
