#include "../twentyq_app_i.h"

static void twentyq_scene_learn_confirm_callback(DialogExResult result, void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)result);
}

/** Stores this game's answers against the new word, so it is findable next time. */
static bool twentyq_learn_save(TwentyQApp* app) {
    Tq_Engine* eng = app->engine;
    uint16_t qids[TQ_LEARN_MAX_ANSWERS];
    uint8_t nibs[TQ_LEARN_MAX_ANSWERS];
    uint16_t count = 0;

    for(uint16_t i = 0; i < eng->asked_count && count < TQ_LEARN_MAX_ANSWERS; i++) {
        Tq_Answer a = (Tq_Answer)eng->answers[i];
        if(a == TqAnswerUnknown) continue; /* carries no information */
        qids[count] = eng->asked_order[i];
        nibs[count] = tq_answer_to_nibble(&app->dataset, a);
        count++;
    }

    if(!tq_learned_add(&app->learned, app->learn_name, app->learn_category, qids, nibs, count)) {
        return false;
    }
    return twentyq_learned_save_file(app->storage, &app->learned);
}

void twentyq_scene_learn_confirm_on_enter(void* context) {
    TwentyQApp* app = context;
    DialogEx* dialog = app->dialog;

    snprintf(app->message, sizeof(app->message), "Remember %s\nfor next time?", app->learn_name);

    dialog_ex_reset(dialog);
    dialog_ex_set_header(dialog, "Teach me", 64, 8, AlignCenter, AlignCenter);
    dialog_ex_set_text(dialog, app->message, 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog, "No");
    dialog_ex_set_right_button_text(dialog, "Save");
    dialog_ex_set_result_callback(dialog, twentyq_scene_learn_confirm_callback);
    dialog_ex_set_context(dialog, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewDialog);
}

bool twentyq_scene_learn_confirm_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DialogExResultRight) {
        if(twentyq_learn_save(app)) {
            twentyq_notify(app, &sequence_success);
        } else {
            twentyq_show_error(app, "Could not save.\nCheck the SD card.");
            return true;
        }
    }
    scene_manager_search_and_switch_to_another_scene(app->scene_manager, TwentyQSceneStart);
    return true;
}

void twentyq_scene_learn_confirm_on_exit(void* context) {
    TwentyQApp* app = context;
    dialog_ex_reset(app->dialog);
}
