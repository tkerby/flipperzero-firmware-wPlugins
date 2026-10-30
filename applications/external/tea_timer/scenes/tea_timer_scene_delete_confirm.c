#include <string.h>

#include "../tea_timer_i.h"
#include "../lib/tea_logic/tea_preset_save.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerDelete"

/** Максимальная длина строки виджета (мелкий шрифт, 128×64). */
#define TEA_DELETE_TEXT_MAX 26

/** Вернуться на карточку пресета, убрав из стека меню действий и подтверждение. */
static void tea_timer_scene_delete_confirm_back_to_card(TeaTimerApp* app) {
    if(!scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, TeaTimerScenePresetCard)) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

/**
 * Обрезать строку многоточием до ширины экрана.
 * `memmove` позволяет вызывать функцию с тем же буфером в `src` и `out`.
 */
static void tea_timer_scene_delete_confirm_fit(const char* src, char* out, size_t size) {
    if(out == NULL || size == 0) {
        return;
    }
    if(src == NULL) {
        out[0] = '\0';
        return;
    }

    size_t len = strlen(src);
    if(len <= size - 1) {
        memmove(out, src, len);
        out[len] = '\0';
        return;
    }

    // Оставляем место под `...` и терминатор.
    size_t keep = (size > 4) ? (size - 1 - 3) : 0;
    if(keep > 0) {
        memmove(out, src, keep);
    }
    memcpy(out + keep, "...", 3);
    out[keep + 3] = '\0';
}

/**
 * Удалить файл пресета. `Retry` из диалога повторяет попытку; `Cancel` и успешное
 * завершение возвращают управление вызывающему.
 */
static void tea_timer_scene_delete_confirm_delete(TeaTimerApp* app) {
    const char* path = app->session.preset.path;

    do {
        PresetDeleteResult result = tea_preset_delete(app->storage, path);

        if(result == PresetDeleteOK) {
            FURI_LOG_I(TAG, "deleted %s", path);
            // Имя файла копируется до сброса сессии: `path` указывает в её буфер.
            tea_timer_basename(path, app->saved_file, sizeof(app->saved_file));
            // Активная сессия удалённого пресета сбрасывается целиком.
            if(app->session.has_preset && strcmp(app->session.preset.path, path) == 0) {
                brew_session_clear(&app->session);
            }
            app->saved_kind = TeaSavedKindDeleted;
            scene_manager_next_scene(app->scene_manager, TeaTimerSceneSaved);
            return;
        }

        FURI_LOG_E(TAG, "delete %s failed: %d", path, (int)result);

        const char* header = "Cannot delete";
        const char* text = NULL;
        bool retry_possible = false;
        switch(result) {
        case PresetDeleteErrNotFound:
            text = "File not found";
            break;
        case PresetDeleteErrNoCard:
            header = "SD card not found";
            text = "Insert the card";
            break;
        case PresetDeleteErrRemove:
        default:
            text = "Check the SD card";
            retry_possible = true;
            break;
        }

        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
        dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
        if(retry_possible) {
            dialog_message_set_buttons(message, "Retry", NULL, "Cancel");
        } else {
            dialog_message_set_buttons(message, NULL, NULL, "OK");
        }
        DialogMessageButton button = dialog_message_show(app->dialogs, message);
        dialog_message_free(message);

        if(!retry_possible || button != DialogMessageButtonLeft) {
            // Отмена или фатальная ошибка: файл не изменён, возврат на карточку.
            tea_timer_scene_delete_confirm_back_to_card(app);
            return;
        }
    } while(true);
}

/** `Yes` (Right): удаление файла после явного подтверждения. */
static void tea_timer_scene_delete_confirm_yes_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    tea_timer_scene_delete_confirm_delete(app);
}

/** `No` (Center): отмена, файл не меняется. */
static void tea_timer_scene_delete_confirm_no_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;
    FURI_LOG_I(TAG, "delete cancelled");
    tea_timer_scene_delete_confirm_back_to_card(app);
}

void tea_timer_scene_delete_confirm_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->delete_confirm;
    char file[TEA_DELETE_TEXT_MAX + 1];
    char name[TEA_DELETE_TEXT_MAX + 1];

    // Имя файла без каталога, затем обрезка обеих строк по ширине экрана.
    tea_timer_basename(app->session.preset.path, file, sizeof(file));
    tea_timer_scene_delete_confirm_fit(app->session.preset.name, name, sizeof(name));
    tea_timer_scene_delete_confirm_fit(file, file, sizeof(file));

    widget_reset(widget);
    widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontSecondary, name);
    widget_add_string_element(widget, 64, 16, AlignCenter, AlignTop, FontSecondary, file);
    widget_add_string_element(
        widget, 64, 32, AlignCenter, AlignTop, FontPrimary, "Delete preset?");

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "No", tea_timer_scene_delete_confirm_no_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Yes", tea_timer_scene_delete_confirm_yes_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewDeleteConfirm);
}

bool tea_timer_scene_delete_confirm_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // BACK равен `No`: удаление только по явному `Yes`.
        tea_timer_scene_delete_confirm_back_to_card(app);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_delete_confirm_on_exit(void* context) {
    UNUSED(context);
}
