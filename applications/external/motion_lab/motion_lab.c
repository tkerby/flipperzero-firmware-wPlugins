// Motion Lab — measure motion with the Video Game Module.
//
// Two modes over one shared pipeline:
//   Meter  — arm, swing or hit something, read peak g, peak rotation and tempo
//   Logger — stream motion to a CSV on the SD card with a live graph
//
// The sensor thread runs at 1 kHz and only ever touches the Capture core (and
// the log buffer) under a mutex. The UI samples that state on a 20 Hz timer, so
// no drawing work ever happens at sample rate.

#include "capture.h"
#include "motion_sensor.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/elements.h>
#include <storage/storage.h>

#define TAG "MotionLab"

// Default trigger. Low enough that a deliberate flick registers, high enough
// that handling the device or setting it down does not.
#define TRIGGER_G_DEFAULT 1.5f

#define UI_PERIOD_MS 50u // 20 Hz redraw

// CSV is written at a tenth of the sample rate. Full rate would mean ~45 KB/s
// of text formatting inside a 1 kHz callback; 100 Hz resolves everything a
// swing or a drop does while keeping the file small and readable.
#define LOG_DECIM   10u
#define LOG_RATE_HZ (CAPTURE_RATE_HZ / LOG_DECIM)

// Flushed to the card by the UI thread, never by the sensor thread.
#define LOG_BUF_SIZE 2048u

#define GRAPH_W 128u

typedef enum {
    ViewIdMenu,
    ViewIdMeter,
    ViewIdLogger,
    ViewIdNoModule,
} ViewId;

typedef enum {
    MenuIndexMeter,
    MenuIndexLogger,
    MenuIndexCount,
} MenuIndex;

typedef enum {
    MeterPhaseReady, // armed, watching
    MeterPhaseCapturing,
    MeterPhaseResult,
} MeterPhase;

typedef struct {
    uint8_t index; // which mode is highlighted
    float live_g;
    float live_dps;
} HomeModel;

typedef struct {
    MeterPhase phase;
    float live_g;
    float live_dps;
    bool show_gyro; // which trace the graph is drawing
    CaptureResult result;
    bool has_result;
    float best_g;
    float best_dps;
    uint32_t captures;
} MeterModel;

typedef struct {
    bool recording;
    bool failed;
    float live_g;
    float live_dps;
    uint32_t samples;
    uint32_t seconds;
    char name[32];
    uint8_t trace_len;
    float trace[GRAPH_W];
} LoggerModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* home;
    View* meter;
    View* logger;
    Widget* no_module;
    FuriTimer* ui_timer;

    MotionSensor* sensor;
    Capture* capture;
    FuriMutex* lock; // guards capture, log buffer and the counters below

    // Session bests, kept across captures until the app exits.
    float best_g;
    float best_dps;
    uint32_t captures;
    bool result_pending; // set by the sensor thread, consumed by the UI

    // --- logger ---
    Storage* storage;
    File* file;
    bool logging;
    uint32_t log_samples;
    uint32_t log_decim;
    uint32_t log_start_tick;
    char log_name[32];
    uint8_t log_buf[LOG_BUF_SIZE];
    size_t log_len;
    bool log_failed;

    // Rolling graph for the logger, advanced by the UI timer.
    float live_trace[GRAPH_W];
    uint8_t live_trace_len;

    ViewId active;

    // Both of these are large enough that they must not live on a stack: the
    // result is ~1 KB and the staging buffer 2 KB, and the UI update used to
    // run on the timer service thread, whose stack is far smaller than that.
    CaptureResult tick_result;
    uint8_t flush_buf[LOG_BUF_SIZE];
} MotionLab;

// The timer only posts this; all the work happens on the view dispatcher's
// thread, which is the one with a stack sized for it.
#define EVENT_TICK 1u

// ---------------------------------------------------------------- formatting

