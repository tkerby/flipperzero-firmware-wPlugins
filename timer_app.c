#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <furi_hal_speaker.h>  // スピーカー機能用
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h> // バイブ用
#include <stdlib.h>   // malloc, free
#include <string.h>   // memset, strncpy, snprintf
#include <time.h>     // time_t
#include <math.h>     // cos, sin, sqrt, round
#include <storage/storage.h>  // ファイル入出力用API

// -----------------------------------------------------------------------------
// ユーティリティ: epoch から「時」と「分」を取得
// -----------------------------------------------------------------------------
static inline void get_hour_minute(time_t epoch, int* hour, int* minute) {
    *minute = (epoch / 60) % 60;
    *hour   = (epoch / 3600) % 24;
}

// -----------------------------------------------------------------------------
// ポモドーロの状態
// -----------------------------------------------------------------------------
typedef enum {
    PomodoroPhaseIdle = 0,       // 次の XX:00 で開始アラーム
    PomodoroPhaseWaitingRest,    // 次の XX:50 で休憩アラーム
} PomodoroPhase;

// -----------------------------------------------------------------------------
// アラーム種類
// -----------------------------------------------------------------------------
typedef enum {
    AlarmTypeNone = 0,
    AlarmTypeStart,
    AlarmTypeRest,
} AlarmType;

// -----------------------------------------------------------------------------
// 日毎の実績記録用構造体
// -----------------------------------------------------------------------------
typedef struct {
    int year;
    int month;
    int day;
    int start_count;
    int rest_count;
    int fail_count;
} DailyRecord;

// -----------------------------------------------------------------------------
// ポモドーロ状態構造体
// -----------------------------------------------------------------------------
typedef struct {
    PomodoroPhase phase;
    time_t next_rest_time;
    int last_hour_triggered;   // 最後にアラームを出した「時」(0-23)、-1なら未設定
} PomodoroState;

// -----------------------------------------------------------------------------
// アプリ全体管理構造体
// -----------------------------------------------------------------------------
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    bool running;
    PomodoroState state;

    // ダイアログ関連
    bool dialog_active;
    bool dialog_result;
    uint32_t dialog_timeout;
    uint32_t dialog_start;
    char dialog_title[32];
    char dialog_message[64];
    AlarmType current_alarm_type; // 現在開いているアラーム(開始/休憩)

    // バックライト（LED）は使用せず、バックライト関連はダミー
    bool blinking;
    uint32_t blink_start;
    uint32_t blink_duration;
    uint32_t last_blink_toggle;
    bool backlight_on;

    // 毎時アラームのフラグ
    bool hourly_alarm_active;

    // カウンタ（表示には使用しない）
    int start_count;
    int rest_count;
    int fail_count;

    // 日毎の実績記録用フィールド
    bool record_view_active;
    DailyRecord current_daily_record;
    DailyRecord daily_records[30];
    int daily_record_count;

    // 【追加】アラーム音が再生中かどうかのフラグ
    bool alarm_sound_active;
} PomodoroApp;

// -----------------------------------------------------------------------------
// ファイルパス定義
// -----------------------------------------------------------------------------
#define RECORD_FILE APP_DATA_PATH("pomodoro_records.dat")

// -----------------------------------------------------------------------------
// 日毎の記録データ保存関数
// -----------------------------------------------------------------------------
static void save_daily_records(PomodoroApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RECORD_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->daily_record_count, sizeof(app->daily_record_count));
        storage_file_write(file, app->daily_records, sizeof(app->daily_records));
        storage_file_write(file, &app->current_daily_record, sizeof(app->current_daily_record));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// -----------------------------------------------------------------------------
// 日毎の記録データ読み込み関数
// -----------------------------------------------------------------------------
static void load_daily_records(PomodoroApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RECORD_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &app->daily_record_count, sizeof(app->daily_record_count));
        storage_file_read(file, app->daily_records, sizeof(app->daily_records));
        storage_file_read(file, &app->current_daily_record, sizeof(app->current_daily_record));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// -----------------------------------------------------------------------------
// 時刻ユーティリティ (UTC)
// -----------------------------------------------------------------------------
static const char* WDAY_NAME[7] = {"S", "M", "T", "W", "T", "F", "S"};

