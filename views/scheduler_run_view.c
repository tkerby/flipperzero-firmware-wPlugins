#include <stdio.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <string.h>
#include <subghz_scheduler_icons.h>
#include <gui/elements.h>

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

struct SchedulerRunView {
    View* view;
};

struct SchedulerUIRunState {
    SchedulerApp* app;

    FuriString* header;
    FuriString* mode;
    FuriString* interval;
    FuriString* tx_repeats;
    FuriString* file_type;
    FuriString* file_name;
    FuriString* tx_countdown;

    //FuriTimer* scroll_timer;
    //uint8_t scroll_counter;
    uint8_t tick_counter;
    uint8_t list_count;
};
SchedulerUIRunState* state;

static void scheduler_run_view_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* ============= MAIN FRAME ============= */
    canvas_draw_rframe(canvas, 0, 0, GUI_DISPLAY_WIDTH, GUI_DISPLAY_HEIGHT, 5);

    /* ============= HEADER ============= */
    canvas_draw_frame(canvas, 0, GUI_TABLE_ROW_A, GUI_DISPLAY_WIDTH, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        GUI_DISPLAY_WIDTH / 2,
        2,
        AlignCenter,
        AlignTop,
        furi_string_get_cstr(state->header));

    canvas_set_font(canvas, FontSecondary);

    /* ============= MODE ============= */
    canvas_draw_frame(canvas, 0, GUI_TABLE_ROW_A, 49, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN, (GUI_TEXT_GAP * 2) - 4, &I_mode_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 10,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        furi_string_get_cstr(state->mode));

    /* ============= INTERVAL ============= */
    canvas_draw_frame(canvas, 48, GUI_TABLE_ROW_A, 48, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN + 48, (GUI_TEXT_GAP * 2) - 4, &I_time_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 58,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        furi_string_get_cstr(state->interval));

    /* ============= TX REPEATS ============= */
    canvas_draw_frame(canvas, 95, GUI_TABLE_ROW_A, 33, GUI_TEXTBOX_HEIGHT);
    canvas_draw_icon(canvas, GUI_MARGIN + 96, (GUI_TEXT_GAP * 2) - 4, &I_rept_7px);
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN + 106,
        (GUI_TEXT_GAP * 2),
        AlignLeft,
        AlignCenter,
        furi_string_get_cstr(state->tx_repeats));

    /* ============= FILE NAME ============= */
    canvas_draw_str_aligned(
        canvas, GUI_MARGIN, (GUI_TEXT_GAP * 5) - 2, AlignLeft, AlignCenter, "File: ");
    elements_scrollable_text_line(
        canvas,
        GUI_MARGIN + 22,
        (GUI_TEXT_GAP * 5) + 1,
        95,
        state->file_name,
        state->tick_counter,
        false);

    /* ============= NEXT TX COUNTDOWN ============= */
    canvas_draw_str_aligned(
        canvas,
        GUI_MARGIN,
        GUI_DISPLAY_HEIGHT - GUI_MARGIN - 1,
        AlignLeft,
        AlignCenter,
        "Next TX: ");
    canvas_draw_str_aligned(
        canvas,
        GUI_DISPLAY_WIDTH - GUI_MARGIN,
        GUI_DISPLAY_HEIGHT - GUI_MARGIN - 1,
        AlignRight,
        AlignCenter,
        furi_string_get_cstr(state->tx_countdown));
}

//static void scheduler_scroll_timer_callback(void* context) {
//    SchedulerApp* app = context;
//    state->scroll_counter++;
//    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerSceneRunSchedule);
//}

static void update_countdown(Scheduler* scheduler) {
    char countdown[16];
    scheduler_get_countdown_fmt(scheduler, countdown, sizeof(countdown));
    furi_string_set(state->tx_countdown, countdown);
}

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
    //state->scroll_timer =
    //    furi_timer_alloc(scheduler_scroll_timer_callback, FuriTimerTypePeriodic, app);

    state->list_count = scheduler_get_list_count(app->scheduler);
    state->app = app;
}

static void scheduler_ui_run_state_free() {
    furi_string_free(state->header);
    furi_string_free(state->mode);
    furi_string_free(state->interval);
    furi_string_free(state->tx_repeats);
    furi_string_free(state->file_type);
    furi_string_free(state->file_name);
    furi_string_free(state->tx_countdown);
    //furi_timer_free(state->scroll_timer);
    free(state);
}

void scheduler_scene_run_on_enter(void* context) {
    SchedulerApp* app = context;
    furi_hal_power_suppress_charge_enter();
    scheduler_reset(app->scheduler);
    scheduler_ui_run_state_alloc(app);
    subghz_devices_init();
    //furi_timer_start(state->scroll_timer, 500);
    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerSceneRunSchedule);
}

void scheduler_scene_run_on_exit(void* context) {
    SchedulerApp* app = context;
    if(app->thread) {
        furi_thread_join(app->thread);
    }
    //furi_timer_stop(state->scroll_timer);
    scheduler_reset(app->scheduler);
    subghz_devices_deinit();
    scheduler_ui_run_state_free();
    furi_hal_power_suppress_charge_exit();
}

bool scheduler_scene_run_on_event(void* context, SceneManagerEvent event) {
    SchedulerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->is_transmitting && !(state->tick_counter % 2)) {
            update_countdown(app->scheduler);
            bool trig = scheduler_time_to_trigger(app->scheduler);

            if(trig) {
                scheduler_start_tx(app);
            } else {
                notification_message(app->notifications, &sequence_blink_red_10);
            }
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerSceneRunSchedule);
        state->tick_counter++;
        consumed = true;
    }

    return consumed;
}

SchedulerRunView* scheduler_run_view_alloc() {
    SchedulerRunView* run_view = malloc(sizeof(SchedulerRunView));
    run_view->view = view_alloc();
    view_set_context(run_view->view, run_view);
    view_set_draw_callback(run_view->view, scheduler_run_view_draw_callback);
    return run_view;
}

void scheduler_run_view_free(SchedulerRunView* run_view) {
    furi_assert(run_view);
    view_free(run_view->view);
    free(run_view);
}

View* scheduler_run_view_get_view(SchedulerRunView* run_view) {
    furi_assert(run_view);
    return run_view->view;
}
