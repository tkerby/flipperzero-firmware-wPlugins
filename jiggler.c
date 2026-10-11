// Jiggler: keeps a computer awake by acting as a USB mouse that makes tiny
// net-zero moves at random intervals. With the desktop helper running it
// also knows when you are really using the computer and holds off.

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <stdio.h>
#include <string.h>

#include "usb_hid_jig.h"

#define TICK_MS        100
#define HELPER_LOST_MS 4000
#define BLINK_MS       400

#define SAVE_DIR   EXT_PATH("apps_data/jiggler")
#define SAVE_PATH  SAVE_DIR "/settings.bin"
#define SAVE_MAGIC 0x4A494731 // "JIG1"

#define STOP_AT_OFF 0xFFFF

static const uint16_t wait_table[] = {5, 10, 15, 20, 30, 45, 60, 90, 120, 180, 240, 300, 600};
static const uint16_t resume_table[] = {0, 5, 10, 15, 30, 60, 120, 300};
static const uint16_t run_for_table[] = {0, 15, 30, 60, 120, 240, 480, 600, 720}; // minutes

typedef enum {
    MoveNudge,
    MoveWiggle,
    MoveSquare,
    MoveF15,
    MoveShift,
    MoveCount,
} Move;

static const char* const move_names[MoveCount] =
    {"Nudge", "Wiggle", "Square", "F15 key", "Shift key"};

typedef struct {
    uint32_t magic;
    uint8_t min_idx;
    uint8_t max_idx;
    uint8_t move;
    uint8_t resume_idx;
    uint8_t pause_when_locked;
    uint8_t run_for_idx;
    uint16_t stop_at; // minutes after midnight, or STOP_AT_OFF
} Settings;

typedef enum {
    StateStopped,
    StateJiggling,
    StatePausedBusy,
    StatePausedLocked,
    StateDone,
} State;

typedef enum {
    ScreenMain,
    ScreenSettings,
    ScreenUsbBusy,
} Screen;

typedef enum {
    SetWaitMin,
    SetWaitMax,
    SetMove,
    SetResume,
    SetLocked,
    SetRunFor,
    SetStopAt,
    SetCount,
} SettingItem;

#define SET_VISIBLE 4

typedef struct {
    FuriMutex* mutex;
    Settings settings;
    Screen screen;
    State state;
    uint8_t sel;
    uint8_t scroll;

    uint32_t moves;
    uint32_t start_tick;
    uint32_t next_tick;
    uint32_t last_move_tick;
    uint32_t run_until_tick; // 0 when no run timer
    uint32_t stop_at_ts; // 0 when no stop time
    uint32_t last_rtc_tick;

    JigHostInfo host;
    bool status_due;
    uint8_t frame;
} App;

// ---------------------------------------------------------------- settings

static void settings_default(Settings* s) {
    memset(s, 0, sizeof(*s));
    s->magic = SAVE_MAGIC;
    s->min_idx = 4; // 30 s
    s->max_idx = 8; // 120 s
    s->move = MoveNudge;
    s->resume_idx = 4; // 30 s
    s->pause_when_locked = 0;
    s->run_for_idx = 0;
    s->stop_at = STOP_AT_OFF;
}

static void settings_load(Settings* s) {
    settings_default(s);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Settings tmp;
        if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp) && tmp.magic == SAVE_MAGIC &&
           tmp.min_idx < COUNT_OF(wait_table) && tmp.max_idx < COUNT_OF(wait_table) &&
           tmp.move < MoveCount && tmp.resume_idx < COUNT_OF(resume_table) &&
           tmp.run_for_idx < COUNT_OF(run_for_table) &&
           (tmp.stop_at == STOP_AT_OFF || tmp.stop_at < 24 * 60)) {
            *s = tmp;
            if(s->max_idx < s->min_idx) s->max_idx = s->min_idx;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_save(const Settings* s) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, s, sizeof(*s));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// ---------------------------------------------------------------- helpers

static uint32_t rand_between(uint32_t lo, uint32_t hi) {
    if(hi <= lo) return lo;
    return lo + furi_hal_random_get() % (hi - lo + 1);
}

// Seconds as h:mm:ss, or m:ss under an hour
static void fmt_duration(char* buf, size_t len, uint32_t seconds) {
    uint32_t h = seconds / 3600, m = (seconds / 60) % 60, s = seconds % 60;
    if(h) {
        snprintf(buf, len, "%lu:%02lu:%02lu", h, m, s);
    } else {
        snprintf(buf, len, "%lu:%02lu", m, s);
    }
}

