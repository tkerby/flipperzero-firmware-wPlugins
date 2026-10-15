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
    static const char* main_labels[] = {
        "Return",
        "Restart",
        "Play New Song",
        "Difficulty",
        "Playback Speed",
        "Scroll Speed",
        "Backward Interval",
        "Forward Interval",
        "Dong Volume",
        "Ka Volume",
        "GPIO Output",
        "GPIO Setting",
        "Restore Defaults",
        "Exit"};
    static const char* gpio_labels[] = {
        "Return", "Left Ka", "Left Dong", "Right Dong", "Right Ka"};
    const char* const* labels = menu->gpio_settings ? gpio_labels : main_labels;
    const size_t count = menu->gpio_settings ? TaikoGpioMenuItemCount : TaikoMenuItemCount;
    const size_t selection = menu->gpio_settings ? (size_t)menu->gpio_selected :
                                                   (size_t)menu->selected;
    const size_t visible = 5;
    const size_t first = selection >= visible ? selection - visible + 1 : 0;
    for(size_t i = first; i < first + visible && i < count; i++) {
        const int y = 10 + (i - first) * 12;
        const bool selected = i == selection;
        if(selected) {
            canvas_draw_rbox(canvas, 1, y - 9, 122, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y, labels[i]);
        char value[16] = "";
        if(menu->gpio_settings) {
            if(i != TaikoGpioMenuReturn)
                snprintf(
                    value, sizeof(value), "< %s >", taiko_gpio_pin_name(menu->gpio.pins[i - 1]));
        } else if(i == TaikoMenuGpioOutput) {
            snprintf(value, sizeof(value), "< %s >", menu->gpio.enabled ? "On" : "Off");
        } else if(i == TaikoMenuGpioSetting) {
            snprintf(value, sizeof(value), ">");
        } else if(i >= TaikoMenuDifficulty && i <= TaikoMenuKaVolume) {
            if(i == TaikoMenuDifficulty) {
                snprintf(value, sizeof(value), "< %s >", tja_course_name(menu->course));
            } else if(i == TaikoMenuPlaybackSpeed || i == TaikoMenuScrollSpeed) {
                unsigned speed = i == TaikoMenuPlaybackSpeed ? taiko_menu_playback_tenths(menu) :
                                                               taiko_menu_scroll_tenths(menu);
                snprintf(value, sizeof(value), "< %u.%ux >", speed / 10, speed % 10);
            } else if(i == TaikoMenuBackward || i == TaikoMenuForward) {
                const uint32_t interval = i == TaikoMenuBackward ? taiko_menu_backward_ms(menu) :
                                                                   taiko_menu_forward_ms(menu);
                snprintf(value, sizeof(value), "< %lus >", interval / 1000);
            } else {
                unsigned volume = i == TaikoMenuDonVolume ? menu->don_volume : menu->ka_volume;
                snprintf(value, sizeof(value), "< %u%% >", volume);
            }
        }
        if(*value) canvas_draw_str(canvas, 120 - canvas_string_width(canvas, value), y, value);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, selection, count);
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

/* A narrow neck opens into a rounded bulb, following the incoming side. */
static void draw_balloon_tail(Canvas* canvas, int x, int direction, int radius) {
    static const uint8_t extent[] = {2, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 4, 4, 3, 2, 1};
    if(!direction) direction = 1;
    for(int pass = 0; pass < 3; pass++) {
        canvas_set_color(canvas, pass == 1 ? ColorBlack : ColorWhite);
        for(int i = pass; i < (int)sizeof(extent) - pass; i++) {
            int height = extent[i];
            for(int j = i - pass; j <= i + pass; j++)
                if(extent[j] < height) height = extent[j];
            height -= pass;
            if(height < 0) continue;
            int column = x + direction * (radius - 1 + i);
            if(column < 0 || column > 127) continue;
            canvas_draw_line(canvas, column, LANE_Y - height, column, LANE_Y + height);
        }
    }
}

static void draw_balloon_head(Canvas* canvas, int x) {
    lane_disc(canvas, x, TAIKO_LANE_BIG_RADIUS, ColorWhite);
    lane_disc(canvas, x, TAIKO_LANE_BIG_RADIUS - 1, ColorBlack);
    lane_disc(canvas, x, TAIKO_LANE_BIG_RADIUS - 2, ColorWhite);
}

static void draw_balloon_note(Canvas* canvas, const TaikoLaneRoll* note) {
    int direction = note->tail_x < note->x ? -1 : 1;
    draw_balloon_tail(canvas, note->x, direction, TAIKO_LANE_BIG_RADIUS);
    draw_balloon_head(canvas, note->x);
}

static void draw_balloon_counter(Canvas* canvas, const TaikoPlayerView* view) {
    if(view->balloon.remaining && view->balloon.total) {
        char count[12], total[12];
        snprintf(count, sizeof(count), "%lu", view->balloon.remaining);
        snprintf(total, sizeof(total), "%lu", view->balloon.total);
        /* Size depends on the original count, so crossing a digit boundary never
         * shrinks the tail. The bulb and neck inflate; the head stays fixed. */
        const int inflation =
            (view->balloon.total - view->balloon.remaining) * 6 / view->balloon.total;
        int radius = canvas_string_width(canvas, total) / 2 + 2;
        if(radius < 7) radius = 7;
        radius += inflation;
        if(radius > 18) radius = 18;
        int direction = view->balloon.direction < 0 ? -1 : 1;
        const int reach = TAIKO_LANE_BIG_RADIUS + 4 + 2 * radius + 1;
        /* The counter must stay readable even for a reverse-scroll balloon. */
        if(TAIKO_LANE_HIT_X + direction * reach < 0 || TAIKO_LANE_HIT_X + direction * reach > 127)
            direction = -direction;
        const int center = TAIKO_LANE_HIT_X + direction * (TAIKO_LANE_BIG_RADIUS + 4 + radius);
        int neck_start = TAIKO_LANE_HIT_X + direction * (TAIKO_LANE_BIG_RADIUS - 1);
        int neck_end = center - direction * (radius / 2);
        int neck_length = direction * (neck_end - neck_start);
        for(int pass = 0; pass < 3; pass++) {
            canvas_set_color(canvas, pass == 1 ? ColorBlack : ColorWhite);
            for(int step = 0; step <= neck_length; step++) {
                int height =
                    2 + inflation / 2 + (inflation - inflation / 2) * step / neck_length - pass;
                int x = neck_start + direction * step;
                canvas_draw_line(canvas, x, LANE_Y - height, x, LANE_Y + height);
            }
            /* Paint the neck and bulb as one shape in each border layer, so the
             * join has no circular outline cutting through the white center. */
            lane_disc(canvas, center, radius + 1 - pass, pass == 1 ? ColorBlack : ColorWhite);
        }
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(
            canvas, center - canvas_string_width(canvas, count) / 2, LANE_Y + 3, count);
        draw_balloon_head(canvas, TAIKO_LANE_HIT_X);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_gogo_background(Canvas* canvas, const TaikoPlayerView* view) {
    if(!view->gogo) return;
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static const uint8_t density[] = {7, 4, 2, 1};
    static const int8_t jitter[] = {0, 1, 0, -1, 0, 0, 1, -1};
    /* Source time keeps the animation still on pause and deterministic on seek.
     * Four dithered rows jitter by one pixel, underneath the notes and target. */
    const uint32_t phase = view->position_ms / 80;
    canvas_set_color(canvas, ColorWhite);
    for(int x = 0; x < 128; x++) {
        const int offset = jitter[((x / 3) + phase) & 7];
        for(int row = 0; row < 4; row++) {
            const int y = LANE_TOP + row + offset;
            if(y < LANE_TOP || y > LANE_TOP + 4) continue;
            if(bayer[(row + phase / 2) & 3][(x + phase) & 3] < density[row])
                canvas_draw_dot(canvas, x, y);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_lane(Canvas* canvas, const TaikoPlayerView* view) {
    canvas_draw_box(canvas, 0, LANE_TOP, 128, LANE_BOTTOM - LANE_TOP + 1);
    draw_gogo_background(canvas, view);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 7);
    canvas_draw_circle(canvas, TAIKO_LANE_HIT_X, LANE_Y, 5);
    for(size_t i = view->roll_count; i > 0; i--) {
        TaikoLaneRoll roll = view->lane_rolls[i - 1];
        if(roll.kind == 7 || roll.kind == 9) {
            draw_balloon_note(canvas, &roll);
            continue;
        }
        int radius = roll.kind == 5 ? TAIKO_LANE_SMALL_RADIUS : TAIKO_LANE_BIG_RADIUS;
        int left = roll.x < roll.tail_x ? roll.x : roll.tail_x;
        int right = roll.x > roll.tail_x ? roll.x : roll.tail_x;
        /* White center with a black outline and a one-pixel white outer halo. */
        for(int pass = 0; pass < 3; pass++) {
            int r = radius - pass;
            canvas_set_color(canvas, pass == 1 ? ColorBlack : ColorWhite);
            for(int y = -r; y <= r; y++) {
                int extent = r;
                while(extent * extent + y * y > r * r)
                    extent--;
                int x1 = left - extent, x2 = right + extent;
                if(x2 < 0 || x1 > 127) continue;
                if(x1 < 0) x1 = 0;
                if(x2 > 127) x2 = 127;
                canvas_draw_line(canvas, x1, LANE_Y + y, x2, LANE_Y + y);
            }
        }
    }
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

    char status[24];
    if(!view->bpm_tenths)
        snprintf(status, sizeof(status), "BPM: --");
    else if(view->bpm_tenths % 10)
        snprintf(
            status, sizeof(status), "BPM: %lu.%lu", view->bpm_tenths / 10, view->bpm_tenths % 10);
    else
        snprintf(status, sizeof(status), "BPM: %lu", view->bpm_tenths / 10);
    furi_string_set(text, status);
    elements_string_fit_width(canvas, text, 49);
    canvas_draw_str(canvas, 38, 39, furi_string_get_cstr(text));
    unsigned speed = taiko_menu_scroll_tenths(&view->menu);
    snprintf(status, sizeof(status), "Speed: %u.%u", speed / 10, speed % 10);
    canvas_draw_str(canvas, 38, 49, status);

    char time[16];
    uint32_t seconds = view->position_ms / 1000;
    snprintf(time, sizeof(time), "%lu:%02lu", seconds / 60, seconds % 60);
    canvas_draw_str(canvas, 89, 39, time);
    furi_string_set(text, view->course);
    elements_string_fit_width(canvas, text, 31);
    canvas_draw_str(canvas, 89, 49, furi_string_get_cstr(text));
    furi_string_free(text);
    canvas_draw_frame(canvas, 123, 30, 5, 21);
    const int volume_height = (view->volume * 19 + 50) / 100;
    if(volume_height) canvas_draw_box(canvas, 124, 50 - volume_height, 3, volume_height);
    draw_control(canvas, "BWD", AlignLeft);
    draw_control(
        canvas,
        view->loading                  ? "..." :
        view->paused || view->finished ? "Play" :
                                         "Pause",
        AlignCenter);
    draw_control(canvas, "FWD", AlignRight);
    /* The enlarged balloon is a popup above the lane and drum indicator. */
    draw_balloon_counter(canvas, view);
}