// Floats are rendered by hand rather than with %f: the Flipper's printf pulls
// in double formatting, and two decimals is all these readings justify.
static void fmt_fixed2(char* buf, size_t cap, float v) {
    if(v < 0.0f) v = 0.0f;
    const uint32_t scaled = (uint32_t)(v * 100.0f + 0.5f);
    snprintf(
        buf, cap, "%lu.%02lu", (unsigned long)(scaled / 100u), (unsigned long)(scaled % 100u));
}

static void fmt_int(char* buf, size_t cap, float v) {
    if(v < 0.0f) v = 0.0f;
    snprintf(buf, cap, "%lu", (unsigned long)(v + 0.5f));
}

// ---------------------------------------------------------------- graph

// Draw a trace scaled to its own maximum, with a baseline. Self-scaling keeps
// a gentle motion readable instead of flat against the axis; the peak figure
// printed above it supplies the absolute number.
static void
    draw_trace(Canvas* canvas, uint8_t x, uint8_t y, uint8_t h, const float* v, uint8_t n) {
    canvas_draw_line(canvas, x, y + h, x + GRAPH_W - 1, y + h);
    if(n == 0) return;

    float max = 0.0f;
    for(uint8_t i = 0; i < n; i++) {
        if(v[i] > max) max = v[i];
    }
    if(max <= 0.0f) return;

    for(uint8_t i = 0; i < n && i < GRAPH_W; i++) {
        uint8_t bar = (uint8_t)((v[i] / max) * (float)h);
        if(bar > h) bar = h;
        if(bar > 0) canvas_draw_line(canvas, x + i, y + h - bar, x + i, y + h - 1);
    }
}

// ---------------------------------------------------------------- home view

// A gauge face: the Swing Meter's mark.
static void icon_gauge(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x, y + 7, x + 10, y + 7);
    canvas_draw_line(canvas, x + 1, y + 4, x + 3, y + 1);
    canvas_draw_line(canvas, x + 4, y, x + 6, y);
    canvas_draw_line(canvas, x + 7, y + 1, x + 9, y + 4);
    canvas_draw_line(canvas, x + 5, y + 6, x + 8, y + 2); // needle
}

// A recorded trace: the Motion Logger's mark.
static void icon_trace(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x, y + 5, x + 1, y + 5);
    canvas_draw_line(canvas, x + 1, y + 5, x + 2, y + 2);
    canvas_draw_line(canvas, x + 2, y + 2, x + 3, y + 7);
    canvas_draw_line(canvas, x + 3, y + 7, x + 4, y);
    canvas_draw_line(canvas, x + 4, y, x + 5, y + 6);
    canvas_draw_line(canvas, x + 5, y + 6, x + 6, y + 3);
    canvas_draw_line(canvas, x + 6, y + 3, x + 8, y + 5);
    canvas_draw_line(canvas, x + 8, y + 5, x + 10, y + 5);
}

// One selectable mode. Selected draws solid with inverted text, so the
// highlight reads at a glance instead of relying on a caret.
static void draw_card(
    Canvas* canvas,
    uint8_t y,
    bool selected,
    const char* title,
    const char* subtitle,
    void (*icon)(Canvas*, uint8_t, uint8_t)) {
    const uint8_t h = 20;

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        elements_slightly_rounded_box(canvas, 0, y, 128, h);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
        elements_slightly_rounded_frame(canvas, 0, y, 128, h);
    }

    icon(canvas, 5, y + 5);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 21, y + 9, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 21, y + 16, subtitle);

    canvas_set_color(canvas, ColorBlack);
}

