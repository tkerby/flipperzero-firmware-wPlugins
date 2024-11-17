#include "notifications.h"

static const NotificationMessage* nm_list[16];

void notify_table_bump(PinballApp* app) {
    int n = 0;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_on;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_255;
    }
    nm_list[n++] = &message_delay_100;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_off;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_0;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_error_message(PinballApp* app) {
    int n = 0;
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_c6;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_sound_off;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c5;
        nm_list[n++] = &message_delay_250;
        nm_list[n++] = &message_sound_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}