static void epoch_to_utc_breakdown(
    time_t epoch,
    int* year, int* mon, int* day,
    int* wday,
    int* hour, int* min, int* sec
) {
    *sec = epoch % 60;
    epoch /= 60;
    *min = epoch % 60;
    epoch /= 60;
    *hour = epoch % 24;
    epoch /= 24;
    int days_since_1970 = (int)epoch;
    *wday = (days_since_1970 + 4) % 7;
    int y = 1970;
    while (true) {
        int y_len = ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365);
        if (days_since_1970 >= y_len) {
            days_since_1970 -= y_len;
            y++;
        } else {
            break;
        }
    }
    *year = y;
    static const int MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m;
    for (m = 0; m < 12; m++) {
        int dmax = MDAYS[m];
        if (m == 1 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) {
            dmax = 29;
        }
        if (days_since_1970 >= dmax) {
            days_since_1970 -= dmax;
        } else {
            break;
        }
    }
    *mon = m + 1;
    *day = days_since_1970 + 1;
}

// -----------------------------------------------------------------------------
// 日毎の記録更新
// -----------------------------------------------------------------------------
static void update_daily_record(PomodoroApp* app, time_t now) {
    int year, mon, day, dummy, dummy2, dummy3, dummy4;
    epoch_to_utc_breakdown(now, &year, &mon, &day, &dummy, &dummy2, &dummy3, &dummy4);
    if(year != app->current_daily_record.year ||
       mon != app->current_daily_record.month ||
       day != app->current_daily_record.day) {
        if(app->daily_record_count < 30) {
            app->daily_records[app->daily_record_count++] = app->current_daily_record;
        }
        app->current_daily_record.year = year;
        app->current_daily_record.month = mon;
        app->current_daily_record.day = day;
        app->current_daily_record.start_count = 0;
        app->current_daily_record.rest_count = 0;
        app->current_daily_record.fail_count = 0;
        save_daily_records(app);
    }
}

// -----------------------------------------------------------------------------
// 【修正】アラーム音停止用関数（自動停止時のクラッシュ防止）
// -----------------------------------------------------------------------------
static void stop_alarm_sound(PomodoroApp* app) {
    if(app->alarm_sound_active) {
        // まずフラグをクリア
        app->alarm_sound_active = false;
        furi_hal_speaker_stop();
        furi_delay_ms(10);
        if(furi_hal_speaker_is_mine()) {
            furi_hal_speaker_release();
        }
    }
}

// -----------------------------------------------------------------------------
// バックライトのチカチカ＆バイブ制御（LED呼び出しは削除）
// -----------------------------------------------------------------------------
static void start_screen_blink(PomodoroApp* app, uint32_t duration_ms) {
    app->blinking          = true;
    app->blink_start       = furi_get_tick();
    app->blink_duration    = duration_ms;
    app->last_blink_toggle = app->blink_start;
    app->backlight_on      = true;
    
    // LEDは使用しない（ハードウェア未対応）

    // スピーカー再生：再生前にフラグを true に設定し、各ノート再生前に停止要求をチェック
    if(furi_hal_speaker_acquire(100)) {
        app->alarm_sound_active = true;  // 再生中とする
        // Nokia Tune 楽譜に基づくメロディ
        // Measure 1: A5 (880Hz) eighth, G5 (784Hz) eighth, B4 (493.88Hz) quarter, C#5 (554.37Hz) quarter
        // Measure 2: F#5 (739.99Hz) eighth, E5 (659.26Hz) eighth, G4 (392Hz) quarter, A4 (440Hz) quarter
        // Measure 3: E5 (659.26Hz) eighth, D5 (587.33Hz) eighth, F#4 (369.99Hz) quarter, A4 (440Hz) quarter
        // Measure 4: D5 (587.33Hz) dotted half note
        const float melody[] = {
            880.0f, 784.0f, 493.88f, 554.37f,
            739.99f, 659.26f, 392.0f, 440.0f,
            659.26f, 587.33f, 369.99f, 440.0f,
            587.33f
        };
        const uint32_t noteDurations[] = {
            150, 150, 300, 300,   // Measure 1
            150, 150, 300, 300,   // Measure 2
            150, 150, 300, 300,   // Measure 3
            900                  // Measure 4 (dotted half)
        };
        int num_notes = sizeof(melody) / sizeof(melody[0]);
        for(int i = 0; i < num_notes; i++){
            // 再生中に停止要求が入った場合、ループを抜ける
            if(!app->alarm_sound_active) {
                break;
            }
            if(melody[i] == 0.0f) {
                // 休符
                furi_delay_ms(noteDurations[i]);
            } else {
                furi_hal_speaker_start(melody[i], 1.0f);
                furi_delay_ms(noteDurations[i]);
                furi_hal_speaker_stop();
            }
            // 各ノート間に25msの隙間
            furi_delay_ms(25);
        }
        // 再生終了後、もしまだスピーカーの所有権があるなら解放
        if(furi_hal_speaker_is_mine()) {
            furi_hal_speaker_release();
        }
        app->alarm_sound_active = false;
    }
    
    NotificationApp* notif = furi_record_open("notification");
    notification_message(notif, &sequence_display_backlight_on);
    furi_record_close("notification");
}

