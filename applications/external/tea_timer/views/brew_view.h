#pragma once

#include <gui/view.h>

#include "../tea_timer.h"
#include "../lib/tea_logic/brew_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Модель экрана заваривания — только представление, не источник истины. */
typedef struct {
    uint8_t phase; /**< BrewPhase */
    char name[TEA_NAME_MAX + 1]; /**< Название чая */
    uint16_t completed_pours;
    uint8_t max_pours;
    bool over_limit;
    uint16_t leaf_tenths_g;
    uint16_t pour_number; /**< completed_pours + 1 */
    uint16_t pour_sec; /**< Длительность следующего пролива */
    uint16_t remaining_sec; /**< Остаток текущего пролива */
    FuriString* name_string; /**< Внутренняя строка для обрезки названия по ширине */
} BrewViewModel;

typedef struct BrewView BrewView;

/** Создать экран заваривания (View + lock-free модель). */
BrewView* brew_view_alloc(void);

/** Освободить экран заваривания. */
void brew_view_free(BrewView* brew_view);

/** Получить View для ViewDispatcher. */
View* brew_view_get_view(BrewView* brew_view);

/** Скопировать состояние сессии в модель и перерисовать экран. */
void brew_view_update(BrewView* brew_view, const BrewSession* session);

#ifdef __cplusplus
}
#endif
