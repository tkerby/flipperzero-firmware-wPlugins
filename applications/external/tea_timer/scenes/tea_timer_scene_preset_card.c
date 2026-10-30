#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerCard"

/** Кнопка `Start`: сразу запускаем пролив на экране заваривания. */
static void tea_timer_scene_preset_card_start_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    FURI_LOG_I(TAG, "start pour %u", (unsigned)(app->session.completed_pours + 1));
    scene_manager_next_scene(app->scene_manager, TeaTimerSceneBrew);
    view_dispatcher_send_custom_event(app->view_dispatcher, TeaTimerEventBrewStart);
}

/** Кнопка `>`: меню действий над пресетом (`Edit`/`Delete`, C-UI-3 дополнение 002). */
static void tea_timer_scene_preset_card_actions_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    FURI_LOG_I(TAG, "actions for '%s'", app->session.preset.name);
    scene_manager_next_scene(app->scene_manager, TeaTimerSceneActions);
}

void tea_timer_scene_preset_card_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->preset_card;

    widget_reset(widget);

    // Раскладка 003 (contracts/ui-screens.md §1.1): пять строк шагом 10 px над кнопкой.
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%s", app->session.preset.name);
    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "Leaf %u.%u g/100ml",
        (unsigned)(app->session.preset.leaf_tenths_g / 10),
        (unsigned)(app->session.preset.leaf_tenths_g % 10));
    widget_add_string_element(widget, 64, 10, AlignCenter, AlignTop, FontSecondary, buffer);

    // FR-010: рекомендуемый диапазон температуры воды виден всегда.
    snprintf(
        buffer,
        sizeof(buffer),
        "Water %u-%u C",
        (unsigned)app->session.preset.water_temp_min_c,
        (unsigned)app->session.preset.water_temp_max_c);
    widget_add_string_element(widget, 64, 20, AlignCenter, AlignTop, FontSecondary, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "Pours %u / %u",
        (unsigned)app->session.completed_pours,
        (unsigned)app->session.preset.max_pours);
    widget_add_string_element(widget, 64, 30, AlignCenter, AlignTop, FontSecondary, buffer);

    char time[16];
    brew_math_format_time(app->session.next_pour_sec, time, sizeof(time));
    snprintf(
        buffer, sizeof(buffer), "Pour %u  %s", (unsigned)(app->session.completed_pours + 1), time);
    widget_add_string_element(widget, 64, 40, AlignCenter, AlignTop, FontSecondary, buffer);

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Start", tea_timer_scene_preset_card_start_callback, app);

    // Действия доступны только при загруженном пресете (C-UI-3).
    if(app->session.has_preset) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, ">", tea_timer_scene_preset_card_actions_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewPresetCard);
}

bool tea_timer_scene_preset_card_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_preset_card_on_exit(void* context) {
    UNUSED(context);
}