static void toggle_backlight_with_vibro(PomodoroApp* app) {
    NotificationApp* notif = furi_record_open("notification");
    if (app->backlight_on) {
        notification_message(notif, &sequence_display_backlight_off);
        app->backlight_on = false;
    } else {
        notification_message(notif, &sequence_display_backlight_on);
        app->backlight_on = true;
    }
    notification_message(notif, &sequence_single_vibro);
    furi_delay_ms(50);
    notification_message(notif, &sequence_single_vibro);
    furi_record_close("notification");
}

static void show_dialog(
    PomodoroApp* app,
    const char* title,
    const char* message,
    uint32_t timeout_ms,
    AlarmType alarm_type
) {
    app->dialog_active  = true;
    app->dialog_result  = false;
    app->dialog_timeout = timeout_ms;
    app->dialog_start   = furi_get_tick();
    strncpy(app->dialog_title, title, sizeof(app->dialog_title) - 1);
    strncpy(app->dialog_message, message, sizeof(app->dialog_message) - 1);
    app->current_alarm_type = alarm_type;
}

static void input_callback(InputEvent* event, void* ctx) {
    PomodoroApp* app = (PomodoroApp*)ctx;
    if (event->type == InputTypePress) {
        switch (event->key) {
            case InputKeyBack:
                stop_alarm_sound(app);
                app->running = false;
                break;
            case InputKeyUp:
                if (app->dialog_active) {
                    app->dialog_result = true;
                    app->dialog_active = false;
                    stop_alarm_sound(app);
                    app->blinking = false;
                }
                break;
            case InputKeyRight:
                if (app->state.phase == PomodoroPhaseIdle) {
                    start_screen_blink(app, 3000);
                    show_dialog(
                        app,
                        "Start Alarm (Test)",
                        "Simulate Start.\nPress UP to confirm.",
                        10 * 1000,
                        AlarmTypeStart
                    );
                    app->hourly_alarm_active = true;
                } else if (app->state.phase == PomodoroPhaseWaitingRest) {
                    start_screen_blink(app, 3000);
                    show_dialog(
                        app,
                        "Rest Alarm (Test)",
                        "Simulate Rest.\nPress UP to confirm.",
                        10 * 1000,
                        AlarmTypeRest
                    );
                    app->state.phase = PomodoroPhaseIdle;
                    app->state.next_rest_time = 0;
                }
                break;
            case InputKeyLeft:
                if (!app->dialog_active) {
                    app->record_view_active = !app->record_view_active;
                }
                break;
            default:
                break;
        }
    }
}

