#include "../twentyq_app_i.h"

typedef enum {
    ResultEventContinue,
    ResultEventTeach,
} ResultEvent;

static void twentyq_scene_result_callback(GuiButtonType type, InputType input, void* context) {
    TwentyQApp* app = context;
    if(input != InputTypeShort) return;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        (type == GuiButtonTypeRight) ? ResultEventTeach : ResultEventContinue);
}

static void twentyq_record_result(TwentyQApp* app) {
    TwentyQStats* s = &app->stats;
    if(s->games < 0xFFFF) s->games++;
    if(app->game_won) {
        if(s->wins < 0xFFFF) s->wins++;
        if(app->guess_number == 0) {
            s->questions_to_win += app->engine->asked_count;
            if(s->win_games < 0xFFFF) s->win_games++;
            if(s->best_game == 0 || app->engine->asked_count < s->best_game) {
                s->best_game = app->engine->asked_count;
            }
        }
    }
    twentyq_stats_save(app->storage, s);
}

void twentyq_scene_result_on_enter(void* context) {
    TwentyQApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    twentyq_record_result(app);

    char line[64];
    if(app->game_won) {
        widget_add_string_element(
            widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "I got it!");
        snprintf(
            line,
            sizeof(line),
            "%u question%s%s",
            app->engine->asked_count,
            app->engine->asked_count == 1 ? "" : "s",
            app->guess_number ? ", 2nd guess" : "");
        widget_add_string_element(widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, line);
        if(app->stats.games) {
            snprintf(
                line,
                sizeof(line),
                "Win rate %u%% over %u games",
                (unsigned)(100u * app->stats.wins / app->stats.games),
                app->stats.games);
            widget_add_string_element(
                widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, line);
        }
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Menu", twentyq_scene_result_callback, app);
        twentyq_notify(app, &sequence_success);
    } else {
        widget_add_string_element(
            widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "You win!");
        widget_add_string_element(
            widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, "I could not guess it.");
        widget_add_string_element(
            widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Teach me what it was?");
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Menu", twentyq_scene_result_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Teach", twentyq_scene_result_callback, app);
        twentyq_notify(app, &sequence_error);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewWidget);
}

bool twentyq_scene_result_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, TwentyQSceneStart);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ResultEventTeach) {
        if(app->learned.count >= TQ_LEARN_MAX_RECORDS) {
            twentyq_show_error(app, "Memory full.\nReset learned words\nin Settings.");
        } else {
            app->learn_name[0] = '\0';
            app->learn_category = app->category_hint < TQ_CATEGORY_COUNT ? app->category_hint : 3;
            scene_manager_next_scene(app->scene_manager, TwentyQSceneLearnName);
        }
        return true;
    }
    scene_manager_search_and_switch_to_another_scene(app->scene_manager, TwentyQSceneStart);
    return true;
}

void twentyq_scene_result_on_exit(void* context) {
    TwentyQApp* app = context;
    widget_reset(app->widget);
}
