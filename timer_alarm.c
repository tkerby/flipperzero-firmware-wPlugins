#include "timer_alarm.h"

void stop_alarm_sound(PomodoroApp* app) {
    if(app->alarm_sound_active) {
        // フラグをクリアしてスピーカー停止
        app->alarm_sound_active = false;
        furi_hal_speaker_stop();
        furi_delay_ms(10);
        if(furi_hal_speaker_is_mine()){
            furi_hal_speaker_release();
        }
    }
}

void start_screen_blink(PomodoroApp* app, uint32_t duration_ms) {
    app->blinking          = true;
    app->blink_start       = furi_get_tick();
    app->blink_duration    = duration_ms;
    app->last_blink_toggle = app->blink_start;
    app->backlight_on      = true;
    
    // LEDは使用しない（ハードウェア未対応）
    
    // ※ここでは「もっと早く」感じるテンポとして、BPM240相当の音符長を使用
    // BPM240の場合：1拍＝250ms, エイトノート＝125ms, クォーターノート＝250ms, ドットハーフ＝750ms
    if(furi_hal_speaker_acquire(100)) {
        app->alarm_sound_active = true;
        const float melody[] = {
            880.0f, 784.0f, 493.88f, 554.37f,
            739.99f, 659.26f, 392.0f, 440.0f,
            659.26f, 587.33f, 369.99f, 440.0f,
            587.33f
        };
        const uint32_t noteDurations[] = {
            125, 125, 250, 250,
            125, 125, 250, 250,
            125, 125, 250, 250,
            750
        };
        int num_notes = sizeof(melody) / sizeof(melody[0]);
        for(int i = 0; i < num_notes; i++){
            if(!app->alarm_sound_active){
                break;
            }
            if(melody[i] == 0.0f) {
                furi_delay_ms(noteDurations[i]);
            } else {
                furi_hal_speaker_start(melody[i], 1.0f);
                furi_delay_ms(noteDurations[i]);
                furi_hal_speaker_stop();
            }
            // ノート間の遅延（ここでは0ms）
            furi_delay_ms(0);
        }
        if(furi_hal_speaker_is_mine()){
            furi_hal_speaker_release();
        }
        app->alarm_sound_active = false;
    }
    
    NotificationApp* notif = furi_record_open("notification");
    notification_message(notif, &sequence_display_backlight_on);
    furi_record_close("notification");
}

void toggle_backlight_with_vibro(PomodoroApp* app) {
    NotificationApp* notif = furi_record_open("notification");
    if(app->backlight_on) {
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

void show_dialog(PomodoroApp* app, const char* title, const char* message, uint32_t timeout_ms, AlarmType alarm_type) {
    app->dialog_active  = true;
    app->dialog_result  = false;
    app->dialog_timeout = timeout_ms;
    app->dialog_start   = furi_get_tick();
    strncpy(app->dialog_title, title, sizeof(app->dialog_title) - 1);
    strncpy(app->dialog_message, message, sizeof(app->dialog_message) - 1);
    app->current_alarm_type = alarm_type;
}
