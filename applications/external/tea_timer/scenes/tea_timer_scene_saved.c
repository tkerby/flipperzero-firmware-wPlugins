#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerSaved"

/** `OK` на экране результата: вернуться к выбору файла. */
static void
    tea_timer_scene_saved_ok_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    tea_timer_goto_file_select(app);
}

void tea_timer_scene_saved_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->saved;

    widget_reset(widget);
    widget_add_string_element(
        widget,
        64,
        18,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        (app->saved_kind == TeaSavedKindDeleted) ? "Deleted" : "Saved");
    widget_add_string_element(
        widget, 64, 34, AlignCenter, AlignCenter, FontSecondary, app->saved_file);
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "OK", tea_timer_scene_saved_ok_callback, app);

    FURI_LOG_I(
        TAG,
        "%s %s",
        (app->saved_kind == TeaSavedKindDeleted) ? "deleted" : "saved",
        app->saved_file);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewSaved);
}

bool tea_timer_scene_saved_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // BACK равно `OK`: не оставляем пользователя в тупике.
        tea_timer_goto_file_select(app);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_saved_on_exit(void* context) {
    UNUSED(context);
}
