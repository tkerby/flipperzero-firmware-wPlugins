#include "timer_logic.h"
#include "timer_alarm.h"  // アラーム関連の関数を使用するため

// 日毎の記録データ保存
void save_daily_records(PomodoroApp* app) {
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

// 日毎の記録データ読み込み
void load_daily_records(PomodoroApp* app) {
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

// 日毎の記録更新
void update_daily_record(PomodoroApp* app, time_t now) {
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

// アプリ全体の状態更新ロジック
void update_logic(PomodoroApp* app) {
    time_t now = furi_hal_rtc_get_timestamp();
    update_daily_record(app, now);
    
    if(app->blinking) {
        uint32_t now_tick = furi_get_tick();
        uint32_t elapsed  = now_tick - app->blink_start;
        if(elapsed >= app->blink_duration) {
            if(!app->backlight_on) {
                toggle_backlight_with_vibro(app);
            }
            app->blinking = false;
        } else {
            if((now_tick - app->last_blink_toggle) >= 500) {
                toggle_backlight_with_vibro(app);
                app->last_blink_toggle = now_tick;
            }
        }
    }
    
    if(app->dialog_active) {
        uint32_t elapsed = furi_get_tick() - app->dialog_start;
        if(elapsed >= app->dialog_timeout) {
            app->fail_count++;
            app->current_daily_record.fail_count++;
            app->dialog_active = false;
            stop_alarm_sound(app);
            app->blinking = false;
            save_daily_records(app);
        }
        return;
    } else {
        if(app->dialog_result) {
            stop_alarm_sound(app);
            app->blinking = false;
            if(app->current_alarm_type == AlarmTypeStart) {
                app->start_count++;
                app->current_daily_record.start_count++;
                app->state.phase = PomodoroPhaseWaitingRest;
                time_t now = furi_hal_rtc_get_timestamp();
                int hour, minute;
                get_hour_minute(now, &hour, &minute);
                int diff_to_50 = 50 - minute;
                if(diff_to_50 < 0) diff_to_50 += 60;
                app->state.next_rest_time = now + diff_to_50 * 60;
            } else if(app->current_alarm_type == AlarmTypeRest) {
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
    if(app->state.phase == PomodoroPhaseIdle) {
        if(minute == 0 && hour != app->state.last_hour_triggered) {
            start_screen_blink(app, 3000);
            show_dialog(app, "Start Alarm", "Time to start?\nPress UP to confirm.", 10 * 1000, AlarmTypeStart);
            app->state.last_hour_triggered = hour;
            app->hourly_alarm_active = true;
        }
    } else if(app->state.phase == PomodoroPhaseWaitingRest) {
        if(app->state.next_rest_time != 0 && now >= app->state.next_rest_time) {
            start_screen_blink(app, 3000);
            show_dialog(app, "Rest Alarm", "Time to rest?\nPress UP to confirm.", 10 * 1000, AlarmTypeRest);
            app->state.phase = PomodoroPhaseIdle;
            app->state.next_rest_time = 0;
        }
    }
}