// Live motion against the trigger threshold, with a tick showing how hard a
// movement has to be before a capture fires. Shared so the home screen and the
// ready screen agree on what "close to triggering" looks like.
static void draw_motion_bar(Canvas* canvas, uint8_t y, float live_g) {
    canvas_draw_frame(canvas, 0, y, 128, 7);

    float scale = live_g / (TRIGGER_G_DEFAULT * 2.0f);
    if(scale > 1.0f) scale = 1.0f;
    const uint8_t fill = (uint8_t)(scale * 126.0f);
    if(fill > 0) canvas_draw_box(canvas, 1, y + 1, fill, 5);

    const uint8_t tick = 1 + (uint8_t)(0.5f * 126.0f);
    canvas_draw_line(canvas, tick, y - 2, tick, y - 1);
}

static void home_draw(Canvas* canvas, void* model) {
    HomeModel* m = model;
    char buf[16];
    char line[32];

    canvas_clear(canvas);

    // Title bar, inverted so the app reads as a tool rather than a list.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 9, "MOTION LAB");

    // Live reading, right aligned. Its presence is also the proof that the
    // module is attached and streaming.
    canvas_set_font(canvas, FontSecondary);
    fmt_fixed2(buf, sizeof(buf), m->live_g);
    snprintf(line, sizeof(line), "%s g", buf);
    canvas_draw_str_aligned(canvas, 125, 5, AlignRight, AlignCenter, line);
    canvas_set_color(canvas, ColorBlack);

    draw_card(
        canvas, 13, m->index == MenuIndexMeter, "Swing Meter", "peak force, tempo", icon_gauge);
    draw_card(
        canvas, 34, m->index == MenuIndexLogger, "Motion Logger", "record to CSV", icon_trace);

    draw_motion_bar(canvas, 56, m->live_g);
}

static void home_enter_mode(MotionLab* app, uint8_t index);

static bool home_input(InputEvent* event, void* context) {
    MotionLab* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            app->home, HomeModel * m, { m->index = (m->index + 1u) % MenuIndexCount; }, true);
        return true;
    }

    if(event->key == InputKeyOk) {
        uint8_t index = 0;
        with_view_model(app->home, HomeModel * m, { index = m->index; }, false);
        home_enter_mode(app, index);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------- meter view

static void meter_draw(Canvas* canvas, void* model) {
    MeterModel* m = model;
    char buf[24];
    char line[48];

    canvas_clear(canvas);

    if(m->phase == MeterPhaseResult && m->has_result) {
        canvas_set_font(canvas, FontPrimary);
        fmt_fixed2(buf, sizeof(buf), m->result.peak_accel_g);
        snprintf(line, sizeof(line), "%s g", buf);
        canvas_draw_str(canvas, 2, 11, line);

        canvas_set_font(canvas, FontSecondary);
        fmt_int(buf, sizeof(buf), m->result.peak_gyro_dps);
        snprintf(line, sizeof(line), "%s dps", buf);
        canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignCenter, line);

        if(m->result.tempo_valid) {
            fmt_fixed2(buf, sizeof(buf), m->result.tempo_ratio);
            snprintf(line, sizeof(line), "tempo %s:1", buf);
        } else {
            // An impact has no backswing to compare against, so report how long
            // it lasted rather than a meaningless ratio.
            snprintf(line, sizeof(line), "hit %lums", (unsigned long)m->result.duration_ms);
        }
        canvas_draw_str(canvas, 2, 21, line);

        // Right aligned: the capture count reaches three digits, and laying
        // these out from the left ran them off the edge of the screen.
        snprintf(line, sizeof(line), "#%lu", (unsigned long)m->captures);
        canvas_draw_str_aligned(canvas, 126, 18, AlignRight, AlignCenter, line);

        snprintf(line, sizeof(line), "%lums", (unsigned long)m->result.time_to_peak_ms);
        canvas_draw_str_aligned(canvas, 104, 18, AlignRight, AlignCenter, line);

        draw_trace(
            canvas,
            0,
            24,
            28,
            m->show_gyro ? m->result.trace_gyro_dps : m->result.trace_accel_g,
            m->result.trace_len);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 62, m->show_gyro ? "rotation" : "force");
        elements_button_center(canvas, "Again");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, m->phase == MeterPhaseCapturing ? "Capturing..." : "Ready");

    canvas_set_font(canvas, FontSecondary);
    fmt_fixed2(buf, sizeof(buf), m->live_g);
    snprintf(line, sizeof(line), "now %s g", buf);
    canvas_draw_str(canvas, 2, 24, line);

    fmt_int(buf, sizeof(buf), m->live_dps);
    snprintf(line, sizeof(line), "%s dps", buf);
    canvas_draw_str_aligned(canvas, 126, 21, AlignRight, AlignCenter, line);

    if(m->captures > 0) {
        fmt_fixed2(buf, sizeof(buf), m->best_g);
        snprintf(line, sizeof(line), "best %s g", buf);
        canvas_draw_str(canvas, 2, 36, line);

        fmt_int(buf, sizeof(buf), m->best_dps);
        snprintf(line, sizeof(line), "%s dps", buf);
        canvas_draw_str_aligned(canvas, 126, 33, AlignRight, AlignCenter, line);

        snprintf(line, sizeof(line), "%lu captured", (unsigned long)m->captures);
        canvas_draw_str(canvas, 2, 47, line);
    } else {
        canvas_draw_str(canvas, 2, 36, "Swing or tap the Flipper.");
        canvas_draw_str(canvas, 2, 47, "Hold still first.");
    }

    // Same bar as the home screen, so "how close am I to triggering" reads the
    // same way in both places.
    draw_motion_bar(canvas, 56, m->live_g);
}

