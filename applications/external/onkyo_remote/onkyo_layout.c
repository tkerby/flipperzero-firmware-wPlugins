#include "onkyo_layout.h"

#include <furi.h>
#include <onkyo_remote_icons.h>

#define ONKYO_ROW_COUNT 11
#define ONKYO_ICON_SIZE 16

/** Kept clear above and below the focused button when scrolling. */
#define ONKYO_SCROLL_MARGIN 2

/** The "INPUT" section header, which is a label and never takes focus. */
#define ONKYO_HEADER_Y      112
#define ONKYO_HEADER_HEIGHT 10
#define ONKYO_HEADER_TEXT   "INPUT"

typedef struct {
    OnkyoCmdId cmd;
    const Icon* icon; /**< NULL for a text button */
    const char* label; /**< NULL for an icon button */
    uint8_t x;
    uint8_t y; /**< content space, not screen space */
    uint8_t w;
    uint8_t h;
    uint8_t row;
    uint8_t col;
} OnkyoButton;

#define ONKYO_ICON_BUTTON(command, icon, x, y, w, row, col) \
    {(command), (icon), NULL, (x), (y), (w), 20, (row), (col)}

#define ONKYO_TEXT_BUTTON(command, text, y, row) \
    {(command), NULL, (text), 2, (y), 60, 13, (row), 0}

static const OnkyoButton onkyo_buttons[] = {
    ONKYO_ICON_BUTTON(OnkyoCmdPower, &I_power_16x16, 2, 2, 29, 0, 0),
    ONKYO_ICON_BUTTON(OnkyoCmdMute, &I_mute_16x16, 33, 2, 29, 0, 1),
    ONKYO_ICON_BUTTON(OnkyoCmdPlay, &I_play_pause_16x16, 2, 24, 60, 1, 0),
    ONKYO_ICON_BUTTON(OnkyoCmdVolDown, &I_vol_down_16x16, 2, 46, 29, 2, 0),
    ONKYO_ICON_BUTTON(OnkyoCmdVolUp, &I_vol_up_16x16, 33, 46, 29, 2, 1),
    ONKYO_ICON_BUTTON(OnkyoCmdPrev, &I_prev_16x16, 2, 68, 29, 3, 0),
    ONKYO_ICON_BUTTON(OnkyoCmdNext, &I_next_16x16, 33, 68, 29, 3, 1),
    ONKYO_ICON_BUTTON(OnkyoCmdFlat, &I_flat_16x16, 2, 90, 60, 4, 0),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputUsb, "USB", 124, 5),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputBluetooth, "Bluetooth", 139, 6),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputHdmi, "HDMI", 154, 7),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputAux, "AUX", 169, 8),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputRca, "RCA", 184, 9),
    ONKYO_TEXT_BUTTON(OnkyoCmdInputOpt, "OPT", 199, 10),
};

/** Columns per row, indexed by row. */
static const uint8_t onkyo_row_columns[ONKYO_ROW_COUNT] = {2, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1};

static const OnkyoButton* onkyo_button_at(uint8_t row, uint8_t col) {
    for(size_t i = 0; i < COUNT_OF(onkyo_buttons); i++) {
        if(onkyo_buttons[i].row == row && onkyo_buttons[i].col == col) {
            return &onkyo_buttons[i];
        }
    }
    return NULL;
}

/** Scroll the focused button fully into view, margin included. */
static void onkyo_scroll_to_focus(OnkyoViewModel* model) {
    const OnkyoButton* button = onkyo_button_at(model->row, model->col);
    furi_check(button);

    int16_t scroll = model->scroll;
    if(button->y - scroll < ONKYO_SCROLL_MARGIN) {
        scroll = button->y - ONKYO_SCROLL_MARGIN;
    }
    if(button->y + button->h - scroll > ONKYO_SCREEN_HEIGHT - ONKYO_SCROLL_MARGIN) {
        scroll = button->y + button->h - (ONKYO_SCREEN_HEIGHT - ONKYO_SCROLL_MARGIN);
    }

    if(scroll < 0) scroll = 0;
    if(scroll > ONKYO_SCROLL_MAX) scroll = ONKYO_SCROLL_MAX;
    model->scroll = (uint8_t)scroll;
}

