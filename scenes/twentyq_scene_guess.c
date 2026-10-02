#include "../twentyq_app_i.h"

typedef enum {
    GuessResultNo,
    GuessResultYes,
} GuessResult;

static void twentyq_scene_guess_callback(DialogExResult result, void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, (result == DialogExResultRight) ? GuessResultYes : GuessResultNo);
}

void twentyq_scene_guess_on_enter(void* context) {
    TwentyQApp* app = context;
    DialogEx* dialog = app->dialog;

    /* Upper-case reads better as a headline; the data is stored lower-case. */
    char shout[TQ_MAX_NAME + 1];
    size_t i = 0;
    for(; app->guess_name[i] && i < TQ_MAX_NAME; i++) {
        char c = app->guess_name[i];
        shout[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    shout[i] = '\0';

    dialog_ex_reset(dialog);
    dialog_ex_set_header(
        dialog,
        app->guess_number == 0 ? "I'm thinking of..." : "Then maybe...",
        64,
        8,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_text(dialog, shout, 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog, "No");
    dialog_ex_set_right_button_text(dialog, "Yes!");
    dialog_ex_set_result_callback(dialog, twentyq_scene_guess_callback);
    dialog_ex_set_context(dialog, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewDialog);
}

bool twentyq_scene_guess_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == GuessResultYes) {
        app->game_won = true;
        scene_manager_next_scene(app->scene_manager, TwentyQSceneResult);
        return true;
    }

    /* Wrong: zero that entity, take five more questions, try again (SPEC 6.5). */
    app->guess_number++;
    if(app->guess_number >= TQ_MAX_GUESSES) {
        app->game_won = false;
        scene_manager_next_scene(app->scene_manager, TwentyQSceneResult);
        return true;
    }
    app->job = TwentyQJobRejectGuess;
    scene_manager_next_scene(app->scene_manager, TwentyQSceneThinking);
    return true;
}

void twentyq_scene_guess_on_exit(void* context) {
    TwentyQApp* app = context;
    dialog_ex_reset(app->dialog);
}