static void update_logic(PomodoroApp* app) {
    time_t now = furi_hal_rtc_get_timestamp();
    update_daily_record(app, now);
    if (app->blinking) {
        uint32_t now_tick = furi_get_tick();
        uint32_t elapsed  = now_tick - app->blink_start;
        if (elapsed >= app->blink_duration) {
            if (!app->backlight_on) {
                toggle_backlight_with_vibro(app);
            }
            app->blinking = false;
        } else {
            if ((now_tick - app->last_blink_toggle) >= 500) {
                toggle_backlight_with_vibro(app);
                app->last_blink_toggle = now_tick;
            }
        }
    }
    if (app->dialog_active) {
        uint32_t elapsed = furi_get_tick() - app->dialog_start;
        if (elapsed >= app->dialog_timeout) {
            app->fail_count++;
            app->current_daily_record.fail_count++;
            app->dialog_active = false;
            stop_alarm_sound(app);
            app->blinking = false;
            save_daily_records(app);
        }
        return;
    } else {
        if (app->dialog_result) {
            stop_alarm_sound(app);
            app->blinking = false;
            if (app->current_alarm_type == AlarmTypeStart) {
                app->start_count++;
                app->current_daily_record.start_count++;
                app->state.phase = PomodoroPhaseWaitingRest;
                time_t now = furi_hal_rtc_get_timestamp();
                int hour, minute;
                get_hour_minute(now, &hour, &minute);
                int diff_to_50 = 50 - minute;
                if (diff_to_50 < 0) diff_to_50 += 60;
                app->state.next_rest_time = now + diff_to_50 * 60;
            } else if (app->current_alarm_type == AlarmTypeRest) {
                app->rest_count++;
                app->current_daily_record.rest_count++;
                app->state.phase = PomodoroPhaseIdle;
                app->state.next_rest_time = 0;
            }
            app->dialog_result = false;
            app->current_alarm_type = AlarmTypeNone;
            save_daily_records(app);
        }
    }
    int hour, minute;
    get_hour_minute(now, &hour, &minute);
    if (app->state.phase == PomodoroPhaseIdle) {
        if (minute == 0 && hour != app->state.last_hour_triggered) {
            start_screen_blink(app, 3000);
            show_dialog(
                app,
                "Start Alarm",
                "Time to start?\nPress UP to confirm.",
                10 * 1000,
                AlarmTypeStart
            );
            app->state.last_hour_triggered = hour;
            app->hourly_alarm_active = true;
        }
    } else if (app->state.phase == PomodoroPhaseWaitingRest) {
        if (app->state.next_rest_time != 0 && now >= app->state.next_rest_time) {
            start_screen_blink(app, 3000);
            show_dialog(
                app,
                "Rest Alarm",
                "Time to rest?\nPress UP to confirm.",
                10 * 1000,
                AlarmTypeRest
            );
            app->state.phase = PomodoroPhaseIdle;
            app->state.next_rest_time = 0;
        }
    }
}

static void draw_thick_line(Canvas* canvas, int x0, int y0, int x1, int y1, int thickness) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    float length = sqrt(dx * dx + dy * dy);
    if (length == 0) return;
    float ux = (float)dy / length;
    float uy = -(float)dx / length;
    int half = thickness / 2;
    for (int i = -half; i <= half; i++) {
        int offset_x = (int)round(i * ux);
        int offset_y = (int)round(i * uy);
        canvas_draw_line(canvas, x0 + offset_x, y0 + offset_y, x1 + offset_x, y1 + offset_y);
    }
}

