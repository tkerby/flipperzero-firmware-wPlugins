#include "../twentyq_app_i.h"

static void twentyq_question_answer_callback(Tq_Answer answer, void* context) {
    TwentyQApp* app = context;
    app->pending_answer = answer;
    view_dispatcher_send_custom_event(app->view_dispatcher, TwentyQEventAnswer);
}

static void twentyq_question_undo_callback(void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TwentyQEventUndo);
}

static void twentyq_question_quit_callback(void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TwentyQEventQuit);
}

void twentyq_scene_question_on_enter(void* context) {
    TwentyQApp* app = context;

    twentyq_question_view_set_callbacks(
        app->question_view,
        twentyq_question_answer_callback,
        twentyq_question_undo_callback,
        twentyq_question_quit_callback,
        app);
    twentyq_question_view_set_question(
        app->question_view,
        app->question_text,
        app->engine->asked_count,
        tq_engine_question_budget(app->engine),
        app->settings.default_answer_yes ? TqAnswerYes : TqAnswerUnknown);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewQuestion);
}

bool twentyq_scene_question_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case TwentyQEventAnswer:
        twentyq_notify(app, &sequence_blink_blue_10);
        app->job = TwentyQJobAnswer;
        scene_manager_next_scene(app->scene_manager, TwentyQSceneThinking);
        return true;
    case TwentyQEventUndo:
        if(app->engine->asked_count == 0) {
            /* nothing to undo -- treat it as leaving the game */
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, TwentyQSceneStart);
            return true;
        }
        app->job = TwentyQJobUndo;
        scene_manager_next_scene(app->scene_manager, TwentyQSceneThinking);
        return true;
    case TwentyQEventQuit:
        scene_manager_next_scene(app->scene_manager, TwentyQSceneQuitConfirm);
        return true;
    default:
        return false;
    }
}

void twentyq_scene_question_on_exit(void* context) {
    UNUSED(context);
}
