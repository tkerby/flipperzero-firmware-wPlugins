#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/loading.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "helpers/twentyq_engine.h"
#include "helpers/twentyq_storage.h"
#include "scenes/twentyq_scene.h"
#include "views/twentyq_question_view.h"

#define TAG "TwentyQ"

typedef enum {
    TwentyQViewSubmenu,
    TwentyQViewDialog,
    TwentyQViewPopup,
    TwentyQViewTextInput,
    TwentyQViewVarItemList,
    TwentyQViewWidget,
    TwentyQViewLoading,
    TwentyQViewQuestion,
} TwentyQView;

typedef enum {
    TwentyQEventAnswer, /* the player chose an answer */
    TwentyQEventUndo, /* short Back on the question screen */
    TwentyQEventQuit, /* long Back */
    TwentyQEventWorkerDone, /* the engine finished a turn */
} TwentyQCustomEvent;

/** What the worker thread should do next. */
typedef enum {
    TwentyQJobStartGame,
    TwentyQJobAnswer,
    TwentyQJobUndo,
    TwentyQJobRejectGuess,
} TwentyQJob;

/** What it produced. */
typedef enum {
    TwentyQOutcomeQuestion,
    TwentyQOutcomeGuess,
    TwentyQOutcomeOutOfIdeas,
    TwentyQOutcomeError,
} TwentyQOutcome;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;

    Submenu* submenu;
    DialogEx* dialog;
    Popup* popup;
    TextInput* text_input;
    VariableItemList* var_item_list;
    Widget* widget;
    Loading* loading;
    TwentyQQuestionView* question_view;

    /* --- game state --- */
    TwentyQStorage store; /* dataset file handle + reader bridge */
    Tq_Dataset dataset;
    bool dataset_ready;
    Tq_Learned learned;
    Tq_Engine* engine;

    TwentyQSettings settings;
    TwentyQStats stats;

    FuriThread* worker;
    TwentyQJob job;
    TwentyQOutcome outcome;
    Tq_Answer pending_answer;
    uint32_t turn_ms; /* last turn duration, logged for the SPEC 6.6 budget */

    uint8_t category_hint; /* 0..3, or TQ_NO_CATEGORY */
    uint16_t question_id;
    char question_text[TQ_MAX_QTEXT + 1];
    uint32_t guess_index;
    char guess_name[TQ_MAX_NAME + 1];
    uint8_t guess_number;

    char learn_name[TQ_MAX_NAME + 1];
    uint8_t learn_category;
    char message[96];
    char message_header[24];
    bool game_won;
} TwentyQApp;

void twentyq_show_error(TwentyQApp* app, const char* text);
void twentyq_show_message(TwentyQApp* app, const char* header, const char* text);
void twentyq_notify(TwentyQApp* app, const NotificationSequence* seq);