static bool meter_input(InputEvent* event, void* context) {
    MotionLab* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        furi_mutex_acquire(app->lock, FuriWaitForever);
        capture_arm(app->capture, TRIGGER_G_DEFAULT);
        app->result_pending = false;
        furi_mutex_release(app->lock);

        with_view_model(
            app->meter,
            MeterModel * m,
            {
                m->phase = MeterPhaseReady;
                m->has_result = false;
            },
            true);
        return true;
    }

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        with_view_model(app->meter, MeterModel * m, { m->show_gyro = !m->show_gyro; }, true);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------- logger view

static void logger_draw(Canvas* canvas, void* model) {
    LoggerModel* m = model;
    char buf[24];
    char line[48];

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->failed) {
        canvas_draw_str(canvas, 2, 11, "Write failed");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Check the SD card");
        return;
    }

    canvas_draw_str(canvas, 2, 11, m->recording ? "Recording" : "Logger");

    canvas_set_font(canvas, FontSecondary);
    if(m->recording) {
        snprintf(
            line,
            sizeof(line),
            "%lu:%02lu  %lu pts",
            (unsigned long)(m->seconds / 60u),
            (unsigned long)(m->seconds % 60u),
            (unsigned long)m->samples);
        canvas_draw_str(canvas, 60, 11, line);
        canvas_draw_str(canvas, 2, 22, m->name);
    } else {
        fmt_fixed2(buf, sizeof(buf), m->live_g);
        snprintf(line, sizeof(line), "now %s g", buf);
        canvas_draw_str(canvas, 2, 22, line);

        fmt_int(buf, sizeof(buf), m->live_dps);
        snprintf(line, sizeof(line), "%s dps", buf);
        canvas_draw_str(canvas, 70, 22, line);
    }

    draw_trace(canvas, 0, 26, 26, m->trace, m->trace_len);

    elements_button_center(canvas, m->recording ? "Stop" : "Record");
}

// Forward declarations: input toggles recording, which owns the file.
static void logger_start(MotionLab* app);
static void logger_stop(MotionLab* app);

static bool logger_input(InputEvent* event, void* context) {
    MotionLab* app = context;
    if(event->type != InputTypeShort || event->key != InputKeyOk) return false;

    if(app->logging) {
        logger_stop(app);
    } else {
        logger_start(app);
    }
    return true;
}

// ---------------------------------------------------------------- logging

