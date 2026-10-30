#include "../twentyq_app_i.h"

void twentyq_scene_about_on_enter(void* context) {
    TwentyQApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "\e#How it works\e#\n"
        "Think of anything real. I ask up to %u yes/no questions and try to name it.\n"
        "\n"
        "I hold a belief about every word I know and update it after each answer "
        "(Bayes' rule), then pick whichever question splits my remaining suspects "
        "most evenly.\n"
        "\n"
        "\e#Controls\e#\n"
        "Left/Right pick an answer, OK confirms.\n"
        "Up is a shortcut for Yes, Down for No.\n"
        "Back undoes your last answer.\n"
        "Hold Back to give up.\n"
        "\n"
        "\e#This build\e#\n"
        "%lu things, %lu questions\n"
        "Data: %s\n"
        "Everything runs offline on the Flipper. No network, no phone.\n",
        app->settings.question_limit,
        app->dataset.entity_count,
        app->dataset.question_count,
        app->dataset.dataset_id);

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewWidget);
}

bool twentyq_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void twentyq_scene_about_on_exit(void* context) {
    TwentyQApp* app = context;
    widget_reset(app->widget);
}
