#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <datetime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_DIR   "/ext/apps_data/coach_watch"
#define MAX_SPLITS 40
#define MAX_FILES  24
#define NAME_LEN   32

typedef enum {
    PageTimer,
    PageHistory,
    PageDetail,
} Page;

typedef enum {
    Idle,
    Running,
    Stopped,
} RunState;

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notify;
    Storage* storage;
    Page page;
    RunState run;
    bool app_running;
    uint32_t t_start;
    uint32_t t_acc; /* elapsed when paused */
    uint32_t splits[MAX_SPLITS];
    uint8_t split_n;
    bool unsaved;
    char files[MAX_FILES][NAME_LEN];
    uint8_t file_n;
    int16_t hist_sel;
    int16_t hist_top;
    char detail[6][28];
    uint8_t detail_n;
} CoachWatch;

static void beep(CoachWatch* app) {
    notification_message(app->notify, &sequence_single_vibro);
}

static uint32_t elapsed_ms(const CoachWatch* app) {
    if(app->run == Running) {
        return app->t_acc + (furi_get_tick() - app->t_start);
    }
    return app->t_acc;
}

static void fmt_ms(uint32_t ms, char* buf, size_t n) {
    uint32_t cs = (ms / 10) % 100;
    uint32_t s = (ms / 1000) % 60;
    uint32_t m = (ms / 60000) % 60;
    uint32_t h = ms / 3600000UL;
    if(h > 0) {
        snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        snprintf(
            buf, n, "%02lu:%02lu.%02lu", (unsigned long)m, (unsigned long)s, (unsigned long)cs);
    }
}

static void ensure_dir(CoachWatch* app) {
    storage_simply_mkdir(app->storage, "/ext/apps_data");
    storage_simply_mkdir(app->storage, SAVE_DIR);
}

static bool write_session(CoachWatch* app) {
    uint32_t total = elapsed_ms(app);
    if(total < 50) {
        return false;
    }
    ensure_dir(app);
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char path[96];
    snprintf(
        path,
        sizeof(path),
        SAVE_DIR "/%04u%02u%02u_%02u%02u%02u.txt",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        char line[80];
        int len = snprintf(
            line,
            sizeof(line),
            "%04u-%02u-%02u %02u:%02u:%02u\n",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second);
        storage_file_write(f, line, len);
        char tot[16];
        fmt_ms(total, tot, sizeof(tot));
        len = snprintf(line, sizeof(line), "total %s\nsplits %u\n", tot, (unsigned)app->split_n);
        storage_file_write(f, line, len);
        uint32_t prev = 0;
        for(uint8_t i = 0; i < app->split_n; i++) {
            char cum[16];
            char lap[16];
            fmt_ms(app->splits[i], cum, sizeof(cum));
            fmt_ms(app->splits[i] - prev, lap, sizeof(lap));
            len = snprintf(line, sizeof(line), "%u %s  lap %s\n", (unsigned)(i + 1), cum, lap);
            storage_file_write(f, line, len);
            prev = app->splits[i];
        }
        if(app->split_n == 0) {
            len = snprintf(line, sizeof(line), "(no splits)\n");
            storage_file_write(f, line, len);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    if(ok) {
        app->unsaved = false;
    }
    return ok;
}

static void reset_timer(CoachWatch* app) {
    app->run = Idle;
    app->t_start = 0;
    app->t_acc = 0;
    app->split_n = 0;
    app->unsaved = false;
}

static void save_and_exit(CoachWatch* app) {
    /* Running sessions never set unsaved until Stop; still persist them. */
    if(app->unsaved || app->run == Running) {
        write_session(app);
    }
    app->app_running = false;
}

static void load_history(CoachWatch* app) {
    app->file_n = 0;
    ensure_dir(app);
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, SAVE_DIR)) {
        FileInfo info;
        char name[NAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(info.flags & FSF_DIRECTORY) continue;
            if(app->file_n >= MAX_FILES) break;
            strncpy(app->files[app->file_n], name, NAME_LEN - 1);
            app->files[app->file_n][NAME_LEN - 1] = '\0';
            app->file_n++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    app->hist_sel = 0;
    app->hist_top = 0;
}

static void load_detail(CoachWatch* app, const char* name) {
    app->detail_n = 0;
    char path[96];
    snprintf(path, sizeof(path), SAVE_DIR "/%s", name);
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        uint16_t n = storage_file_read(f, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        storage_file_close(f);
        uint16_t start = 0;
        for(uint16_t i = 0; i <= n && app->detail_n < 6; i++) {
            if(buf[i] == '\n' || buf[i] == '\0') {
                uint16_t len = i - start;
                if(len > 27) len = 27;
                memcpy(app->detail[app->detail_n], buf + start, len);
                app->detail[app->detail_n][len] = '\0';
                app->detail_n++;
                start = i + 1;
            }
        }
    }
    storage_file_free(f);
}

static void draw_time_big(Canvas* canvas, uint32_t ms) {
    uint32_t cs = (ms / 10) % 100;
    uint32_t s = (ms / 1000) % 60;
    uint32_t m = (ms / 60000) % 60;
    uint32_t h = ms / 3600000UL;
    char a[8];
    char b[8];
    char c[8];
    if(h > 0) {
        snprintf(a, sizeof(a), "%lu", (unsigned long)h);
        snprintf(b, sizeof(b), "%02lu", (unsigned long)m);
        snprintf(c, sizeof(c), "%02lu", (unsigned long)s);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 4, 32, a);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 30, ":");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 36, 32, b);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 72, 30, ":");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 80, 32, c);
    } else {
        snprintf(a, sizeof(a), "%02lu", (unsigned long)m);
        snprintf(b, sizeof(b), "%02lu", (unsigned long)s);
        snprintf(c, sizeof(c), "%02lu", (unsigned long)cs);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 8, 32, a);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 44, 30, ":");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 52, 32, b);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 88, 30, ".");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 96, 32, c);
    }
}