static void logger_start(MotionLab* app) {
    storage_common_mkdir(app->storage, APP_DATA_PATH(""));

    // Find a name that is not taken yet, so a session never overwrites one.
    char path[96];
    uint32_t n = 1;
    for(; n < 1000; n++) {
        snprintf(app->log_name, sizeof(app->log_name), "motion_%03lu.csv", (unsigned long)n);
        snprintf(path, sizeof(path), "%s/%s", APP_DATA_PATH(""), app->log_name);
        if(!storage_common_exists(app->storage, path)) break;
    }

    app->file = storage_file_alloc(app->storage);
    bool ok = storage_file_open(app->file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(ok) {
        // Integer milli-units: exact, cheap to format, and trivially parsed by
        // a spreadsheet or a plotting script.
        const char* header = "t_ms,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps\n";
        ok = storage_file_write(app->file, header, strlen(header)) == strlen(header);
    }

    if(!ok) {
        FURI_LOG_E(TAG, "could not open %s", path);
        storage_file_free(app->file);
        app->file = NULL;
        app->log_failed = true;
        with_view_model(app->logger, LoggerModel * m, { m->failed = true; }, true);
        return;
    }

    furi_mutex_acquire(app->lock, FuriWaitForever);
    app->log_len = 0;
    app->log_samples = 0;
    app->log_decim = 0;
    app->log_failed = false;
    app->logging = true;
    furi_mutex_release(app->lock);

    app->log_start_tick = furi_get_tick();
}

// Move whatever the sensor thread has buffered onto the card. Called from the
// UI timer and at stop, never from the sensor callback.
static void logger_flush(MotionLab* app) {
    size_t len;

    furi_mutex_acquire(app->lock, FuriWaitForever);
    len = app->log_len;
    if(len) memcpy(app->flush_buf, app->log_buf, len);
    app->log_len = 0;
    furi_mutex_release(app->lock);

    if(len == 0 || !app->file) return;

    if(storage_file_write(app->file, app->flush_buf, len) != len) {
        FURI_LOG_E(TAG, "log write failed");
        app->log_failed = true;
    }
}

static void logger_stop(MotionLab* app) {
    furi_mutex_acquire(app->lock, FuriWaitForever);
    app->logging = false;
    furi_mutex_release(app->lock);

    logger_flush(app);

    if(app->file) {
        storage_file_close(app->file);
        storage_file_free(app->file);
        app->file = NULL;
    }

    with_view_model(
        app->logger,
        LoggerModel * m,
        {
            m->recording = false;
            m->failed = app->log_failed;
        },
        true);
}

// ---------------------------------------------------------------- sensor path

// Runs on the sensor thread at 1 kHz. Everything here is O(1) and allocation
// free; anything heavier belongs in the UI timer.
static void on_sample(const CaptureSample* s, void* context) {
    MotionLab* app = context;

    furi_mutex_acquire(app->lock, FuriWaitForever);

    if(capture_feed(app->capture, s)) {
        CaptureResult r;
        if(capture_result(app->capture, &r)) {
            if(r.peak_accel_g > app->best_g) app->best_g = r.peak_accel_g;
            if(r.peak_gyro_dps > app->best_dps) app->best_dps = r.peak_gyro_dps;
            app->captures++;
            app->result_pending = true;
        }
    }

    if(app->logging) {
        if(++app->log_decim >= LOG_DECIM) {
            app->log_decim = 0;

            // Drop samples rather than block the sensor thread if the card
            // cannot keep up. The sample count in the UI reflects what was
            // actually written, so a gap is visible rather than silent.
            if(app->log_len + 64u <= LOG_BUF_SIZE) {
                const uint32_t t_ms = (app->log_samples * 1000u) / LOG_RATE_HZ;
                const int n = snprintf(
                    (char*)app->log_buf + app->log_len,
                    LOG_BUF_SIZE - app->log_len,
                    "%lu,%d,%d,%d,%d,%d,%d\n",
                    (unsigned long)t_ms,
                    (int)(s->ax * 1000.0f),
                    (int)(s->ay * 1000.0f),
                    (int)(s->az * 1000.0f),
                    (int)(s->gx * 1000.0f),
                    (int)(s->gy * 1000.0f),
                    (int)(s->gz * 1000.0f));
                if(n > 0) {
                    app->log_len += (size_t)n;
                    app->log_samples++;
                }
            }
        }
    }

    furi_mutex_release(app->lock);
}

// ---------------------------------------------------------------- ui timer

static void ui_update(MotionLab* app) {
    float live_g, live_dps;
    bool pending;
    CaptureState state;
    bool have_result = false;

    furi_mutex_acquire(app->lock, FuriWaitForever);
    live_g = capture_live_accel_g(app->capture);
    live_dps = capture_live_gyro_dps(app->capture);
    state = capture_state(app->capture);
    pending = app->result_pending;
    if(pending) {
        have_result = capture_result(app->capture, &app->tick_result);
        app->result_pending = false;
    }
    furi_mutex_release(app->lock);

    // Rolling live graph, one point per tick, newest at the right.
    if(app->live_trace_len < GRAPH_W) {
        app->live_trace[app->live_trace_len++] = live_g;
    } else {
        memmove(app->live_trace, app->live_trace + 1, sizeof(float) * (GRAPH_W - 1));
        app->live_trace[GRAPH_W - 1] = live_g;
    }

    if(app->active == ViewIdMenu) {
        with_view_model(
            app->home,
            HomeModel * m,
            {
                m->live_g = live_g;
                m->live_dps = live_dps;
            },
            true);
    } else if(app->active == ViewIdMeter) {
        with_view_model(
            app->meter,
            MeterModel * m,
            {
                m->live_g = live_g;
                m->live_dps = live_dps;
                m->best_g = app->best_g;
                m->best_dps = app->best_dps;
                m->captures = app->captures;
                if(have_result) {
                    m->result = app->tick_result;
                    m->has_result = true;
                    m->phase = MeterPhaseResult;
                } else if(m->phase != MeterPhaseResult) {
                    m->phase = (state == CaptureRecording) ? MeterPhaseCapturing : MeterPhaseReady;
                }
            },
            true);
    } else if(app->active == ViewIdLogger) {
        if(app->logging) logger_flush(app);

        const uint32_t secs = app->logging ? (furi_get_tick() - app->log_start_tick) /
                                                 furi_kernel_get_tick_frequency() :
                                             0;

        with_view_model(
            app->logger,
            LoggerModel * m,
            {
                m->recording = app->logging;
                m->failed = app->log_failed;
                m->live_g = live_g;
                m->live_dps = live_dps;
                m->samples = app->log_samples;
                m->seconds = secs;
                strncpy(m->name, app->log_name, sizeof(m->name) - 1);
                m->trace_len = app->live_trace_len;
                memcpy(m->trace, app->live_trace, sizeof(m->trace));
            },
            true);
    }
}

// Runs on the timer service thread, whose stack is small. It does nothing but
// post an event, so every heavy update happens on the dispatcher's thread.
static void ui_timer_cb(void* context) {
    MotionLab* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EVENT_TICK);
}

