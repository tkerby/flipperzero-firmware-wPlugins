#include "../fire_string.h"

void fire_string_scene_on_enter_about(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_about");
    furi_check(context);

    FireString* app = context;

    widget_reset(app->widget);

    FuriString* tmp_str = furi_string_alloc();
    furi_string_printf(tmp_str, "\e#%s\n", "Information");

    furi_string_cat_printf(tmp_str, "Version: %s\n", FAP_VERSION);
    furi_string_cat_printf(tmp_str, "\e#Developed by:\n%s\n", "Ryan Aboueljoud");
    furi_string_cat_printf(
        tmp_str, "\e#Github:\n%s\n\n", "https://github.com/\nRyanAboueljoud/Fire-String");

    furi_string_cat_printf(tmp_str, "\e#%s\n", "Description");
    furi_string_cat_printf(
        tmp_str, "Generate a truly random\nstring using IR noise as\nentropy.\n\n");
    furi_string_cat_printf(
        tmp_str,
        "Ignite a lighter 6+ inches\naway from the Flipper's IR\nsensor. The Fire String\ngenerator will capture IR\nbursts and use them to\ngenerate truly random strings\nof characters.\n");

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);
    widget_add_text_box_element(
        app->widget,
        0,
        2,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!             Fire String            \e!\n",
        false);
    widget_add_text_scroll_element(app->widget, 0, 16, 128, 50, furi_string_get_cstr(tmp_str));

    furi_string_free(tmp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);
}

bool fire_string_scene_on_event_about(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_about");
    furi_check(context);

    FireString* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        break;
    case SceneManagerEventTypeBack:
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_MainMenu);
        consumed = true;
        break;
    case SceneManagerEventTypeTick:
        break;
    }

    return consumed;
}

void fire_string_scene_on_exit_about(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_about");
    furi_check(context);

    FireString* app = context;

    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 0, 0, &I_Flame_128x64);
    furi_delay_us(250000);
    widget_reset(app->widget);
}