static void draw_timer(Canvas* canvas, CoachWatch* app) {
    const char* st = "STOP";
    if(app->run == Running)
        st = "RUN";
    else if(app->run == Stopped)
        st = "PAUSE";
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, "Coach Watch");
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, st);

    draw_time_big(canvas, elapsed_ms(app));

    canvas_set_font(canvas, FontSecondary);
    char sub[40];
    if(app->split_n == 0) {
        snprintf(sub, sizeof(sub), "0 split");
    } else {
        char lap[16];
        uint32_t prev = (app->split_n > 1) ? app->splits[app->split_n - 2] : 0;
        fmt_ms(app->splits[app->split_n - 1] - prev, lap, sizeof(lap));
        snprintf(sub, sizeof(sub), "S%u lap %s", (unsigned)app->split_n, lap);
    }
    canvas_draw_str(canvas, 2, 44, sub);
    if(app->unsaved) {
        canvas_draw_str_aligned(canvas, 126, 44, AlignRight, AlignBottom, "*");
    }

    if(app->run == Idle) {
        elements_button_center(canvas, "Start");
        elements_button_left(canvas, "Exit");
        elements_button_right(canvas, "Log");
    } else if(app->run == Running) {
        elements_button_center(canvas, "Stop");
        elements_button_left(canvas, "Exit");
        elements_button_right(canvas, "Split");
    } else {
        elements_button_center(canvas, "Reset");
        elements_button_left(canvas, "Exit");
        elements_button_right(canvas, "Save");
    }
}

