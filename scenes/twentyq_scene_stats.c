#include "../twentyq_app_i.h"

void twentyq_scene_stats_on_enter(void* context) {
    TwentyQApp* app = context;
    Widget* widget = app->widget;
    const TwentyQStats* s = &app->stats;
    widget_reset(widget);

    widget_add_string_element(widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Stats");

    char line[64];
    snprintf(line, sizeof(line), "Games %u   Wins %u", s->games, s->wins);
    widget_add_string_element(widget, 64, 24, AlignCenter, AlignCenter, FontSecondary, line);

    snprintf(
        line, sizeof(line), "Win rate %u%%", s->games ? (unsigned)(100u * s->wins / s->games) : 0u);
    widget_add_string_element(widget, 64, 34, AlignCenter, AlignCenter, FontSecondary, line);

    if(s->win_games) {
        snprintf(
            line,
            sizeof(line),
            "Average %lu questions",
            (unsigned long)(s->questions_to_win / s->win_games));
    } else {
        snprintf(line, sizeof(line), "No wins yet");
    }
    widget_add_string_element(widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, line);

    if(s->best_game) {
        snprintf(line, sizeof(line), "Best %u   Learned %u", s->best_game, app->learned.count);
    } else {
        snprintf(line, sizeof(line), "Learned words %u", app->learned.count);
    }
    widget_add_string_element(widget, 64, 54, AlignCenter, AlignCenter, FontSecondary, line);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewWidget);
}

bool twentyq_scene_stats_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void twentyq_scene_stats_on_exit(void* context) {
    TwentyQApp* app = context;
    widget_reset(app->widget);
}
