#include "ui.h"

const char* get_moisture_status(uint8_t percent) {
    if(percent < 20) return "Very Dry";
    if(percent < 40) return "Dry";
    if(percent < 60) return "Moist";
    if(percent < 80) return "Wet";
    return "Very Wet";
}

static void
    draw_value_row(Canvas* canvas, const char* label, uint16_t value, uint8_t y, bool selected) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %d", label, value);

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 2, y - 9, 124, 12);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 6, y, "<");
        canvas_draw_str_aligned(canvas, 122, y, AlignRight, AlignBottom, ">");
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignBottom, buf);
}

static void draw_menu_row(Canvas* canvas, const char* label, uint8_t y, bool selected) {
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 2, y - 9, 124, 11);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignBottom, label);
}

static void draw_menu_overlay(Canvas* canvas, MoistureSensorApp* app) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Calibration (ADC)");
    canvas_draw_line(canvas, 0, 16, 128, 16);

    canvas_set_font(canvas, FontSecondary);
    draw_value_row(
        canvas, "Dry:", app->edit_dry_value, 27, app->selected_menu_item == MenuItemDryValue);
    draw_value_row(
        canvas, "Wet:", app->edit_wet_value, 38, app->selected_menu_item == MenuItemWetValue);
    draw_menu_row(canvas, "Reset", 49, app->selected_menu_item == MenuItemResetDefaults);
    draw_menu_row(canvas, "Save", 60, app->selected_menu_item == MenuItemSave);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_confirm_overlay(Canvas* canvas, const char* message) {
    uint8_t box_w = 100;
    uint8_t box_h = 20;
    uint8_t box_x = (128 - box_w) / 2;
    uint8_t box_y = (64 - box_h) / 2;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, box_y + box_h / 2, AlignCenter, AlignCenter, message);
}

void draw_callback(Canvas* canvas, void* ctx) {
    MoistureSensorApp* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint16_t mv = app->millivolts;
    uint16_t raw = app->raw_adc;
    uint8_t percent = app->moisture_percent;
    AppState state = app->state;
    const char* confirm_msg = app->confirm_message;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Moisture Sensor");

    canvas_draw_line(canvas, 0, 14, 128, 14);

    char num_str[4];
    snprintf(num_str, sizeof(num_str), "%d", percent);
    canvas_set_font(canvas, FontBigNumbers);
    uint16_t num_width = canvas_string_width(canvas, num_str);
    canvas_set_font(canvas, FontPrimary);
    uint16_t pct_width = canvas_string_width(canvas, "%");
    uint16_t gap = 2;
    uint16_t total_width = num_width + gap + pct_width;
    uint16_t start_x = (128 - total_width) / 2;
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, start_x, 34, num_str);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, start_x + num_width + gap, 34, "%");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, get_moisture_status(percent));

    char info_str[32];
    snprintf(info_str, sizeof(info_str), "%dmV  ADC:%d  Ch:%d", mv, raw, app->adc_channel);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop, info_str);

    uint8_t bar_width = percent;
    canvas_draw_frame(canvas, 14, 58, 100, 5);
    if(bar_width > 0) {
        canvas_draw_box(canvas, 14, 58, bar_width, 5);
    }

    if(state == AppStateMain) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 4, AlignRight, AlignTop, "<");
    }

    if(state == AppStateMenu) {
        draw_menu_overlay(canvas, app);
    } else if(state == AppStateConfirm && confirm_msg) {
        draw_confirm_overlay(canvas, confirm_msg);
    }
}