static bool on_custom_event(void* context, uint32_t event) {
    MotionLab* app = context;
    if(event == EVENT_TICK) {
        ui_update(app);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- navigation

static uint32_t nav_to_menu(void* context) {
    MotionLab* app = context;
    if(app->logging) logger_stop(app);

    furi_mutex_acquire(app->lock, FuriWaitForever);
    capture_disarm(app->capture);
    furi_mutex_release(app->lock);

    app->active = ViewIdMenu;
    return ViewIdMenu;
}

static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void home_enter_mode(MotionLab* app, uint8_t index) {
    if(index == MenuIndexMeter) {
        furi_mutex_acquire(app->lock, FuriWaitForever);
        capture_arm(app->capture, TRIGGER_G_DEFAULT);
        app->result_pending = false;
        furi_mutex_release(app->lock);

        with_view_model(
            app->meter,
            MeterModel * m,
            {
                m->phase = MeterPhaseReady;
                m->has_result = false;
            },
            false);

        app->active = ViewIdMeter;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMeter);
    } else {
        furi_mutex_acquire(app->lock, FuriWaitForever);
        capture_disarm(app->capture);
        furi_mutex_release(app->lock);

        app->active = ViewIdLogger;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdLogger);
    }
}

// ---------------------------------------------------------------- app

