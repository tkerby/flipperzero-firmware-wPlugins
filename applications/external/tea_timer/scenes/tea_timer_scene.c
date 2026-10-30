#include "tea_timer_scene.h"

#include <string.h>

/** Скопировать имя файла (basename) из полного пути; `memmove` — безопасно и «на месте». */
void tea_timer_basename(const char* path, char* out, size_t size) {
    if(out == NULL || size == 0) {
        return;
    }
    if(path == NULL) {
        out[0] = '\0';
        return;
    }

    const char* base = path;
    for(const char* p = path; *p != '\0'; p++) {
        if(*p == '/') {
            base = p + 1;
        }
    }

    size_t len = strlen(base);
    if(len > size - 1) {
        len = size - 1;
    }
    memmove(out, base, len);
    out[len] = '\0';
}

static void (*const tea_timer_scene_on_enter_handlers[])(void*) = {
    tea_timer_scene_menu_on_enter,
    tea_timer_scene_file_select_on_enter,
    tea_timer_scene_preset_card_on_enter,
    tea_timer_scene_brew_on_enter,
    tea_timer_scene_no_presets_on_enter,
    tea_timer_scene_error_on_enter,
    tea_timer_scene_actions_on_enter,
    tea_timer_scene_edit_name_on_enter,
    tea_timer_scene_edit_number_on_enter,
    tea_timer_scene_edit_summary_on_enter,
    tea_timer_scene_delete_confirm_on_enter,
    tea_timer_scene_saved_on_enter,
};

static bool (*const tea_timer_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    tea_timer_scene_menu_on_event,
    tea_timer_scene_file_select_on_event,
    tea_timer_scene_preset_card_on_event,
    tea_timer_scene_brew_on_event,
    tea_timer_scene_no_presets_on_event,
    tea_timer_scene_error_on_event,
    tea_timer_scene_actions_on_event,
    tea_timer_scene_edit_name_on_event,
    tea_timer_scene_edit_number_on_event,
    tea_timer_scene_edit_summary_on_event,
    tea_timer_scene_delete_confirm_on_event,
    tea_timer_scene_saved_on_event,
};

static void (*const tea_timer_scene_on_exit_handlers[])(void*) = {
    tea_timer_scene_menu_on_exit,
    tea_timer_scene_file_select_on_exit,
    tea_timer_scene_preset_card_on_exit,
    tea_timer_scene_brew_on_exit,
    tea_timer_scene_no_presets_on_exit,
    tea_timer_scene_error_on_exit,
    tea_timer_scene_actions_on_exit,
    tea_timer_scene_edit_name_on_exit,
    tea_timer_scene_edit_number_on_exit,
    tea_timer_scene_edit_summary_on_exit,
    tea_timer_scene_delete_confirm_on_exit,
    tea_timer_scene_saved_on_exit,
};

const SceneManagerHandlers tea_timer_scene_handlers = {
    .on_enter_handlers = tea_timer_scene_on_enter_handlers,
    .on_event_handlers = tea_timer_scene_on_event_handlers,
    .on_exit_handlers = tea_timer_scene_on_exit_handlers,
    .scene_num = TeaTimerSceneCount,
};
