#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#include <string.h>
#include <strings.h>

#define TAG "TeaTimerNoPresets"

/** Короткое расширение файла пресета. */
#define TEA_TIMER_PRESET_EXTENSION ".tea"

/**
 * Проверить, есть ли в корне каталога приложения хотя бы один файл `.tea`
 * (storage-layout.md:64). Подкаталоги, dot-файлы и прочие расширения пропускаются.
 * `storage_dir_close()` вызывается на всех ветках, включая неудачное открытие.
 */
bool tea_timer_has_presets(Storage* storage) {
    furi_assert(storage != NULL);

    bool found = false;
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, TEA_TIMER_APP_DATA_PATH)) {
        FileInfo info;
        char name[64];

        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) {
                continue;
            }
            if(name[0] == '.') {
                continue;
            }
            const char* dot = strrchr(name, '.');
            if(dot != NULL && strcasecmp(dot, TEA_TIMER_PRESET_EXTENSION) == 0) {
                found = true;
                break;
            }
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

/** Кнопка `Reload`: повторная проверка каталога с пресетами. */
static void tea_timer_scene_no_presets_reload_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TeaTimerEventReloadPressed);
}

void tea_timer_scene_no_presets_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->no_presets;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "No presets found");
    widget_add_string_element(
        widget, 64, 22, AlignCenter, AlignTop, FontSecondary, "Add .tea files to");
    widget_add_string_element(
        widget, 64, 32, AlignCenter, AlignTop, FontSecondary, "apps_data/tea_timer");
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Reload", tea_timer_scene_no_presets_reload_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewNoPresets);
}

bool tea_timer_scene_no_presets_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TeaTimerEventReloadPressed) {
            // Повторная маршрутизация: Error / FileSelect / снова NoPresets.
            if(storage_sd_status(app->storage) != FSE_OK) {
                scene_manager_next_scene(app->scene_manager, TeaTimerSceneError);
            } else if(tea_timer_has_presets(app->storage)) {
                FURI_LOG_I(TAG, "presets found, open file select");
                scene_manager_next_scene(app->scene_manager, TeaTimerSceneFileSelect);
            } else {
                FURI_LOG_I(TAG, "still no presets");
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

void tea_timer_scene_no_presets_on_exit(void* context) {
    UNUSED(context);
}
