#include <furi.h>
#include <furi_hal_gpio.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <applications/services/power/power_service/power.h>

#include <stdio.h>

#define SERVO_ANGLE_MIN 0
#define SERVO_ANGLE_MAX 180
#define SERVO_ANGLE_STEP 10
#define SERVO_PULSE_MIN_US 1000
#define SERVO_PULSE_MAX_US 2000
#define SERVO_FRAME_REST_MS 18
#define SWEEP_DWELL_MS 2000

typedef enum {
    ServoModeManual,
    ServoModeCenter,
    ServoModeSweep,
} ServoMode;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriThread* pulse_thread;
    FuriTimer* sweep_timer;
    ViewPort* view_port;
    Gui* gui;
    Power* power;
    uint16_t pulse_us;
    uint8_t angle;
    ServoMode mode;
    bool running;
    bool show_help;
    bool otg_was_enabled;
    bool otg_enabled_by_app;
    bool five_volts_available;
} ServoTesterApp;

static uint16_t angle_to_pulse(uint8_t angle) {
    return SERVO_PULSE_MIN_US +
           ((SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US) * angle) / SERVO_ANGLE_MAX;
}

static int32_t servo_pulse_worker(void* context) {
    ServoTesterApp* app = context;

    while(true) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool running = app->running;
        uint16_t pulse_us = app->pulse_us;
        furi_mutex_release(app->mutex);

        if(!running) break;

        furi_hal_gpio_write(&gpio_ext_pa7, true);
        furi_delay_us(pulse_us);
        furi_hal_gpio_write(&gpio_ext_pa7, false);
        furi_delay_ms(SERVO_FRAME_REST_MS);
    }

    furi_hal_gpio_write(&gpio_ext_pa7, false);
    return 0;
}

static void servo_tester_draw_help(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 17, "Pin 1 +5V -> servo +");
    canvas_draw_str(canvas, 5, 26, "Pin 2 PA7 -> signal");
    canvas_draw_str(canvas, 5, 35, "GND -> servo -");
    canvas_draw_str(canvas, 5, 44, "5V auto on; max 1.2A");
    canvas_draw_str(canvas, 5, 53, "Ext supply: share GND");
    canvas_draw_str(canvas, 5, 63, "OK/Back: return");
}

static void servo_tester_draw(Canvas* canvas, void* context) {
    ServoTesterApp* app = context;
    uint8_t angle;
    ServoMode mode;
    bool show_help;
    bool five_volts_available;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    angle = app->angle;
    mode = app->mode;
    show_help = app->show_help;
    five_volts_available = app->five_volts_available;
    furi_mutex_release(app->mutex);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, show_help ? "SERVO WIRING" : "SERVO TESTER");

    if(show_help) {
        servo_tester_draw_help(canvas);
        return;
    }

    canvas_draw_str(canvas, 86, 9, five_volts_available ? "5V ON" : "5V CHECK");
    canvas_draw_str(canvas, 18, 21, "Mode (U/D)");
    canvas_draw_str(canvas, 79, 21, "Angle L/R");

    const char* mode_names[] = {"Manual", "Center", "Sweep"};
    const uint8_t row_y[] = {30, 41, 52};
    for(uint8_t i = 0; i < 3; i++) {
        if(mode == (ServoMode)i) {
            canvas_draw_disc(canvas, 7, row_y[i] - 3, 3);
        } else {
            canvas_draw_circle(canvas, 7, row_y[i] - 3, 3);
        }
        canvas_draw_str(canvas, 17, row_y[i], mode_names[i]);
    }

    char angle_text[8];
    snprintf(angle_text, sizeof(angle_text), "%u", angle);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 101, 43, AlignCenter, AlignCenter, angle_text);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_circle(canvas, 118, 34, 2);
    canvas_draw_str(canvas, 4, 63, "OK: wiring   BACK: exit");
}

static void servo_tester_input(InputEvent* event, void* context) {
    ServoTesterApp* app = context;
    furi_message_queue_put(app->event_queue, event, 0);
}

