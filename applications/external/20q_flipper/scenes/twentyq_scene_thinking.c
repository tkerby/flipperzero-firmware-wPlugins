#include "../twentyq_app_i.h"

/**
 * All the SD reads and float work happen here, on a worker thread, with the
 * loading spinner up (SPEC 6.6 / 8.3). A turn reads up to TQ_CANDIDATES
 * columns, so it must never run inside a draw callback.
 */

static void twentyq_decide_next(TwentyQApp* app) {
    Tq_Engine* eng = app->engine;

    if(tq_engine_should_guess(eng)) {
        Tq_Candidate top[1];
        if(tq_engine_top(eng, top, 1) == 0 || eng->rejected_count >= TQ_MAX_GUESSES) {
            app->outcome = TwentyQOutcomeOutOfIdeas;
            return;
        }
        app->guess_index = top[0].index;
        if(!tq_engine_entity_name(eng, top[0].index, app->guess_name, sizeof(app->guess_name))) {
            app->outcome = TwentyQOutcomeError;
            return;
        }
        app->outcome = TwentyQOutcomeGuess;
        FURI_LOG_D(TAG, "guess %s (b=%f)", app->guess_name, (double)top[0].belief);
        return;
    }

    uint16_t qid = 0;
    if(!tq_engine_select_question(eng, &qid)) {
        /* Nothing left to ask: guess with whatever we have. */
        app->outcome = TwentyQOutcomeOutOfIdeas;
        return;
    }
    if(!tq_dataset_question_text(
           &app->dataset, qid, app->question_text, sizeof(app->question_text))) {
        app->outcome = TwentyQOutcomeError;
        return;
    }
    app->question_id = qid;
    app->outcome = TwentyQOutcomeQuestion;
}

static int32_t twentyq_worker_thread(void* context) {
    TwentyQApp* app = context;
    uint32_t start = furi_get_tick();
    bool ok = true;

    switch(app->job) {
    case TwentyQJobStartGame:
        tq_engine_set_question_limit(app->engine, app->settings.question_limit);
        ok = tq_engine_reset(app->engine, app->category_hint);
        break;
    case TwentyQJobAnswer:
        ok = tq_engine_answer(app->engine, app->question_id, app->pending_answer);
        break;
    case TwentyQJobUndo: {
        uint16_t last = app->engine->asked_order[app->engine->asked_count - 1];
        ok = tq_engine_undo(app->engine);
        if(ok) {
            app->question_id = last;
            ok = tq_dataset_question_text(
                &app->dataset, last, app->question_text, sizeof(app->question_text));
            app->outcome = ok ? TwentyQOutcomeQuestion : TwentyQOutcomeError;
        }
        break;
    }
    case TwentyQJobRejectGuess:
        tq_engine_reject(app->engine, app->guess_index);
        break;
    }

    if(!ok) {
        app->outcome = TwentyQOutcomeError;
    } else if(app->job != TwentyQJobUndo) {
        twentyq_decide_next(app);
    }

    app->turn_ms = furi_get_tick() - start;
    /* SPEC 11.3: turn time, worker stack watermark, heap headroom.
     * Debug level: see it with `log debug` in the CLI. */
    FURI_LOG_D(
        TAG,
        "job %d took %lums, stack free %lu, heap free %u",
        (int)app->job,
        app->turn_ms,
        furi_thread_get_stack_space(furi_thread_get_current_id()),
        (unsigned)memmgr_get_free_heap());
    view_dispatcher_send_custom_event(app->view_dispatcher, TwentyQEventWorkerDone);
    return 0;
}

void twentyq_scene_thinking_on_enter(void* context) {
    TwentyQApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewLoading);

    app->worker = furi_thread_alloc_ex("TwentyQWorker", 2048, twentyq_worker_thread, app);
    furi_thread_start(app->worker);
}

bool twentyq_scene_thinking_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        return true; /* the worker owns the engine right now: ignore Back */
    }
    if(event.type != SceneManagerEventTypeCustom || event.event != TwentyQEventWorkerDone) {
        return false;
    }

    switch(app->outcome) {
    case TwentyQOutcomeQuestion:
        if(scene_manager_has_previous_scene(app->scene_manager, TwentyQSceneQuestion)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TwentyQSceneQuestion);
        } else {
            scene_manager_next_scene(app->scene_manager, TwentyQSceneQuestion);
        }
        return true;
    case TwentyQOutcomeGuess:
        scene_manager_next_scene(app->scene_manager, TwentyQSceneGuess);
        return true;
    case TwentyQOutcomeOutOfIdeas:
        app->game_won = false;
        scene_manager_next_scene(app->scene_manager, TwentyQSceneResult);
        return true;
    case TwentyQOutcomeError:
    default:
        twentyq_show_error(
            app,
            app->engine->io_error ? "SD card unreadable.\nGame stopped." :
                                    "Something went wrong.");
        return true;
    }
}

void twentyq_scene_thinking_on_exit(void* context) {
    TwentyQApp* app = context;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
}
