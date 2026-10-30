#include <stdio.h>
#include <string.h>

#include "../tea_timer_i.h"
#include "../lib/tea_logic/tea_preset_save.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerSummary"

/** Шаг формы, к которому относится проблемное поле черновика. */
static TeaEditStep tea_timer_scene_edit_summary_step_for_field(const char* field) {
    if(field == NULL || field[0] == '\0') {
        return TeaEditStepName;
    }
    if(strcmp(field, "name") == 0) {
        return TeaEditStepName;
    }
    if(strcmp(field, "leaf_g_per_100ml") == 0) {
        return TeaEditStepLeaf;
    }
    if(strcmp(field, "water_temp_min_c") == 0) {
        return TeaEditStepWaterMin;
    }
    if(strcmp(field, "water_temp_max_c") == 0) {
        return TeaEditStepWaterMax;
    }
    if(strcmp(field, "first_pour_sec") == 0) {
        return TeaEditStepFirst;
    }
    if(strcmp(field, "increment_sec") == 0) {
        return TeaEditStepIncrement;
    }
    return TeaEditStepMaxPours;
}

/** Текст диалога `Bad value`: поле и допустимый диапазон (C-UI-17). */
static const char* tea_timer_scene_edit_summary_hint(TeaEditStep step) {
    switch(step) {
    case TeaEditStepLeaf:
        return "Leaf: 0.1-50.0 g";
    case TeaEditStepWaterMin:
    case TeaEditStepWaterMax:
        return "Water: 85-90 C";
    case TeaEditStepFirst:
        return "First: 1-3600 sec";
    case TeaEditStepIncrement:
        return "Step: 0-600 sec";
    case TeaEditStepMaxPours:
        return "Max pours: 1-99";
    case TeaEditStepName:
    default:
        return "Name: 1-24 chars";
    }
}

/**
 * Вернуть пользователя на проблемный шаг формы (значение остаётся в `draft`).
 * Сводка и все шаги выше проблемного из стека убираются.
 */
static void tea_timer_scene_edit_summary_goto_step(TeaTimerApp* app, TeaEditStep step) {
    app->edit_step = (uint8_t)step;

    if(step == TeaEditStepName) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, TeaTimerSceneEditName)) {
            scene_manager_next_scene(app->scene_manager, TeaTimerSceneEditName);
        }
        return;
    }

    // Предыдущая сцена сводки — `EditNumber`; она настраивается по `edit_step`.
    scene_manager_previous_scene(app->scene_manager);
}

/**
 * Диалог ошибки записи (C-UI-17).
 * @return true — пользователь выбрал `Retry`
 */
static bool tea_timer_scene_edit_summary_save_error(TeaTimerApp* app, PresetSaveResult result) {
    const char* header = "Cannot save";
    const char* text = "Check the SD card";
    const char* left = "Retry";
    const char* center = NULL;
    const char* right = "Cancel";

    switch(result) {
    case PresetSaveErrNoDir:
    case PresetSaveErrOpen:
    case PresetSaveErrWrite:
    case PresetSaveErrVerify:
        // Повторная попытка записи возможна.
        break;
    case PresetSaveErrReplace:
        text = "File is kept";
        left = NULL;
        center = "OK";
        right = NULL;
        break;
    case PresetSaveErrNoName:
        text = "Too many copies";
        left = NULL;
        center = "OK";
        right = NULL;
        break;
    case PresetSaveErrDraft:
    case PresetSaveOK:
    default:
        text = "Bad value";
        left = NULL;
        center = "OK";
        right = NULL;
        break;
    }

    FURI_LOG_E(TAG, "save failed: %d", (int)result);

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, left, center, right);
    DialogMessageButton button = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);

    return button == DialogMessageButtonLeft;
}

/**
 * Правка активного пресета: обновить значения в сессии, не сбрасывая счётчик проливов.
 * Для нового файла и чужого пути сессия не трогается.
 */
static void tea_timer_scene_edit_summary_sync_session(TeaTimerApp* app, const char* path) {
    if(!app->editing || !app->session.has_preset || path == NULL) {
        return;
    }
    if(strcmp(app->session.preset.path, path) != 0) {
        return;
    }

    app->session.preset.leaf_tenths_g = app->draft.leaf_tenths_g;
    app->session.preset.water_temp_min_c = app->draft.water_temp_min_c;
    app->session.preset.water_temp_max_c = app->draft.water_temp_max_c;
    app->session.preset.first_pour_sec = app->draft.first_pour_sec;
    app->session.preset.increment_sec = app->draft.increment_sec;
    app->session.preset.max_pours = app->draft.max_pours;
    snprintf(app->session.preset.name, sizeof(app->session.preset.name), "%s", app->draft.name);
}

