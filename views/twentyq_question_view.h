/**
 * The question screen (SPEC 8.2). DialogEx only offers three buttons and we
 * need five answers plus a progress indicator, so this is a custom View.
 */
#pragma once

#include <gui/view.h>

#include "../helpers/twentyq_engine.h"

typedef struct TwentyQQuestionView TwentyQQuestionView;

typedef void (*TwentyQQuestionAnswerCallback)(Tq_Answer answer, void* context);
typedef void (*TwentyQQuestionEventCallback)(void* context);

TwentyQQuestionView* twentyq_question_view_alloc(void);
void twentyq_question_view_free(TwentyQQuestionView* view);
View* twentyq_question_view_get_view(TwentyQQuestionView* view);

void twentyq_question_view_set_callbacks(
    TwentyQQuestionView* view,
    TwentyQQuestionAnswerCallback on_answer,
    TwentyQQuestionEventCallback on_undo,
    TwentyQQuestionEventCallback on_quit,
    void* context);

void twentyq_question_view_set_question(
    TwentyQQuestionView* view,
    const char* text,
    uint16_t asked,
    uint16_t total,
    Tq_Answer default_answer);