static void servo_sweep_tick(void* context) {
    ServoTesterApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->running && app->mode == ServoModeSweep) {
        app->angle = app->angle == SERVO_ANGLE_MIN ? SERVO_ANGLE_MAX : SERVO_ANGLE_MIN;
        app->pulse_us = angle_to_pulse(app->angle);
    }
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void servo_set_angle(ServoTesterApp* app, uint8_t angle) {
    if(angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->angle = angle;
    app->pulse_us = angle_to_pulse(angle);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void servo_set_mode(ServoTesterApp* app, ServoMode mode) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->mode = mode;
    furi_mutex_release(app->mutex);

    if(mode == ServoModeSweep) {
        servo_set_angle(app, SERVO_ANGLE_MIN);
        furi_timer_start(app->sweep_timer, furi_ms_to_ticks(SWEEP_DWELL_MS));
    } else {
        furi_timer_stop(app->sweep_timer);
        if(mode == ServoModeCenter) servo_set_angle(app, 90);
        else view_port_update(app->view_port);
    }
}

static void servo_process_input(ServoTesterApp* app, InputEvent* event) {
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->show_help) app->show_help = false;
        else app->running = false;
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
        return;
    }

    if(event->type != InputTypePress && event->type != InputTypeRepeat) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool show_help = app->show_help;
    ServoMode mode = app->mode;
    uint8_t angle = app->angle;
    furi_mutex_release(app->mutex);

    if(show_help) {
        if(event->key == InputKeyOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->show_help = false;
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
        return;
    }

    if(event->key == InputKeyOk) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->show_help = true;
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    } else if(event->key == InputKeyUp) {
        ServoMode next = mode == ServoModeManual ? ServoModeSweep :
                         mode == ServoModeSweep ? ServoModeCenter : ServoModeManual;
        servo_set_mode(app, next);
    } else if(event->key == InputKeyDown) {
        ServoMode next = mode == ServoModeManual ? ServoModeCenter :
                         mode == ServoModeCenter ? ServoModeSweep : ServoModeManual;
        servo_set_mode(app, next);
    } else if(mode == ServoModeManual && event->key == InputKeyLeft) {
        servo_set_angle(app, angle >= SERVO_ANGLE_STEP ? angle - SERVO_ANGLE_STEP : 0);
    } else if(mode == ServoModeManual && event->key == InputKeyRight) {
        uint16_t next_angle = angle + SERVO_ANGLE_STEP;
        servo_set_angle(app, next_angle > SERVO_ANGLE_MAX ? SERVO_ANGLE_MAX : next_angle);
    }
}

static bool servo_enable_5v(ServoTesterApp* app) {
    app->power = furi_record_open(RECORD_POWER);
    app->otg_was_enabled = power_is_otg_enabled(app->power);

    PowerInfo power_info;
    power_get_info(app->power, &power_info);
    bool usb_powered = power_info.voltage_vbus > 4.0f;

    if(!app->otg_was_enabled && !usb_powered) {
        power_enable_otg(app->power, true);
        app->otg_enabled_by_app = power_is_otg_enabled(app->power);
    }

    app->five_volts_available = usb_powered || power_is_otg_enabled(app->power);
    return app->five_volts_available;
}

static void servo_restore_5v(ServoTesterApp* app) {
    if(app->otg_enabled_by_app && !app->otg_was_enabled) {
        power_enable_otg(app->power, false);
    }
    furi_record_close(RECORD_POWER);
}

int32_t servotesterapp_app(void* p) {
    UNUSED(p);
    ServoTesterApp app = {
        .mutex = furi_mutex_alloc(FuriMutexTypeNormal),
        .event_queue = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .pulse_us = angle_to_pulse(90),
        .angle = 90,
        .mode = ServoModeManual,
        .running = true,
    };

    servo_enable_5v(&app);

    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_ext_pa7, false);

    app.pulse_thread = furi_thread_alloc_ex("ServoPulse", 512, servo_pulse_worker, &app);
    furi_thread_start(app.pulse_thread);
    app.sweep_timer = furi_timer_alloc(servo_sweep_tick, FuriTimerTypePeriodic, &app);

    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, servo_tester_draw, &app);
    view_port_input_callback_set(app.view_port, servo_tester_input, &app);
    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);
    view_port_update(app.view_port);

    while(app.running) {
        InputEvent event;
        if(furi_message_queue_get(app.event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            servo_process_input(&app, &event);
        }
    }

    furi_timer_stop(app.sweep_timer);
    furi_timer_free(app.sweep_timer);

    furi_mutex_acquire(app.mutex, FuriWaitForever);
    app.running = false;
    furi_mutex_release(app.mutex);
    furi_thread_join(app.pulse_thread);

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close(RECORD_GUI);
    furi_thread_free(app.pulse_thread);
    furi_message_queue_free(app.event_queue);
    servo_restore_5v(&app);
    furi_mutex_free(app.mutex);
    return 0;
}