/** `Save` на сводке: проверка черновика, запись файла, экран `Saved`. */
static void tea_timer_scene_edit_summary_save_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    UNUSED(result);
    if(type != InputTypeShort) {
        return;
    }

    TeaTimerApp* app = context;

    char field[24];
    TeaPresetResult check = tea_preset_check_draft_ex(&app->draft, field, sizeof(field));
    if(check != TeaPresetResultOK) {
        TeaEditStep step = tea_timer_scene_edit_summary_step_for_field(field);
        FURI_LOG_E(TAG, "bad draft: %d field=%s step=%d", (int)check, field, (int)step);

        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Bad value", 64, 2, AlignCenter, AlignTop);
        dialog_message_set_text(
            message, tea_timer_scene_edit_summary_hint(step), 64, 32, AlignCenter, AlignCenter);
        dialog_message_set_buttons(message, NULL, NULL, "Retry");
        dialog_message_show(app->dialogs, message);
        dialog_message_free(message);

        // Возврат на проблемный шаг: введённые значения остаются в `draft`.
        tea_timer_scene_edit_summary_goto_step(app, step);
        return;
    }

    do {
        char path[TEA_PATH_MAX];
        PresetSaveResult saved =
            tea_preset_save(app->storage, &app->draft, app->editing, path, sizeof(path));
        if(saved == PresetSaveOK) {
            FURI_LOG_I(TAG, "saved %s (editing=%d)", path, app->editing);
            tea_timer_scene_edit_summary_sync_session(app, path);
            tea_timer_basename(path, app->saved_file, sizeof(app->saved_file));
            app->saved_kind = TeaSavedKindSaved;
            scene_manager_next_scene(app->scene_manager, TeaTimerSceneSaved);
            return;
        }

        // `Cancel`: остаёмся на сводке, черновик не теряется.
        if(!tea_timer_scene_edit_summary_save_error(app, saved)) {
            return;
        }
    } while(true);
}

void tea_timer_scene_edit_summary_on_enter(void* context) {
    TeaTimerApp* app = context;
    Widget* widget = app->edit_summary;
    char buffer[48];

    // Кнопка `Save` занимает y=48…63, поэтому строки формы заканчиваются не ниже y=44.
    widget_reset(widget);
    widget_add_string_element(
        widget,
        64,
        0,
        AlignCenter,
        AlignTop,
        FontPrimary,
        app->editing ? "Edit preset" : "New preset");

    widget_add_string_element(
        widget, 64, 10, AlignCenter, AlignTop, FontSecondary, app->draft.name);

    // FR-018: граммовка и диапазон температуры в одной строке над кнопкой `Save`.
    snprintf(
        buffer,
        sizeof(buffer),
        "Leaf %u.%u  Water %u-%u C",
        (unsigned)(app->draft.leaf_tenths_g / 10U),
        (unsigned)(app->draft.leaf_tenths_g % 10U),
        (unsigned)app->draft.water_temp_min_c,
        (unsigned)app->draft.water_temp_max_c);
    widget_add_string_element(widget, 64, 19, AlignCenter, AlignTop, FontSecondary, buffer);

    char first[16];
    char step[16];
    brew_math_format_time(app->draft.first_pour_sec, first, sizeof(first));
    brew_math_format_time(app->draft.increment_sec, step, sizeof(step));
    snprintf(buffer, sizeof(buffer), "First %s  Step %s", first, step);
    widget_add_string_element(widget, 64, 28, AlignCenter, AlignTop, FontSecondary, buffer);

    snprintf(buffer, sizeof(buffer), "Max %u pours", (unsigned)app->draft.max_pours);
    widget_add_string_element(widget, 64, 37, AlignCenter, AlignTop, FontSecondary, buffer);

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Save", tea_timer_scene_edit_summary_save_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewEditSummary);
}

bool tea_timer_scene_edit_summary_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Назад к последнему шагу формы; файл не пишется.
        app->edit_step = TeaEditStepMaxPours;
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_edit_summary_on_exit(void* context) {
    UNUSED(context);
}