static MotionLab* motion_lab_alloc(void) {
    MotionLab* app = malloc(sizeof(MotionLab));
    memset(app, 0, sizeof(MotionLab));

    app->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    app->capture = capture_alloc(TRIGGER_G_DEFAULT);
    app->sensor = motion_sensor_alloc();
    app->storage = furi_record_open(RECORD_STORAGE);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, on_custom_event);

    app->home = view_alloc();
    view_allocate_model(app->home, ViewModelTypeLocking, sizeof(HomeModel));
    view_set_context(app->home, app);
    view_set_draw_callback(app->home, home_draw);
    view_set_input_callback(app->home, home_input);
    view_set_previous_callback(app->home, nav_exit);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, app->home);

    app->meter = view_alloc();
    view_allocate_model(app->meter, ViewModelTypeLocking, sizeof(MeterModel));
    view_set_context(app->meter, app);
    view_set_draw_callback(app->meter, meter_draw);
    view_set_input_callback(app->meter, meter_input);
    view_set_previous_callback(app->meter, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMeter, app->meter);

    app->logger = view_alloc();
    view_allocate_model(app->logger, ViewModelTypeLocking, sizeof(LoggerModel));
    view_set_context(app->logger, app);
    view_set_draw_callback(app->logger, logger_draw);
    view_set_input_callback(app->logger, logger_input);
    view_set_previous_callback(app->logger, nav_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdLogger, app->logger);

    app->no_module = widget_alloc();
    widget_add_string_element(
        app->no_module, 64, 14, AlignCenter, AlignCenter, FontPrimary, "No module");
    widget_add_text_box_element(
        app->no_module,
        0,
        24,
        128,
        36,
        AlignCenter,
        AlignTop,
        "Attach the Video Game\nModule and restart\nthis app.",
        false);
    view_set_previous_callback(widget_get_view(app->no_module), nav_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdNoModule, widget_get_view(app->no_module));

    app->ui_timer = furi_timer_alloc(ui_timer_cb, FuriTimerTypePeriodic, app);

    return app;
}

static void motion_lab_free(MotionLab* app) {
    furi_timer_stop(app->ui_timer);
    furi_timer_free(app->ui_timer);

    if(app->logging) logger_stop(app);
    motion_sensor_stop(app->sensor);

    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMeter);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdLogger);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdNoModule);

    view_free(app->home);
    view_free(app->meter);
    view_free(app->logger);
    widget_free(app->no_module);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    motion_sensor_free(app->sensor);
    capture_free(app->capture);
    furi_mutex_free(app->lock);
    free(app);
}

int32_t motion_lab_app(void* p) {
    UNUSED(p);
    MotionLab* app = motion_lab_alloc();

    if(motion_sensor_present(app->sensor)) {
        // Calibration happens on the sensor thread the moment it starts, which
        // is why both the menu and this app's docs say to hold still at launch.
        motion_sensor_start(app->sensor, on_sample, app);
        furi_timer_start(app->ui_timer, furi_ms_to_ticks(UI_PERIOD_MS));
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
        app->active = ViewIdMenu;
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdNoModule);
        app->active = ViewIdNoModule;
    }

    view_dispatcher_run(app->view_dispatcher);
    motion_lab_free(app);
    return 0;
}