static void fmt_wait(char* buf, size_t len, uint16_t seconds) {
    if(seconds >= 60 && seconds % 60 == 0) {
        snprintf(buf, len, "%u min", seconds / 60);
    } else {
        snprintf(buf, len, "%u s", seconds);
    }
}

static uint32_t rtc_now(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return datetime_datetime_to_timestamp(&dt);
}

static bool helper_present(const App* app, uint32_t now) {
    return app->host.valid && (now - app->host.tick) < HELPER_LOST_MS;
}

// Host idle time extrapolated from the last packet
static uint32_t helper_idle_now(const App* app, uint32_t now) {
    return app->host.idle_seconds + (now - app->host.tick) / 1000;
}

// ---------------------------------------------------------------- jiggling

static void schedule_next(App* app, uint32_t now) {
    uint32_t lo = wait_table[app->settings.min_idx];
    uint32_t hi = wait_table[app->settings.max_idx];
    app->next_tick = now + rand_between(lo, hi) * 1000;
}

static void do_move(App* app, uint32_t now) {
    switch(app->settings.move) {
    case MoveNudge:
        jig_usb_mouse_move(1, 0);
        jig_usb_mouse_move(-1, 0);
        break;
    case MoveWiggle: {
        static const int8_t dirs[8][2] = {
            {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
        uint8_t d = furi_hal_random_get() % 8;
        jig_usb_mouse_move(dirs[d][0], dirs[d][1]);
        jig_usb_mouse_move(-dirs[d][0], -dirs[d][1]);
        break;
    }
    case MoveSquare:
        jig_usb_mouse_move(1, 0);
        jig_usb_mouse_move(0, 1);
        jig_usb_mouse_move(-1, 0);
        jig_usb_mouse_move(0, -1);
        break;
    case MoveF15:
        jig_usb_key_tap(HID_KEYBOARD_F15);
        break;
    case MoveShift:
        jig_usb_key_tap(KEY_MOD_LEFT_SHIFT);
        break;
    default:
        break;
    }
    app->moves++;
    app->last_move_tick = now;
}

static void jig_start(App* app) {
    uint32_t now = furi_get_tick();
    app->state = StateJiggling;
    app->moves = 0;
    app->start_tick = now;
    app->last_move_tick = 0;
    // First move soon, then the random schedule
    app->next_tick = now + 1000;

    uint16_t run_min = run_for_table[app->settings.run_for_idx];
    app->run_until_tick = run_min ? now + (uint32_t)run_min * 60000 : 0;

    app->stop_at_ts = 0;
    if(app->settings.stop_at != STOP_AT_OFF) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        uint32_t today = datetime_datetime_to_timestamp(&dt);
        uint32_t midnight = today - (dt.hour * 3600 + dt.minute * 60 + dt.second);
        uint32_t ts = midnight + (uint32_t)app->settings.stop_at * 60;
        if(ts <= today) ts += 24 * 3600; // already past today: tomorrow
        app->stop_at_ts = ts;
    }
    app->status_due = true;
}

static void jig_stop(App* app, State state) {
    app->state = state;
    app->run_until_tick = 0;
    app->stop_at_ts = 0;
    app->status_due = true;
}

static bool jig_running(const App* app) {
    return app->state == StateJiggling || app->state == StatePausedBusy ||
           app->state == StatePausedLocked;
}

static void jig_tick(App* app) {
    uint32_t now = furi_get_tick();
    app->frame++;

    JigHostInfo info;
    if(jig_usb_take_host_info(&info)) {
        app->host = info;
        app->status_due = true;
    }

    if(jig_running(app)) {
        // Run timers
        if(app->run_until_tick && (int32_t)(now - app->run_until_tick) >= 0) {
            jig_stop(app, StateDone);
        } else if(app->stop_at_ts && (now - app->last_rtc_tick) >= 1000) {
            app->last_rtc_tick = now;
            if(rtc_now() >= app->stop_at_ts) jig_stop(app, StateDone);
        }
    }

    if(jig_running(app)) {
        State want = StateJiggling;
        if(helper_present(app, now)) {
            bool locked = (app->host.flags & JIG_HOST_HAS_LOCK) &&
                          (app->host.flags & JIG_HOST_LOCKED);
            uint16_t grace = resume_table[app->settings.resume_idx];
            if(locked && app->settings.pause_when_locked) {
                want = StatePausedLocked;
            } else if(
                grace && (app->host.flags & JIG_HOST_HAS_IDLE) &&
                helper_idle_now(app, now) < grace) {
                want = StatePausedBusy;
            }
        }
        if(want != app->state) {
            if(want == StateJiggling) {
                // Coming back from a pause: move again shortly
                app->next_tick = now + rand_between(2, 6) * 1000;
            }
            app->state = want;
            app->status_due = true;
        }
        if(app->state == StateJiggling && (int32_t)(now - app->next_tick) >= 0) {
            do_move(app, now);
            schedule_next(app, now);
            app->status_due = true;
        }
    }

    if(app->status_due && jig_usb_is_connected()) {
        uint32_t next = 0;
        if(app->state == StateJiggling && (int32_t)(app->next_tick - now) > 0) {
            next = (app->next_tick - now) / 1000;
        }
        uint32_t since = app->last_move_tick ? now - app->last_move_tick : 0xFFFF;
        if(since > 0xFFFF) since = 0xFFFF;
        jig_usb_send_status(app->state, next > 0xFFFF ? 0xFFFF : next, since, app->moves);
        app->status_due = false;
    }
}

// ---------------------------------------------------------------- drawing

static void draw_cursor(Canvas* canvas, int32_t x, int32_t y) {
    // A classic arrow pointer, 11 wide, 17 tall, tip at (x, y)
    static const uint16_t rows[17] = {
        0x400,
        0x600,
        0x700,
        0x780,
        0x7C0,
        0x7E0,
        0x7F0,
        0x7F8,
        0x7FC,
        0x7FE,
        0x7E0,
        0x770,
        0x670,
        0x438,
        0x038,
        0x01C,
        0x01C,
    };
    for(int32_t r = 0; r < 17; r++) {
        for(int32_t c = 0; c < 11; c++) {
            if(rows[r] & (1 << (10 - c))) canvas_draw_dot(canvas, x + c, y + r);
        }
    }
}

static void draw_header(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, title);
    canvas_set_font(canvas, FontSecondary);
    if(jig_usb_is_connected()) {
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, "USB ok");
    } else {
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, "no USB");
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);
}

