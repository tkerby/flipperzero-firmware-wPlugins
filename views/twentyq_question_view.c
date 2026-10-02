#include "twentyq_question_view.h"

#include <furi.h>
#include <gui/elements.h>

#define TQ_ANSWER_SLOTS 5

struct TwentyQQuestionView {
    View* view;
    TwentyQQuestionAnswerCallback on_answer;
    TwentyQQuestionEventCallback on_undo;
    TwentyQQuestionEventCallback on_quit;
    void* context;
};

typedef struct {
    char text[TQ_MAX_QTEXT + 1];
    uint8_t selected;
    uint16_t asked;
    uint16_t total;
} TwentyQQuestionModel;

/* Short labels: the full names do not fit five-across on 128 px. */
static const char* const kLabels[TQ_ANSWER_SLOTS] = {"No", "no?", "?", "yes?", "Yes"};

static void twentyq_question_view_draw(Canvas* canvas, void* model) {
    TwentyQQuestionModel* m = model;
    canvas_clear(canvas);

    /* header: counter left, progress bar right */
    canvas_set_font(canvas, FontSecondary);
    char counter[16];
    snprintf(counter, sizeof(counter), "Q %u/%u", m->asked + 1u, m->total);
    canvas_draw_str(canvas, 2, 8, counter);

    float progress = m->total ? (float)m->asked / (float)m->total : 0.0f;
    if(progress > 1.0f) progress = 1.0f;
    elements_progress_bar(canvas, 46, 1, 80, progress);

    /* question, centred and wrapped */
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 28, AlignCenter, AlignCenter, m->text);

    /* answer selector */
    canvas_set_font(canvas, FontSecondary);
    const uint8_t y = 52;
    uint8_t x = 2;
    for(uint8_t i = 0; i < TQ_ANSWER_SLOTS; i++) {
        uint8_t w = (uint8_t)(canvas_string_width(canvas, kLabels[i]) + 6);
        if(i == m->selected) {
            canvas_draw_rbox(canvas, x, y, w, 12, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x + 3, y + 9, kLabels[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, x, y, w, 12, 2);
            canvas_draw_str(canvas, x + 3, y + 9, kLabels[i]);
        }
        x = (uint8_t)(x + w + 2);
    }
}

static bool twentyq_question_view_input(InputEvent* event, void* context) {
    TwentyQQuestionView* view = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyLeft:
            with_view_model(
                view->view,
                TwentyQQuestionModel * model,
                {
                    if(model->selected > 0) model->selected--;
                },
                true);
            consumed = true;
            break;
        case InputKeyRight:
            with_view_model(
                view->view,
                TwentyQQuestionModel * model,
                {
                    if(model->selected + 1 < TQ_ANSWER_SLOTS) model->selected++;
                },
                true);
            consumed = true;
            break;
        case InputKeyUp: /* power-user shortcut: straight to Yes */
            if(view->on_answer) view->on_answer(TqAnswerYes, view->context);
            consumed = true;
            break;
        case InputKeyDown: /* ... and to No */
            if(view->on_answer) view->on_answer(TqAnswerNo, view->context);
            consumed = true;
            break;
        case InputKeyOk: {
            uint8_t selected = 0;
            with_view_model(
                view->view, TwentyQQuestionModel * model, { selected = model->selected; }, false);
            if(view->on_answer) view->on_answer((Tq_Answer)selected, view->context);
            consumed = true;
            break;
        }
        case InputKeyBack:
            if(view->on_undo) view->on_undo(view->context);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyBack) {
        if(view->on_quit) view->on_quit(view->context);
        consumed = true;
    }
    return consumed;
}

TwentyQQuestionView* twentyq_question_view_alloc(void) {
    TwentyQQuestionView* view = malloc(sizeof(TwentyQQuestionView));
    memset(view, 0, sizeof(*view));
    view->view = view_alloc();
    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(TwentyQQuestionModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, twentyq_question_view_draw);
    view_set_input_callback(view->view, twentyq_question_view_input);
    return view;
}

void twentyq_question_view_free(TwentyQQuestionView* view) {
    furi_assert(view);
    view_free(view->view);
    free(view);
}

View* twentyq_question_view_get_view(TwentyQQuestionView* view) {
    furi_assert(view);
    return view->view;
}

void twentyq_question_view_set_callbacks(
    TwentyQQuestionView* view,
    TwentyQQuestionAnswerCallback on_answer,
    TwentyQQuestionEventCallback on_undo,
    TwentyQQuestionEventCallback on_quit,
    void* context) {
    furi_assert(view);
    view->on_answer = on_answer;
    view->on_undo = on_undo;
    view->on_quit = on_quit;
    view->context = context;
}

void twentyq_question_view_set_question(
    TwentyQQuestionView* view,
    const char* text,
    uint16_t asked,
    uint16_t total,
    Tq_Answer default_answer) {
    furi_assert(view);
    with_view_model(
        view->view,
        TwentyQQuestionModel * model,
        {
            strncpy(model->text, text, TQ_MAX_QTEXT);
            model->text[TQ_MAX_QTEXT] = '\0';
            model->asked = asked;
            model->total = total;
            model->selected = (uint8_t)default_answer;
        },
        true);
}
