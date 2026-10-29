#pragma once

#include <gui/scene_manager.h>
#include <storage/storage.h>

#include "tea_timer_scene_config.h"

/** Предварительное объявление состояния приложения (полное — в `tea_timer_i.h`). */
typedef struct TeaTimerApp TeaTimerApp;

/** Сцены приложения Tea Timer. */
typedef enum {
    TeaTimerSceneMenu,
    TeaTimerSceneFileSelect,
    TeaTimerScenePresetCard,
    TeaTimerSceneBrew,
    TeaTimerSceneNoPresets, /**< Каталог пуст или отсутствует (FR-008) */
    TeaTimerSceneError, /**< SD-карта недоступна (FR-024) */
    TeaTimerSceneActions, /**< Меню действий над пресетом */
    TeaTimerSceneEditName, /**< Ввод названия */
    TeaTimerSceneEditNumber, /**< Числовые шаги формы */
    TeaTimerSceneEditSummary, /**< Сводка и запись */
    TeaTimerSceneDeleteConfirm, /**< Подтверждение удаления */
    TeaTimerSceneSaved, /**< Пресет сохранён/удалён */
    TeaTimerSceneCount,
} TeaTimerScene;

ADD_SCENE(tea_timer, menu, Menu)
ADD_SCENE(tea_timer, file_select, FileSelect)
ADD_SCENE(tea_timer, preset_card, PresetCard)
ADD_SCENE(tea_timer, brew, Brew)
ADD_SCENE(tea_timer, no_presets, NoPresets)
ADD_SCENE(tea_timer, error, Error)
ADD_SCENE(tea_timer, actions, Actions)
ADD_SCENE(tea_timer, edit_name, EditName)
ADD_SCENE(tea_timer, edit_number, EditNumber)
ADD_SCENE(tea_timer, edit_summary, EditSummary)
ADD_SCENE(tea_timer, delete_confirm, DeleteConfirm)
ADD_SCENE(tea_timer, saved, Saved)

/** Callback файлового браузера: выбран файл пресета. */
void tea_timer_scene_file_select_file_callback(void* context);

/** Вернуться к выбору файла без промежуточных сцен; при пустом каталоге — маршрутизация. */
void tea_timer_goto_file_select(TeaTimerApp* app);

/** Имя файла (basename) из полного пути; строка всегда завершается нулём. */
void tea_timer_basename(const char* path, char* out, size_t size);

/** Маршрутизация «Presets»: Error / NoPresets / FileSelect по состоянию носителя. */
void tea_timer_show_presets(TeaTimerApp* app);

/** Есть ли в корне каталога приложения хотя бы один файл `.tea`. */
bool tea_timer_has_presets(Storage* storage);

extern const SceneManagerHandlers tea_timer_scene_handlers;
