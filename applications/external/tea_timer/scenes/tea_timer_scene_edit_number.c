#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerNumber"

/** Обработчик OK на числовом шаге (объявлен до настройки number_input). */
static void tea_timer_scene_edit_number_callback(void* context, int32_t number);

/** Настроить number_input под текущий шаг формы (C-UI-13). */
static void tea_timer_scene_edit_number_configure(TeaTimerApp* app) {
    NumberInput* number_input = app->number_input;
    const char* header = "";
    int32_t value = 0;
    int32_t min_value = 0;
    int32_t max_value = 0;

    switch(app->edit_step) {
    case TeaEditStepLeaf:
        header = "Leaf, 0.1 g/100ml";
        value = app->draft.leaf_tenths_g;
        min_value = 1;
        max_value = 500;
        break;
    case TeaEditStepWaterMin:
        header = "Water min, C";
        value = app->draft.water_temp_min_c;
        min_value = TEA_WATER_TEMP_MIN;
        max_value = TEA_WATER_TEMP_MAX;
        break;
    case TeaEditStepWaterMax:
        // Нижняя граница = введённый минимум: инверсия диапазона невозможна (FR-017).
        header = "Water max, C";
        value = app->draft.water_temp_max_c;
        min_value = app->draft.water_temp_min_c;
        max_value = TEA_WATER_TEMP_MAX;
        break;
    case TeaEditStepFirst:
        header = "First pour, sec";
        value = app->draft.first_pour_sec;
        min_value = 1;
        max_value = 3600;
        break;
    case TeaEditStepIncrement:
        header = "Step, sec";
        value = app->draft.increment_sec;
        min_value = 0;
        max_value = 600;
        break;
    case TeaEditStepMaxPours:
    default:
        header = "Max pours";
        value = app->draft.max_pours;
        min_value = 1;
        max_value = 99;
        break;
    }

    number_input_set_header_text(number_input, header);
    number_input_set_result_callback(
        number_input, tea_timer_scene_edit_number_callback, app, value, min_value, max_value);
}

/** OK: сохранить значение шага и перейти к следующему. */
static void tea_timer_scene_edit_number_callback(void* context, int32_t number) {
    TeaTimerApp* app = context;

    switch(app->edit_step) {
    case TeaEditStepLeaf:
        app->draft.leaf_tenths_g = (uint16_t)number;
        app->edit_step = TeaEditStepWaterMin;
        break;
    case TeaEditStepWaterMin:
        app->draft.water_temp_min_c = (uint8_t)number;
        app->edit_step = TeaEditStepWaterMax;
        break;
    case TeaEditStepWaterMax:
        app->draft.water_temp_max_c = (uint8_t)number;
        app->edit_step = TeaEditStepFirst;
        break;
    case TeaEditStepFirst:
        app->draft.first_pour_sec = (uint16_t)number;
        app->edit_step = TeaEditStepIncrement;
        break;
    case TeaEditStepIncrement:
        app->draft.increment_sec = (uint16_t)number;
        app->edit_step = TeaEditStepMaxPours;
        break;
    case TeaEditStepMaxPours:
    default:
        app->draft.max_pours = (uint8_t)number;
        FURI_LOG_I(TAG, "form complete, summary");
        app->edit_step = TeaEditStepSummary;
        scene_manager_next_scene(app->scene_manager, TeaTimerSceneEditSummary);
        return;
    }

    // Следующий шаг — тот же экран с новым заголовком и границами.
    tea_timer_scene_edit_number_configure(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewEditNumber);
}

void tea_timer_scene_edit_number_on_enter(void* context) {
    TeaTimerApp* app = context;
    tea_timer_scene_edit_number_configure(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewEditNumber);
}

bool tea_timer_scene_edit_number_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // BACK: предыдущий шаг; с граммовки — назад к вводу названия.
        if(app->edit_step == TeaEditStepLeaf || app->edit_step == TeaEditStepName) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            app->edit_step--;
            tea_timer_scene_edit_number_configure(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewEditNumber);
        }
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_edit_number_on_exit(void* context) {
    UNUSED(context);
}