static void draw_main(Canvas* canvas, App* app) {
    char buf[40];
    uint32_t now = furi_get_tick();
    draw_header(canvas, "Jiggler");

    // Pointer on the left, twitching while jiggling
    int32_t cx = 14, cy = 22;
    if(app->state == StateJiggling && (now - app->last_move_tick) < BLINK_MS) {
        cx += (app->frame & 1) ? 1 : -1;
    }
    draw_cursor(canvas, cx, cy);
    if(app->state == StatePausedBusy || app->state == StatePausedLocked) {
        canvas_draw_box(canvas, 28, 24, 3, 9);
        canvas_draw_box(canvas, 33, 24, 3, 9);
    } else if(app->state == StateDone) {
        canvas_draw_line(canvas, 27, 29, 30, 32);
        canvas_draw_line(canvas, 30, 32, 36, 25);
    } else if(app->state == StateStopped) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 28, 32, "zz");
    }

    const char* title = "Stopped";
    const char* detail = "OK to start";
    switch(app->state) {
    case StateJiggling: {
        title = "Jiggling";
        uint32_t left = (int32_t)(app->next_tick - now) > 0 ? (app->next_tick - now) / 1000 : 0;
        char t[16];
        fmt_duration(t, sizeof(t), left);
        snprintf(buf, sizeof(buf), "next move in %s", t);
        detail = buf;
        break;
    }
    case StatePausedBusy:
        title = "Paused";
        detail = "you're using the PC";
        break;
    case StatePausedLocked:
        title = "Paused";
        detail = "the PC is locked";
        break;
    case StateDone:
        title = "Finished";
        detail = "the run timer ended";
        break;
    default:
        break;
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 44, 23, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 44, 33, detail);

    // Counters
    char up[16] = "";
    if(jig_running(app) || app->state == StateDone) {
        uint32_t secs = (now - app->start_tick) / 1000;
        fmt_duration(up, sizeof(up), secs);
    }
    if(jig_running(app) && app->run_until_tick) {
        char t[16];
        fmt_duration(t, sizeof(t), (app->run_until_tick - now) / 1000);
        snprintf(buf, sizeof(buf), "%lu moves, %s left", app->moves, t);
    } else if(jig_running(app) && app->stop_at_ts) {
        snprintf(
            buf,
            sizeof(buf),
            "%lu moves, until %02u:%02u",
            app->moves,
            app->settings.stop_at / 60,
            app->settings.stop_at % 60);
    } else if(up[0]) {
        snprintf(buf, sizeof(buf), "%lu move%s in %s", app->moves, app->moves == 1 ? "" : "s", up);
    } else {
        char lo[12], hi[12];
        fmt_wait(lo, sizeof(lo), wait_table[app->settings.min_idx]);
        fmt_wait(hi, sizeof(hi), wait_table[app->settings.max_idx]);
        snprintf(buf, sizeof(buf), "every %s to %s", lo, hi);
    }
    canvas_draw_str(canvas, 44, 43, buf);

    // Helper line
    if(helper_present(app, now)) {
        if((app->host.flags & JIG_HOST_HAS_LOCK) && (app->host.flags & JIG_HOST_LOCKED)) {
            snprintf(buf, sizeof(buf), "Helper: PC locked");
        } else if(app->host.flags & JIG_HOST_HAS_IDLE) {
            char t[16];
            fmt_duration(t, sizeof(t), helper_idle_now(app, now));
            snprintf(buf, sizeof(buf), "Helper: idle %s", t);
        } else {
            snprintf(buf, sizeof(buf), "Helper: no idle info");
        }
    } else {
        snprintf(buf, sizeof(buf), "Helper: not running");
    }
    canvas_draw_str(canvas, 44, 53, buf);

    // Hints
    canvas_draw_str(canvas, 2, 63, jig_running(app) ? "OK: stop" : "OK: start");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "Right: settings");
}