static void draw_history(Canvas* canvas, CoachWatch* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Sessions");
    canvas_set_font(canvas, FontSecondary);
    if(app->file_n == 0) {
        canvas_draw_str(canvas, 8, 32, "No files");
        elements_button_left(canvas, "Back");
        return;
    }
    for(int16_t i = 0; i < 5; i++) {
        int16_t idx = app->hist_top + i;
        if(idx >= app->file_n) break;
        int16_t y = 14 + i * 8;
        if(idx == app->hist_sel) {
            canvas_draw_box(canvas, 0, y - 1, 128, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 2, y + 7, app->files[idx]);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_button_center(canvas, "Open");
    elements_button_left(canvas, "Back");
}

static void draw_detail(Canvas* canvas, CoachWatch* app) {
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < app->detail_n; i++) {
        canvas_draw_str(canvas, 2, 10 + i * 9, app->detail[i]);
    }
    elements_button_left(canvas, "Back");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    CoachWatch* app = ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(app->page == PageTimer) {
        draw_timer(canvas, app);
    } else if(app->page == PageHistory) {
        draw_history(canvas, app);
    } else {
        draw_detail(canvas, app);
    }
}

static void input_cb(InputEvent* event, void* ctx) {
    CoachWatch* app = ctx;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

static void open_history(CoachWatch* app) {
    load_history(app);
    app->page = PageHistory;
}

static void handle_timer(CoachWatch* app, InputKey key) {
    if(key == InputKeyOk) {
        if(app->run == Idle) {
            app->run = Running;
            app->t_start = furi_get_tick();
            beep(app);
        } else if(app->run == Running) {
            app->t_acc += furi_get_tick() - app->t_start;
            app->run = Stopped;
            app->unsaved = (app->t_acc >= 50);
            beep(app);
        } else {
            if(app->unsaved) {
                write_session(app);
            }
            reset_timer(app);
            beep(app);
        }
    } else if(key == InputKeyRight) {
        if(app->run == Running && app->split_n < MAX_SPLITS) {
            app->splits[app->split_n++] = elapsed_ms(app);
            beep(app);
        } else if(app->run == Stopped && app->unsaved) {
            if(write_session(app)) beep(app);
        } else if(app->run == Idle) {
            open_history(app);
        }
    } else if(key == InputKeyUp) {
        open_history(app);
    } else if(key == InputKeyDown) {
        if((app->run == Stopped || app->run == Running) && elapsed_ms(app) >= 50) {
            if(app->run == Running) {
                /* snapshot without stopping */
                uint32_t acc = elapsed_ms(app);
                uint32_t old_acc = app->t_acc;
                RunState old = app->run;
                app->t_acc = acc;
                app->run = Stopped;
                bool ok = write_session(app);
                app->t_acc = old_acc;
                app->run = old;
                app->unsaved = true;
                if(ok) beep(app);
            } else if(write_session(app)) {
                beep(app);
            }
        }
    } else if(key == InputKeyBack || key == InputKeyLeft) {
        save_and_exit(app);
    }
}

static void handle_history(CoachWatch* app, InputKey key) {
    if(key == InputKeyUp && app->hist_sel > 0) {
        app->hist_sel--;
        if(app->hist_sel < app->hist_top) app->hist_top = app->hist_sel;
    } else if(key == InputKeyDown && app->hist_sel + 1 < app->file_n) {
        app->hist_sel++;
        if(app->hist_sel >= app->hist_top + 5) app->hist_top = app->hist_sel - 4;
    } else if(key == InputKeyOk && app->file_n > 0) {
        load_detail(app, app->files[app->hist_sel]);
        app->page = PageDetail;
    } else if(key == InputKeyBack || key == InputKeyLeft) {
        app->page = PageTimer;
    }
}

static void handle_detail(CoachWatch* app, InputKey key) {
    if(key == InputKeyBack || key == InputKeyLeft) {
        app->page = PageHistory;
    }
}

int32_t coach_watch_app(void* p) {
    UNUSED(p);
    CoachWatch* app = malloc(sizeof(CoachWatch));
    memset(app, 0, sizeof(CoachWatch));
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->page = PageTimer;
    app->run = Idle;
    app->app_running = true;

    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    app->notify = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent ev;
    while(app->app_running) {
        uint32_t wait = (app->page == PageTimer && app->run == Running) ? 50 : FuriWaitForever;
        FuriStatus st = furi_message_queue_get(app->queue, &ev, wait);
        if(st == FuriStatusOk) {
            if(ev.key == InputKeyBack && ev.type == InputTypeLong) {
                save_and_exit(app);
            } else if(ev.type == InputTypeShort) {
                if(app->page == PageTimer) {
                    handle_timer(app, ev.key);
                } else if(app->page == PageHistory) {
                    handle_history(app, ev.key);
                } else {
                    handle_detail(app, ev.key);
                }
            } else if(
                ev.type == InputTypeRepeat && app->page == PageHistory &&
                (ev.key == InputKeyUp || ev.key == InputKeyDown)) {
                handle_history(app, ev.key);
            }
        }
        view_port_update(app->view_port);
    }

    if(app->unsaved) {
        write_session(app);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
