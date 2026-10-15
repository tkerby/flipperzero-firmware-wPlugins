#include "taiko_player_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>

#define LANE_TOP          11
#define LANE_BOTTOM       27
#define LANE_Y            19
#define DRUM_X            18
#define DRUM_Y            40
#define DRUM_INNER_RADIUS 7
#define DRUM_OUTER_RADIUS 11

static void draw_menu(Canvas* canvas, const TaikoMenu* menu) {
    static const char* labels[] = {
        "Return",
        "Restart",
        "Backward Interval",
        "Forward Interval",
        "Dong Volume",
        "Ka Volume",
        "Exit"};
    const size_t visible = 5;
    const size_t first = menu->selected >= visible ? menu->selected - visible + 1 : 0;
    for(size_t i = first; i < first + visible && i < TaikoMenuItemCount; i++) {
        const int y = 10 + (i - first) * 12;
        const bool selected = i == menu->selected;
        if(selected) {
            canvas_draw_rbox(canvas, 1, y - 9, 122, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y, labels[i]);
        if(i == TaikoMenuBackward || i == TaikoMenuForward || i == TaikoMenuDonVolume ||
           i == TaikoMenuKaVolume) {
            char value[16];
            if(i == TaikoMenuBackward || i == TaikoMenuForward) {
                const uint32_t interval = i == TaikoMenuBackward ? taiko_menu_backward_ms(menu) :
                                                                   taiko_menu_forward_ms(menu);
                snprintf(value, sizeof(value), "< %lus >", interval / 1000);
            } else {
                unsigned volume = i == TaikoMenuDonVolume ? menu->don_volume : menu->ka_volume;
                snprintf(value, sizeof(value), "< %u%% >", volume);
            }
            canvas_draw_str(canvas, 120 - canvas_string_width(canvas, value), y, value);
        }
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, menu->selected, TaikoMenuItemCount);
}

static void draw_control(Canvas* canvas, const char* text, Align align) {
    static const uint8_t left[] = {0x08, 0x0c, 0x0e, 0x0f, 0x0e, 0x0c, 0x08};
    static const uint8_t right[] = {0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01};
    static const uint8_t center[] = {0x1c, 0x22, 0x5d, 0x5d, 0x5d, 0x22, 0x1c};
    const uint8_t* icon = align == AlignLeft ? left : align == AlignRight ? right : center;
    const int padding = align == AlignCenter ? 1 : 3;
    const int icon_width = align == AlignCenter ? 7 : 4;
    const int width = canvas_string_width(canvas, text) + padding * 2 + icon_width + 3;
    /* Native buttons are bottom-aligned; use a baseline one pixel lower. */
    const int bottom = canvas_height(canvas) + 1;
    const int top = bottom - 12;
    const int x = align == AlignLeft  ? 0 :
                  align == AlignRight ? (int)canvas_width(canvas) - width :
                                        ((int)canvas_width(canvas) - width) / 2;
    canvas_draw_box(canvas, x, top, width, 12);
    for(int i = 0; i < 3; i++) {
        if(align != AlignLeft) canvas_draw_line(canvas, x - 1 - i, top + i, x - 1 - i, bottom);
        if(align != AlignRight)
            canvas_draw_line(canvas, x + width + i, top + i, x + width + i, bottom);
    }
    canvas_invert_color(canvas);
    const int icon_x = align == AlignRight ? x + width - padding - icon_width : x + padding;
    const int text_x = align == AlignRight ? x + padding : x + padding + icon_width + 3;
    canvas_draw_xbm(canvas, icon_x, bottom - 10, icon_width, 7, icon);
    canvas_draw_str(canvas, text_x, bottom - 3, text);
    canvas_invert_color(canvas);
}

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
    if(view->menu.open) {
        draw_menu(canvas, &view->menu);
        return;
    }
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
    elements_string_fit_width(canvas, text, 36);
    canvas_draw_str(canvas, 84, 49, furi_string_get_cstr(text));
    furi_string_free(text);
    canvas_draw_frame(canvas, 123, 30, 5, 21);
    const int volume_height = (view->volume * 19 + 50) / 100;
    if(volume_height) canvas_draw_box(canvas, 124, 50 - volume_height, 3, volume_height);
    if(view->loading || view->finished)
        canvas_draw_str(canvas, 42, 45, view->loading ? "Load" : "End");
    draw_control(canvas, "BWD", AlignLeft);
    draw_control(
        canvas,
        view->loading                  ? "..." :
        view->paused || view->finished ? "Play" :
                                         "Pause",
        AlignCenter);
    draw_control(canvas, "FWD", AlignRight);
}