void onkyo_layout_init(OnkyoViewModel* model) {
    model->row = 0;
    model->col = 0;
    model->sticky_col = 0;
    model->scroll = 0;
    model->sending = false;
}

void onkyo_layout_move(OnkyoViewModel* model, int8_t d_row, int8_t d_col) {
    if(d_col != 0) {
        const int8_t col = (int8_t)model->col + d_col;
        if(col < 0 || col >= (int8_t)onkyo_row_columns[model->row]) return;
        model->col = (uint8_t)col;
        /* An explicit sideways press is what sets the column to come back to. */
        model->sticky_col = model->col;
    } else if(d_row != 0) {
        const int8_t row = (int8_t)model->row + d_row;
        if(row < 0 || row >= ONKYO_ROW_COUNT) return;
        model->row = (uint8_t)row;
        const uint8_t columns = onkyo_row_columns[model->row];
        model->col = (model->sticky_col < columns) ? model->sticky_col : columns - 1;
    }

    onkyo_scroll_to_focus(model);
}

OnkyoCmdId onkyo_layout_focused_command(const OnkyoViewModel* model) {
    const OnkyoButton* button = onkyo_button_at(model->row, model->col);
    furi_check(button);
    return button->cmd;
}

static void onkyo_draw_header(Canvas* canvas, const OnkyoViewModel* model) {
    const int16_t y = ONKYO_HEADER_Y - model->scroll;
    if(y + ONKYO_HEADER_HEIGHT < 0 || y >= ONKYO_SCREEN_HEIGHT) return;

    const int16_t middle = y + ONKYO_HEADER_HEIGHT / 2;
    canvas_draw_str_aligned(canvas, 2, middle, AlignLeft, AlignCenter, ONKYO_HEADER_TEXT);

    const int16_t rule = 2 + canvas_string_width(canvas, ONKYO_HEADER_TEXT) + 3;
    canvas_draw_line(canvas, rule, middle, ONKYO_SCREEN_WIDTH - 3, middle);
}

void onkyo_layout_draw(Canvas* canvas, const OnkyoViewModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    onkyo_draw_header(canvas, model);

    for(size_t i = 0; i < COUNT_OF(onkyo_buttons); i++) {
        const OnkyoButton* button = &onkyo_buttons[i];
        const int16_t y = (int16_t)button->y - model->scroll;
        if(y + button->h < 0 || y >= ONKYO_SCREEN_HEIGHT) continue;

        const bool focused = (button->row == model->row) && (button->col == model->col);

        if(focused && model->sending) {
            /* Grow by a pixel on every side: a press that is visible while the
             * app thread is blocked inside infrared_send(). */
            canvas_draw_rbox(canvas, button->x - 1, y - 1, button->w + 2, button->h + 2, 3);
        } else if(focused) {
            canvas_draw_rbox(canvas, button->x, y, button->w, button->h, 2);
        } else {
            canvas_draw_rframe(canvas, button->x, y, button->w, button->h, 2);
        }

        if(focused) canvas_set_color(canvas, ColorWhite);

        if(button->icon) {
            canvas_draw_icon(
                canvas,
                button->x + (button->w - ONKYO_ICON_SIZE) / 2,
                y + (button->h - ONKYO_ICON_SIZE) / 2,
                button->icon);
        } else {
            canvas_draw_str_aligned(
                canvas,
                button->x + button->w / 2,
                y + button->h / 2,
                AlignCenter,
                AlignCenter,
                button->label);
        }

        if(focused) canvas_set_color(canvas, ColorBlack);
    }
}
