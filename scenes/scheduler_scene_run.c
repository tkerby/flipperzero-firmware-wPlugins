#include "src/scheduler_app_i.h"
#include "src/scheduler_run.h"

#include <lib/subghz/devices/devices.h>

void scheduler_scene_run_on_enter(void* context) {
    SchedulerApp* app = context;
    scheduler_start(app->scheduler);

    scheduler_run_view_set_static_fields(app->run_view, app->scheduler);
    scheduler_run_view_update(app->run_view, app->scheduler, false);

    subghz_devices_init();
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewRunSchedule);
}

void scheduler_scene_run_on_exit(void* context) {
    SchedulerApp* app = context;

    // Back aborts a transmission in progress instead of waiting for it
    scheduler_stop_tx(app);

    scheduler_time_reset(app->scheduler);
    subghz_devices_deinit();
}

bool scheduler_scene_run_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;

    if(event.type != SceneManagerEventTypeTick) {
        return false;
    }

    if(scheduler_tx_finished(app)) {
        scheduler_cleanup_tx(app);

        if(scheduler_get_tx_mode(app->scheduler) == SchedulerTxModeOneShot &&
           scheduler_one_shot_done(app->scheduler)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SchedulerSceneStart);
            return true;
        }

        scheduler_tx_done(app->scheduler);
    }

    const uint8_t tick_counter = scheduler_run_view_get_tick_counter(app->run_view);
    const bool busy = app->thread != NULL;

    if(!busy && scheduler_time_to_trigger(app->scheduler)) {
        const int16_t line = scheduler_fire(app->scheduler);
        scheduler_start_tx(app, line);
    } else if(
        !busy && !(tick_counter % 2) && scheduler_is_armed(app->scheduler) &&
        scheduler_get_countdown_seconds(app->scheduler) <= IDLE_BLINK_WINDOW_S) {
        // Keep quiet during long waits, only blink in the last minute before a send
        notification_message(app->notifications, &sequence_blink_red_10);
    }

    scheduler_run_view_update(app->run_view, app->scheduler, app->thread != NULL);
    scheduler_run_view_inc_tick_counter(app->run_view);

    return true;
}