static void draw_small_analog_clock(Canvas* canvas, time_t now) {
    int center_x = 15, center_y = 15, radius = 15;
    canvas_draw_circle(canvas, center_x, center_y, radius);
    for (int i = 0; i < 12; i++) {
        float tick_angle = i * (2 * M_PI / 12) - (M_PI / 2);
        int tick_inner = radius - 2, tick_outer = radius;
        int x_start = center_x + (int)(tick_inner * cos(tick_angle));
        int y_start = center_y + (int)(tick_inner * sin(tick_angle));
        int x_end   = center_x + (int)(tick_outer * cos(tick_angle));
        int y_end   = center_y + (int)(tick_outer * sin(tick_angle));
        canvas_draw_line(canvas, x_start, y_start, x_end, y_end);
    }
    int year, mon, day, wday, hour, minute, second;
    epoch_to_utc_breakdown(now, &year, &mon, &day, &wday, &hour, &minute, &second);
    int hour_mod = hour % 12;
    float hour_angle = ((hour_mod + minute / 60.0f) / 12.0f) * 2 * M_PI - (M_PI / 2);
    float minute_angle = ((minute + second / 60.0f) / 60.0f) * 2 * M_PI - (M_PI / 2);
    int hour_hand_length = radius - 4, minute_hand_length = radius - 2;
    int hour_x_end = center_x + (int)(hour_hand_length * cos(hour_angle));
    int hour_y_end = center_y + (int)(hour_hand_length * sin(hour_angle));
    int minute_x_end = center_x + (int)(minute_hand_length * cos(minute_angle));
    int minute_y_end = center_y + (int)(minute_hand_length * sin(minute_angle));
    draw_thick_line(canvas, center_x, center_y, hour_x_end, hour_y_end, 2);
    draw_thick_line(canvas, center_x, center_y, minute_x_end, minute_y_end, 2);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    PomodoroApp* app = (PomodoroApp*)ctx;
    if(app->record_view_active) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Daily Record");
        int y_pos = 20;
        for(int i = 0; i < app->daily_record_count; i++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d: S:%d R:%d F:%d",
                     app->daily_records[i].year,
                     app->daily_records[i].month,
                     app->daily_records[i].day,
                     app->daily_records[i].start_count,
                     app->daily_records[i].rest_count,
                     app->daily_records[i].fail_count);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignTop, buf);
            y_pos += 10;
            if(y_pos > 55) break;
        }
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d: S:%d R:%d F:%d",
                     app->current_daily_record.year,
                     app->current_daily_record.month,
                     app->current_daily_record.day,
                     app->current_daily_record.start_count,
                     app->current_daily_record.rest_count,
                     app->current_daily_record.fail_count);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignTop, buf);
        }
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[Left] Exit");
        return;
    }
    canvas_clear(canvas);
    time_t now = furi_hal_rtc_get_timestamp();
    draw_small_analog_clock(canvas, now);
    {
        int y, mo, d, wd, hh, mm, ss;
        epoch_to_utc_breakdown(now, &y, &mo, &d, &wd, &hh, &mm, &ss);
        char date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %s", y, mo, d, WDAY_NAME[wd]);
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", hh, mm, ss);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, date_buf);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, time_buf);
    }
    if (app->dialog_active) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, app->dialog_title);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, app->dialog_message);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "[OK:Up]");
    } else {
        int hour, minute;
        get_hour_minute(now, &hour, &minute);
        canvas_set_font(canvas, FontSecondary);
        if (app->state.phase == PomodoroPhaseIdle) {
            int diff = (60 - minute) % 60;
            char buf[32];
            snprintf(buf, sizeof(buf), "Start in %d min", diff);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, buf);
        } else {
            int diff = 50 - minute;
            if (diff < 0) diff += 60;
            char buf[32];
            snprintf(buf, sizeof(buf), "Rest in %d min", diff);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, buf);
        }
        canvas_draw_str_aligned(canvas, 128, 1, AlignRight, AlignTop, "[Right]:Test");
    }
}

int32_t pomodoro_app_main(void* p) {
    UNUSED(p);
    PomodoroApp* app = malloc(sizeof(PomodoroApp));
    memset(app, 0, sizeof(PomodoroApp));
    app->record_view_active = false;
    app->daily_record_count = 0;
    app->alarm_sound_active = false;
    {
        load_daily_records(app);
        time_t now = furi_hal_rtc_get_timestamp();
        int year, mon, day, dummy, dummy2, dummy3, dummy4;
        epoch_to_utc_breakdown(now, &year, &mon, &day, &dummy, &dummy2, &dummy3, &dummy4);
        if(app->current_daily_record.year != year ||
           app->current_daily_record.month != mon ||
           app->current_daily_record.day != day) {
            app->current_daily_record.year = year;
            app->current_daily_record.month = mon;
            app->current_daily_record.day = day;
            app->current_daily_record.start_count = 0;
            app->current_daily_record.rest_count = 0;
            app->current_daily_record.fail_count = 0;
        }
    }
    app->gui = furi_record_open("gui");
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    time_t now = furi_hal_rtc_get_timestamp();
    app->running = true;
    app->state.phase = PomodoroPhaseWaitingRest;
    int hour, minute;
    get_hour_minute(now, &hour, &minute);
    int diff_to_50 = 50 - minute;
    if (diff_to_50 < 0) diff_to_50 += 60;
    app->state.next_rest_time = now + diff_to_50 * 60;
    app->state.last_hour_triggered = -1;
    app->blinking = false;
    app->hourly_alarm_active = false;
    app->start_count = 0;
    app->rest_count  = 0;
    app->fail_count  = 0;
    while (app->running) {
        update_logic(app);
        view_port_update(app->view_port);
        furi_delay_ms(200);
    }
    save_daily_records(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close("gui");
    free(app);
    return 0;
}
