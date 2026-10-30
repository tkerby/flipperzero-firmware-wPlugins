#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerError"

/** Кнопка `Retry`: повторная проверка носителя. */
static void
    tea_timer_scene_error_retry_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TeaTimerEventReloadPressed);
}

void tea_timer_scene_error_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->error;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "SD card not found");
    widget_add_string_element(
        widget, 64, 26, AlignCenter, AlignTop, FontSecondary, "Insert the card");
    widget_add_string_element(
        widget, 64, 36, AlignCenter, AlignTop, FontSecondary, "and press OK");
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Retry", tea_timer_scene_error_retry_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewError);
}

bool tea_timer_scene_error_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TeaTimerEventReloadPressed) {
            FS_Error status = storage_sd_status(app->storage);
            FURI_LOG_I(TAG, "sd status %d", (int)status);
            if(status == FSE_OK) {
                // Носитель снова доступен: заново маршрутизируем по состоянию каталога.
                tea_timer_show_presets(app);
            } else {
                notification_message(app->notification, &sequence_error);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, TeaTimerSceneMenu)) {
            view_dispatcher_stop(app->view_dispatcher);
        }
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_error_on_exit(void* context) {
    UNUSED(context);
}