static void setting_value(const App* app, SettingItem item, char* buf, size_t len) {
    const Settings* s = &app->settings;
    switch(item) {
    case SetWaitMin:
        fmt_wait(buf, len, wait_table[s->min_idx]);
        break;
    case SetWaitMax:
        fmt_wait(buf, len, wait_table[s->max_idx]);
        break;
    case SetMove:
        snprintf(buf, len, "%s", move_names[s->move]);
        break;
    case SetResume:
        if(resume_table[s->resume_idx] == 0) {
            snprintf(buf, len, "Off");
        } else {
            fmt_wait(buf, len, resume_table[s->resume_idx]);
        }
        break;
    case SetLocked:
        snprintf(buf, len, "%s", s->pause_when_locked ? "Pause" : "Keep going");
        break;
    case SetRunFor: {
        uint16_t m = run_for_table[s->run_for_idx];
        if(m == 0) {
            snprintf(buf, len, "Off");
        } else if(m % 60 == 0) {
            snprintf(buf, len, "%u h", m / 60);
        } else {
            snprintf(buf, len, "%u min", m);
        }
        break;
    }
    case SetStopAt:
        if(s->stop_at == STOP_AT_OFF) {
            snprintf(buf, len, "Off");
        } else {
            snprintf(buf, len, "%02u:%02u", s->stop_at / 60, s->stop_at % 60);
        }
        break;
    default:
        buf[0] = 0;
    }
}

static const char* const setting_names[SetCount] = {
    "Wait at least",
    "Wait at most",
    "Move",
    "Resume after idle",
    "If PC locked",
    "Run for",
    "Stop at",
};

static void draw_settings(Canvas* canvas, App* app) {
    draw_header(canvas, "Settings");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t row = 0; row < SET_VISIBLE; row++) {
        uint8_t i = app->scroll + row;
        if(i >= SetCount) break;
        int32_t y = 15 + row * 12;
        bool selected = i == app->sel;
        if(selected) {
            canvas_draw_rbox(canvas, 0, y, 124, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 3, y + 9, setting_names[i]);
        char val[20];
        setting_value(app, i, val, sizeof(val));
        int32_t w = canvas_string_width(canvas, val);
        canvas_draw_str(canvas, 114 - w, y + 9, val);
        if(selected) {
            canvas_draw_str(canvas, 117, y + 9, ">");
            canvas_draw_str(canvas, 109 - w, y + 9, "<");
            canvas_set_color(canvas, ColorBlack);
        }
    }
    // Scrollbar
    canvas_draw_line(canvas, 127, 15, 127, 62);
    int32_t thumb_h = 48 * SET_VISIBLE / SetCount;
    int32_t thumb_y = 15 + (48 - thumb_h) * app->scroll / (SetCount - SET_VISIBLE);
    canvas_draw_box(canvas, 126, thumb_y, 2, thumb_h);
}

static void draw_usb_busy(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "USB is in use");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 34, AlignCenter, AlignBottom, "Close qFlipper or the mobile");
    canvas_draw_str_aligned(
        canvas, 64, 44, AlignCenter, AlignBottom, "app, then open Jiggler again.");
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back: quit");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(app->screen) {
    case ScreenMain:
        draw_main(canvas, app);
        break;
    case ScreenSettings:
        draw_settings(canvas, app);
        break;
    case ScreenUsbBusy:
        draw_usb_busy(canvas);
        break;
    }
    furi_mutex_release(app->mutex);
}

