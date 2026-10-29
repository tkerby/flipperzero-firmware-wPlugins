#include "../tea_timer_i.h"
#include "tea_timer_scene.h"

#define TAG "TeaTimerBrew"

/** Сигнал завершения пролива: две ноты + вибро (отправляется один раз). */
static const NotificationSequence tea_timer_sequence_done = {
    &message_vibro_on,
    &message_note_c6,
    &message_delay_100,
    &message_note_e6,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_10,
    NULL,
};

/** Принудительно держать подсветку включённой во время отсчёта (FR-017). */
static void tea_timer_scene_brew_backlight_on(TeaTimerApp* app) {
    notification_message(app->notification, &sequence_display_backlight_enforce_on);
}

/** Вернуть подсветку в системный авторежим. */
static void tea_timer_scene_brew_backlight_auto(TeaTimerApp* app) {
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}

void tea_timer_scene_brew_on_enter(void* context) {
    TeaTimerApp* app = context;

    brew_view_update(app->brew_view, &app->session);
    view_dispatcher_switch_to_view(app->view_dispatcher, TeaTimerViewBrew);

    if(app->session.phase == BrewPhaseRUNNING) {
        tea_timer_scene_brew_backlight_on(app);
    }
}

bool tea_timer_scene_brew_on_event(void* context, SceneManagerEvent event) {
    TeaTimerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case TeaTimerEventTick:
            if(app->session.phase == BrewPhaseRUNNING) {
                uint16_t remaining =
                    brew_session_remaining_sec(furi_get_tick(), app->session.deadline_tick);
                // Переход RUNNING → DONE выполняется ровно один раз: brew_session_complete()
                // возвращает true только при этом переходе, поэтому сигнал не повторяется.
                if(remaining == 0 && brew_session_complete(&app->session)) {
                    notification_message(app->notification, &tea_timer_sequence_done);
                    tea_timer_scene_brew_backlight_auto(app);
                    FURI_LOG_I(
                        TAG,
                        "done %us, pours %u",
                        (unsigned)app->session.next_pour_sec,
                        (unsigned)app->session.completed_pours);
                }
            }
            // RUNNING — обновляем остаток; DONE — только перерисовываем, автозапуска нет.
            if(app->session.phase != BrewPhaseIDLE) {
                brew_view_update(app->brew_view, &app->session);
            }
            consumed = true;
            break;

        case TeaTimerEventBrewStart:
            // Повторный старт в RUNNING игнорируется (brew_session_start не сдвигает дедлайн).
            if(app->session.phase != BrewPhaseRUNNING) {
                brew_session_start(furi_get_tick(), &app->session);
                tea_timer_scene_brew_backlight_on(app);
                FURI_LOG_I(
                    TAG,
                    "pour %u start %us",
                    (unsigned)(app->session.completed_pours + 1),
                    (unsigned)app->session.next_pour_sec);
                brew_view_update(app->brew_view, &app->session);
            }
            consumed = true;
            break;

        case TeaTimerEventBrewStop:
            brew_session_abort(&app->session);
            tea_timer_scene_brew_backlight_auto(app);
            FURI_LOG_I(TAG, "abort");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        brew_session_abort(&app->session);
        tea_timer_scene_brew_backlight_auto(app);
        FURI_LOG_I(TAG, "back, abort");
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void tea_timer_scene_brew_on_exit(void* context) {
    TeaTimerApp* app = context;

    tea_timer_scene_brew_backlight_auto(app);
    notification_message(app->notification, &sequence_reset_sound);
    notification_message(app->notification, &sequence_reset_vibro);
}
