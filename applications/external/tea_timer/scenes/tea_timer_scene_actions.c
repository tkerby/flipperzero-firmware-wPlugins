#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerActions"

/** Пункты меню действий (C-UI-11). */
#define TEA_TIMER_ACTION_EDIT   0
#define TEA_TIMER_ACTION_DELETE 1

/** Выбор пункта меню действий. */
static void tea_timer_scene_actions_callback(void* context, uint32_t index) {
    TeaTimerApp* app = context;

    if(index == TEA_TIMER_ACTION_EDIT) {
        // Правка: форма открывается с текущими значениями, запись — в тот же файл.
        app->draft = app->session.preset;
        app->editing = true;
        app->edit_step = TeaEditStepName;
        FURI_LOG_I(TAG, "edit '%s'", app->draft.name);
        scene_manager_next_scene(app->scene_manager, TeaTimerSceneEditName);
    } else if(index == TEA_TIMER_ACTION_DELETE) {
        FURI_LOG_I(TAG, "delete '%s'", app->session.preset.name);
        scene_manager_next_scene(app->scene_manager, TeaTimerSceneDeleteConfirm);
    }
}

void tea_timer_scene_actions_on_enter(void* context) {
    TeaTimerApp* app = context;
    Submenu* submenu = app->actions_menu;

    submenu_reset(submenu);
    submenu_set_header(submenu, app->session.preset.name);
    submenu_add_item(
        submenu, "Edit", TEA_TIMER_ACTION_EDIT, tea_timer_scene_actions_callback, app);
    submenu_add_item(
        submenu, "Delete", TEA_TIMER_ACTION_DELETE, tea_timer_scene_actions_callback, app);
    submenu_set_selected_item(submenu, TEA_TIMER_ACTION_EDIT);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewActions);
}

bool tea_timer_scene_actions_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Назад на карточку; файл не меняется.
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_actions_on_exit(void* context) {
    UNUSED(context);
}
