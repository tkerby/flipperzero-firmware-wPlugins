#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerName"

/** Подсказка допустимых символов названия (C-UI-12). */
#define TEA_NAME_ALLOWED "A-Z a-z 0-9 _"

/** Допустимый символ названия: латиница, цифры, пробел и `_`. */
static bool tea_timer_edit_name_char_allowed(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == ' ' || c == '_';
}

/** Валидатор поля названия: объясняет, какие символы допустимы. */
static bool tea_timer_edit_name_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    for(const char* p = text; *p != '\0'; p++) {
        if(!tea_timer_edit_name_char_allowed(*p)) {
            if(error != NULL) {
                furi_string_set_str(error, TEA_NAME_ALLOWED);
            }
            return false;
        }
    }
    return true;
}

/** OK на клавиатуре: название введено, переходим к граммовке. */
static void tea_timer_scene_edit_name_callback(void* context) {
    TeaTimerApp* app = context;
    FURI_LOG_I(TAG, "name '%s' editing=%d", app->draft.name, app->editing);
    app->edit_step = TeaEditStepLeaf;
    scene_manager_next_scene(app->scene_manager, TeaTimerSceneEditNumber);
}

void tea_timer_scene_edit_name_on_enter(void* context) {
    TeaTimerApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Tea name");
    // clear_default_text=false: правка открывается с текущим именем, создание — с пустым
    // (меню сбрасывает `draft` в умолчания перед входом).
    text_input_set_result_callback(
        text_input,
        tea_timer_scene_edit_name_callback,
        app,
        app->draft.name,
        sizeof(app->draft.name),
        false);
    text_input_set_minimum_length(text_input, 1);
    text_input_set_validator(text_input, tea_timer_edit_name_validator, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewEditName);
}

bool tea_timer_scene_edit_name_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Создание: назад в меню; правка: назад в меню действий. Файл не меняется.
        if(app->editing) {
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, TeaTimerSceneActions)) {
                scene_manager_previous_scene(app->scene_manager);
            }
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_edit_name_on_exit(void* context) {
    UNUSED(context);
}
