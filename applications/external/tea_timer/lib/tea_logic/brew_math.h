#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../tea_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Предельная длительность пролива: 3600 + 99 * 600 секунд. */
#define BREW_NEXT_POUR_MAX 63000

/** Фаза таймера заваривания. */
typedef enum {
    BrewPhaseIDLE, /**< Ожидание запуска */
    BrewPhaseRUNNING, /**< Идёт отсчёт */
    BrewPhaseDONE, /**< Время истекло, ждём OK/BACK */
} BrewPhase;

/** Сессия заваривания: пресет и состояние проливов. */
typedef struct {
    bool has_preset;
    TeaPreset preset;
    uint16_t completed_pours;
    uint16_t next_pour_sec;
    BrewPhase phase;
    uint32_t deadline_tick;
} BrewSession;

/** Сбросить сессию по пресету: счётчик 0, время первого пролива, фаза IDLE. */
void brew_session_reset_from_preset(const TeaPreset* preset, BrewSession* session);

/** Длительность следующего пролива: `prev + increment`, не больше BREW_NEXT_POUR_MAX. */
uint16_t brew_math_pour_sec(uint16_t prev, uint16_t increment);

/** Запустить пролив: дедлайн = now + next_pour_sec * 1000. Повтор в RUNNING игнорируется. */
void brew_session_start(uint32_t now_tick, BrewSession* session);

/** Остаток в секундах с округлением вверх; 0, если время истекло. */
uint16_t brew_session_remaining_sec(uint32_t now_tick, uint32_t deadline_tick);

/**
 * Завершить пролив: единственное место, где растёт completed_pours и
 * пересчитывается next_pour_sec. Возвращает true только при переходе RUNNING → DONE.
 */
bool brew_session_complete(BrewSession* session);

/** Прервать пролив: RUNNING → IDLE без изменения счётчика и времени. */
void brew_session_abort(BrewSession* session);

/** Полный сброс сессии (удаление активного пресета): счётчик 0, фаза IDLE, путь пуст. */
void brew_session_clear(BrewSession* session);

/** true, если завершённых проливов больше рекомендованного числа. */
bool brew_session_over_limit(const BrewSession* session);

/** Отформатировать секунды как `mm:ss` (< 3600) или `hh:mm:ss`. */
void brew_math_format_time(uint16_t sec, char* buf, size_t size);

#ifdef __cplusplus
}
#endif