// ---------------------------------------------------------------- input

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void adjust_setting(App* app, int8_t dir) {
    Settings* s = &app->settings;
    switch(app->sel) {
    case SetWaitMin:
        if(dir > 0 && s->min_idx + 1 < (int)COUNT_OF(wait_table)) s->min_idx++;
        if(dir < 0 && s->min_idx > 0) s->min_idx--;
        if(s->max_idx < s->min_idx) s->max_idx = s->min_idx;
        break;
    case SetWaitMax:
        if(dir > 0 && s->max_idx + 1 < (int)COUNT_OF(wait_table)) s->max_idx++;
        if(dir < 0 && s->max_idx > 0) s->max_idx--;
        if(s->min_idx > s->max_idx) s->min_idx = s->max_idx;
        break;
    case SetMove:
        s->move = (s->move + MoveCount + dir) % MoveCount;
        break;
    case SetResume:
        if(dir > 0 && s->resume_idx + 1 < (int)COUNT_OF(resume_table)) s->resume_idx++;
        if(dir < 0 && s->resume_idx > 0) s->resume_idx--;
        break;
    case SetLocked:
        s->pause_when_locked = !s->pause_when_locked;
        break;
    case SetRunFor:
        if(dir > 0 && s->run_for_idx + 1 < (int)COUNT_OF(run_for_table)) s->run_for_idx++;
        if(dir < 0 && s->run_for_idx > 0) s->run_for_idx--;
        break;
    case SetStopAt:
        // Off, then every quarter hour around the clock
        if(dir > 0) {
            if(s->stop_at == STOP_AT_OFF) {
                s->stop_at = 0;
            } else if(s->stop_at + 15 >= 24 * 60) {
                s->stop_at = STOP_AT_OFF;
            } else {
                s->stop_at += 15;
            }
        } else {
            if(s->stop_at == STOP_AT_OFF) {
                s->stop_at = 24 * 60 - 15;
            } else if(s->stop_at == 0) {
                s->stop_at = STOP_AT_OFF;
            } else {
                s->stop_at -= 15;
            }
        }
        break;
    default:
        break;
    }
}

// Returns true to quit
static bool handle_input(App* app, const InputEvent* event) {
    bool press = event->type == InputTypeShort;
    bool repeat = event->type == InputTypeRepeat;

    if(app->screen == ScreenUsbBusy) {
        return press && event->key == InputKeyBack;
    }

    if(app->screen == ScreenSettings) {
        if(press && event->key == InputKeyBack) {
            app->screen = ScreenMain;
            settings_save(&app->settings);
            return false;
        }
        if((press || repeat) && event->key == InputKeyUp && app->sel > 0) app->sel--;
        if((press || repeat) && event->key == InputKeyDown && app->sel + 1 < SetCount) app->sel++;
        if((press || repeat) && event->key == InputKeyLeft) adjust_setting(app, -1);
        if((press || repeat) && (event->key == InputKeyRight || event->key == InputKeyOk)) {
            adjust_setting(app, 1);
        }
        if(app->sel < app->scroll) app->scroll = app->sel;
        if(app->sel >= app->scroll + SET_VISIBLE) app->scroll = app->sel - SET_VISIBLE + 1;
        return false;
    }

    // Main screen
    if(press && event->key == InputKeyBack) return true;
    if(press && event->key == InputKeyOk) {
        if(jig_running(app)) {
            jig_stop(app, StateStopped);
        } else {
            jig_start(app);
        }
    }
    if(press && (event->key == InputKeyRight || event->key == InputKeyUp)) {
        app->screen = ScreenSettings;
        app->sel = 0;
        app->scroll = 0;
    }
    return false;
}

// ---------------------------------------------------------------- main

int32_t jiggler_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    settings_load(&app->settings);

    bool usb_ok = !furi_hal_usb_is_locked();
    if(usb_ok) {
        jig_usb_start();
    } else {
        app->screen = ScreenUsbBusy;
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool quit = false;
    while(!quit) {
        FuriStatus status = furi_message_queue_get(queue, &event, furi_ms_to_ticks(TICK_MS));
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(status == FuriStatusOk) {
            quit = handle_input(app, &event);
        }
        if(usb_ok) jig_tick(app);
        furi_mutex_release(app->mutex);
        view_port_update(view_port);
    }

    settings_save(&app->settings);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    if(usb_ok) jig_usb_stop();
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
