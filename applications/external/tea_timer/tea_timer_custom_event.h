#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Пользовательские события приложения Tea Timer. */
typedef enum {
    TeaTimerEventFileSelected, /**< В браузере выбран файл пресета */
    TeaTimerEventReloadPressed, /**< Нажата кнопка Reload (нет пресетов) */
    TeaTimerEventBrewStart, /**< OK на экране заваривания: запустить пролив */
    TeaTimerEventBrewStop, /**< OK/BACK во время отсчёта: прервать пролив */
    TeaTimerEventTick, /**< Тик 1 Гц из view_dispatcher */
    TeaTimerEventEditSave, /**< `Save` на сводке формы */
    TeaTimerEventEditSaved, /**< Запись завершена успешно */
    TeaTimerEventDeleteConfirmed, /**< Подтверждено удаление пресета */
    TeaTimerEventDeleteCancelled, /**< Удаление отменено */
    TeaTimerEventSavedDismissed, /**< `OK`/BACK на экране результата */
} TeaTimerCustomEvent;

#ifdef __cplusplus
}
#endif
