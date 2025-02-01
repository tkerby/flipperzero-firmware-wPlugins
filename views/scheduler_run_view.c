#include <stdio.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>

#include "src/scheduler_run.h"
#include "src/scheduler_app_i.h"

#define TAG "SubGHzSchedulerRunView"

#define DEBUG

#define GUI_DISPLAY_HEIGHT 64
#define GUI_DISPLAY_WIDTH  128
#define GUI_MARGIN         5
#define GUI_TEXT_GAP       10

static void scheduler_update_widgets(void* context, char* time_til_next) {
    SchedulerApp* app = context;
    uint32_t value;
    char* text;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        GUI_DISPLAY_WIDTH / 2,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        "Schedule Running");

    value = scheduler_get_interval(app->scheduler);
    text = (char*)interval_text[value];
    widget_add_string_element(
        app->widget,
        GUI_MARGIN,
        GUI_TEXT_GAP * 2,
        AlignLeft,
        AlignCenter,
        FontSecondary,
        "Time interval:");
    widget_add_string_element(
        app->widget,
        GUI_DISPLAY_WIDTH - GUI_MARGIN,
        GUI_TEXT_GAP * 2,
        AlignRight,
        AlignCenter,
        FontSecondary,
        text);

    value = scheduler_get_tx_repeats(app->scheduler);
    if(value > 0) {
        text = (char*)tx_repeats_text[value];
        widget_add_string_element(
            app->widget,
            GUI_MARGIN,
            GUI_TEXT_GAP * 3,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            "TX Repeats:");
        widget_add_string_element(
            app->widget,
            GUI_DISPLAY_WIDTH - GUI_MARGIN,
            GUI_TEXT_GAP * 3,
            AlignRight,
            AlignCenter,
            FontSecondary,
            text);
    }

    widget_add_string_element(
        app->widget,
        GUI_MARGIN,
        (GUI_TEXT_GAP * 5) - 2,
        AlignLeft,
        AlignCenter,
        FontSecondary,
        "File: ");
    widget_add_string_element(
        app->widget,
        GUI_DISPLAY_WIDTH - GUI_MARGIN,
        (GUI_TEXT_GAP * 5) - 2,
        AlignRight,
        AlignCenter,
        FontSecondary,
        scheduler_get_file_name(app->scheduler));

    widget_add_string_element(
        app->widget,
        GUI_MARGIN,
        GUI_DISPLAY_HEIGHT - GUI_MARGIN - 1,
        AlignLeft,
        AlignCenter,
        FontSecondary,
        "Next TX: ");
    widget_add_string_element(
        app->widget,
        GUI_DISPLAY_WIDTH - GUI_MARGIN,
        GUI_DISPLAY_HEIGHT - GUI_MARGIN - 1,
        AlignRight,
        AlignCenter,
        FontSecondary,
        time_til_next);

    widget_add_frame_element(app->widget, 0, 0, GUI_DISPLAY_WIDTH, GUI_DISPLAY_HEIGHT, 5);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerSceneRunSchedule);
}

static void update_countdown(SchedulerApp* app) {
    char countdown[16];
    scheduler_get_countdown_fmt(app->scheduler, countdown, sizeof(countdown));
    scheduler_update_widgets(app, countdown);
}

void scheduler_scene_run_on_enter(void* context) {
    SchedulerApp* app = context;
    furi_hal_power_suppress_charge_enter();
    scheduler_reset(app->scheduler);
    update_countdown(app);
    subghz_devices_init();
}

void scheduler_scene_run_on_exit(void* context) {
    SchedulerApp* app = context;
    scheduler_reset(app->scheduler);
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();
}

bool scheduler_scene_run_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(scheduler_time_to_trigger(app->scheduler)) {
            update_countdown(app);
            scheduler_tx(app);
        } else {
            update_countdown(app);
            notification_message(app->notifications, &sequence_blink_red_10);
        }
        consumed = true;
    }

    return consumed;
}
