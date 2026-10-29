#include "brew_view.h"

#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>

#include <stdio.h>
#include <string.h>

#include "../tea_timer_i.h"

/** Внутреннее состояние экрана: View и переиспользуемая строка названия. */
struct BrewView {
    View* view;
    FuriString* name_string;
};

/** Нарисовать инверсный блок с текстом (белый текст на чёрном фоне). */
static void brew_view_draw_inverse(Canvas* canvas, uint8_t center_x, uint8_t y, const char* text) {
    uint16_t width = canvas_string_width(canvas, text) + 4;
    uint8_t x = (center_x > width / 2) ? (uint8_t)(center_x - width / 2) : 0;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, y, width, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, center_x, y + 1, AlignCenter, AlignTop, text);
    canvas_set_color(canvas, ColorBlack);
}

/** Ширина инверсного блока счётчика при превышении лимита (слева от плашки OVER). */
#define BREW_VIEW_POURS_OVER_WIDTH 92

/**
 * Строка счётчика проливов.
 * При превышении лимита счётчик рисуется инверсным блоком слева, а справа — плашка OVER;
 * блоки не перекрываются даже при длинном числе проливов.
 */
static void brew_view_draw_pours(Canvas* canvas, const BrewViewModel* model, uint8_t y) {
    char buffer[24];
    snprintf(
        buffer,
        sizeof(buffer),
        "Pours %u / %u",
        (unsigned)model->completed_pours,
        (unsigned)model->max_pours);

    canvas_set_font(canvas, FontSecondary);

    if(!model->over_limit) {
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, buffer);
        return;
    }

    // Инверсный блок счётчика.
    uint16_t width = canvas_string_width(canvas, buffer) + 6;
    if(width > BREW_VIEW_POURS_OVER_WIDTH) {
        width = BREW_VIEW_POURS_OVER_WIDTH;
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 2, y, width, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 2 + width / 2, y + 1, AlignCenter, AlignTop, buffer);

    // Плашка OVER справа от счётчика.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 98, y, 28, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 112, y + 1, AlignCenter, AlignTop, "OVER");
    canvas_set_color(canvas, ColorBlack);
}

/** Строка граммовки: остаётся на экране и при длинном названии. */
static void brew_view_draw_leaf(Canvas* canvas, const BrewViewModel* model, uint8_t y) {
    char buffer[28];
    snprintf(
        buffer,
        sizeof(buffer),
        "Leaf %u.%u g/100ml",
        (unsigned)(model->leaf_tenths_g / 10),
        (unsigned)(model->leaf_tenths_g % 10));
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, buffer);
}

static void brew_view_draw_callback(Canvas* canvas, void* model_ptr) {
    BrewViewModel* model = model_ptr;
    furi_assert(model != NULL);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Название чая с обрезкой по ширине экрана.
    if(model->name_string != NULL) {
        furi_string_set(model->name_string, model->name);
        canvas_set_font(canvas, FontSecondary);
        elements_string_fit_width(canvas, model->name_string, 124);
        canvas_draw_str_aligned(
            canvas, 64, 0, AlignCenter, AlignTop, furi_string_get_cstr(model->name_string));
    }

    char buffer[24];

    if(model->phase == BrewPhaseRUNNING) {
        brew_view_draw_leaf(canvas, model, 8);

        brew_math_format_time(model->remaining_sec, buffer, sizeof(buffer));
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, buffer);

        brew_view_draw_pours(canvas, model, 37);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "OK: stop");
    } else {
        // DONE: время истекло, следующий пролив запускается вручную.
        brew_view_draw_leaf(canvas, model, 8);

        brew_view_draw_inverse(canvas, 64, 17, "Time is up!");

        brew_view_draw_pours(canvas, model, 31);
        canvas_set_font(canvas, FontSecondary);

        brew_math_format_time(model->pour_sec, buffer, sizeof(buffer));
        char pour_line[40];
        snprintf(
            pour_line, sizeof(pour_line), "Pour %u  %s", (unsigned)model->pour_number, buffer);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, pour_line);

        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "OK: start");
    }
}

static bool brew_view_input_callback(InputEvent* event, void* context) {
    TeaTimerApp* app = context;
    furi_assert(app != NULL);

    if(event->type != InputTypeShort || event->key != InputKeyOk) {
        return false;
    }

    // Состояние сессии меняет только сцена: view пересылает событие.
    uint32_t custom_event = (app->session.phase == BrewPhaseRUNNING) ? TeaTimerEventBrewStop :
                                                                       TeaTimerEventBrewStart;
    view_dispatcher_send_custom_event(app->view_dispatcher, custom_event);
    return true;
}

BrewView* brew_view_alloc(void) {
    BrewView* brew_view = malloc(sizeof(BrewView));
    brew_view->view = view_alloc();
    brew_view->name_string = furi_string_alloc();

    view_allocate_model(brew_view->view, ViewModelTypeLockFree, sizeof(BrewViewModel));
    view_set_draw_callback(brew_view->view, brew_view_draw_callback);
    view_set_input_callback(brew_view->view, brew_view_input_callback);

    with_view_model(
        brew_view->view,
        BrewViewModel * model,
        {
            model->name_string = brew_view->name_string;
            model->phase = BrewPhaseIDLE;
            model->pour_number = 1;
        },
        false);

    return brew_view;
}

void brew_view_free(BrewView* brew_view) {
    furi_assert(brew_view != NULL);
    view_free(brew_view->view);
    furi_string_free(brew_view->name_string);
    free(brew_view);
}

View* brew_view_get_view(BrewView* brew_view) {
    furi_assert(brew_view != NULL);
    return brew_view->view;
}

void brew_view_update(BrewView* brew_view, const BrewSession* session) {
    furi_assert(brew_view != NULL);
    furi_assert(session != NULL);

    with_view_model(
        brew_view->view,
        BrewViewModel * model,
        {
            model->name_string = brew_view->name_string;
            model->phase = (uint8_t)session->phase;
            memcpy(model->name, session->preset.name, sizeof(model->name));
            model->name[TEA_NAME_MAX] = '\0';
            model->completed_pours = session->completed_pours;
            model->max_pours = session->preset.max_pours;
            model->over_limit = brew_session_over_limit(session);
            model->leaf_tenths_g = session->preset.leaf_tenths_g;
            model->pour_number = (uint16_t)(session->completed_pours + 1);
            model->pour_sec = session->next_pour_sec;
            model->remaining_sec =
                (session->phase == BrewPhaseRUNNING) ?
                    brew_session_remaining_sec(furi_get_tick(), session->deadline_tick) :
                    0;
        },
        true);
}
