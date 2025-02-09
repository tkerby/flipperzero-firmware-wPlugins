#include <stdio.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <string.h>
#include <subghz_scheduler_icons.h>

#include "src/scheduler_run.h"
#include "src/scheduler_app_i.h"
#include "scheduler_run_view.h"

#define TAG "SubGHzSchedulerRunView"

#define GUI_DISPLAY_HEIGHT 64
#define GUI_DISPLAY_WIDTH  128
#define GUI_MARGIN         5
#define GUI_TEXT_GAP       10

#define GUI_TEXTBOX_HEIGHT 12
#define GUI_TABLE_ROW_A    13
#define GUI_TABLE_ROW_B    (GUI_TABLE_ROW_A + GUI_TEXTBOX_HEIGHT) - 1

struct SchedulerUIRunState {
    FuriString* header;
    FuriString* mode;
    FuriString* interval;
    FuriString* tx_repeats;
    FuriString* file_type;
    FuriString* file_name;
    FuriString* tx_countdown;

    //FuriString* current_tx_file;
    uint8_t list_count;
    uint8_t progress;
};

SchedulerUIRunState* state;

//void scheduler_set_current_tx_file(const char* file) {
//    furi_string_set(state->current_tx_file, file);
//}

static void scheduler_ui_run_state_alloc(SchedulerApp* app) {
    uint32_t value;
    state = malloc(sizeof(SchedulerUIRunState));
    state->header = furi_string_alloc_set("Schedule Running");

    value = scheduler_get_mode(app->scheduler);
    state->mode = furi_string_alloc_set(mode_text[value]);

    value = scheduler_get_interval(app->scheduler);
    state->interval = furi_string_alloc_set(interval_text[value]);

    value = scheduler_get_tx_repeats(app->scheduler);
    state->tx_repeats = furi_string_alloc_set(tx_repeats_text[value]);

    value = scheduler_get_file_type(app->scheduler);
    state->file_type = furi_string_alloc_set(file_type_text[value]);

    state->file_name = furi_string_alloc_set(scheduler_get_file_name(app->scheduler));

    state->tx_countdown = furi_string_alloc();
    state->list_count = scheduler_get_list_count(app->scheduler);
    state->progress = 0;
}

static void scheduler_ui_run_state_free() {
    furi_string_free(state->header);
    furi_string_free(state->mode);
    furi_string_free(state->interval);
    furi_string_free(state->tx_repeats);
    furi_string_free(state->file_type);
    furi_string_free(state->file_name);
    furi_string_free(state->tx_countdown);
    free(state);
}

void scheduler_update_progress(uint8_t x) {
    state->progress = x * (128 - 10) / state->list_count;
}

void scheduler_update_widgets(void* context) {
    SchedulerApp* app = context;

    widget_reset(app->widget);
    widget_add_frame_element(
        app->widget, 0, 0, GUI_DISPLAY_WIDTH, GUI_DISPLAY_HEIGHT, 5); /* Main Frame */

    /* ============= HEADER ============= */
    widget_add_frame_element(
        app->widget, 0, GUI_TABLE_ROW_A, GUI_DISPLAY_WIDTH, 2, 0); /* Header Underline */
    widget_add_string_element(
        app->widget,
        GUI_DISPLAY_WIDTH / 2,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        furi_string_get_cstr(state->header));

    /* ============= MODE ============= */
    widget_add_frame_element(
        app->widget, 0, GUI_TABLE_ROW_A, 49, GUI_TEXTBOX_HEIGHT, 0); /* Mode Box */
    widget_add_icon_element(app->widget, GUI_MARGIN, (GUI_TEXT_GAP * 2) - 4, &I_mode_7px);
    widget_add_string_element(
        app->widget,
        GUI_MARGIN + 10,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(state->mode));

    /* ============= INTERVAL ============= */
    widget_add_frame_element(
        app->widget, 48, GUI_TABLE_ROW_A, 48, GUI_TEXTBOX_HEIGHT, 0); /* Interval Box */
    widget_add_icon_element(app->widget, GUI_MARGIN + 48, (GUI_TEXT_GAP * 2) - 4, &I_time_7px);
    widget_add_string_element(
        app->widget,
        GUI_MARGIN + 58,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(state->interval));

    /* ============= TX REPEATS ============= */
    widget_add_frame_element(
        app->widget, 95, GUI_TABLE_ROW_A, 33, GUI_TEXTBOX_HEIGHT, 0); /* Repeats Box */
    widget_add_icon_element(app->widget, GUI_MARGIN + 96, (GUI_TEXT_GAP * 2) - 4, &I_rept_7px);
    widget_add_string_element(
        app->widget,
        GUI_MARGIN + 106,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(state->tx_repeats));

    /* ============= FILE NAME ============= */
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
        furi_string_get_cstr(state->file_name));

    /* ============= NEXT TX COUNTDOWN ============= */
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
        furi_string_get_cstr(state->tx_countdown));

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerSceneRunSchedule);
}

static void update_countdown(SchedulerApp* app) {
    char countdown[16];
    scheduler_get_countdown_fmt(app->scheduler, countdown, sizeof(countdown));
    furi_string_set(state->tx_countdown, countdown);
    scheduler_update_widgets(app);
}

void scheduler_scene_run_on_enter(void* context) {
    SchedulerApp* app = context;
    furi_hal_power_suppress_charge_enter();
    scheduler_reset(app->scheduler);
    scheduler_ui_run_state_alloc(app);
    update_countdown(app);
    subghz_devices_init();
    furi_delay_ms(100);
}

void scheduler_scene_run_on_exit(void* context) {
    SchedulerApp* app = context;
    if(app->thread) {
        furi_thread_join(app->thread);
    }
    scheduler_reset(app->scheduler);
    subghz_devices_deinit();
    scheduler_ui_run_state_free();
    furi_hal_power_suppress_charge_exit();
}

bool scheduler_scene_run_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->is_transmitting) {
            bool trig = scheduler_time_to_trigger(app->scheduler);
            update_countdown(app);

            if(trig) {
                scheduler_start_tx(app);
            } else {
                notification_message(app->notifications, &sequence_blink_red_10);
            }
            consumed = true;
        }
    }

    return consumed;
}
