#include "tt_alerts.h"
#include "tt_i18n.h"

#include <furi.h>
#include <notification/notification_messages.h>

/* Alle Tabellen aufsteigend: Links = weniger, Rechts = mehr (siehe Header) */

/* Warnung 1: früher Hinweis */
static const uint32_t tt_warn1_table[TT_WARN1_OPTIONS] = {0, 60, 120, 180, 300, 600, 900};
/* Warnung 2: kurz vor Schluss */
static const uint32_t tt_warn2_table[TT_WARN2_OPTIONS] = {0, 30, 60, 120};
/* Erinnerung während der Überziehung: alle n Sekunden */
static const uint32_t tt_overtime_table[TT_OVERTIME_OPTIONS] = {0, 60, 120, 300};

static bool tt_table_find(const uint32_t* table, size_t count, uint32_t seconds, uint8_t* index) {
    for(size_t i = 0; i < count; i++) {
        if(table[i] == seconds) {
            *index = (uint8_t)i;
            return true;
        }
    }
    return false;
}

uint32_t tt_warn1_seconds(uint8_t index) {
    return (index < TT_WARN1_OPTIONS) ? tt_warn1_table[index] : 0;
}

uint32_t tt_warn2_seconds(uint8_t index) {
    return (index < TT_WARN2_OPTIONS) ? tt_warn2_table[index] : 0;
}

uint32_t tt_overtime_seconds(uint8_t index) {
    return (index < TT_OVERTIME_OPTIONS) ? tt_overtime_table[index] : 0;
}

bool tt_warn1_index(uint32_t seconds, uint8_t* index) {
    return tt_table_find(tt_warn1_table, TT_WARN1_OPTIONS, seconds, index);
}

bool tt_warn2_index(uint32_t seconds, uint8_t* index) {
    return tt_table_find(tt_warn2_table, TT_WARN2_OPTIONS, seconds, index);
}

bool tt_overtime_index(uint32_t seconds, uint8_t* index) {
    return tt_table_find(tt_overtime_table, TT_OVERTIME_OPTIONS, seconds, index);
}

void tt_alert_option_text(uint32_t seconds, char* buf, size_t buf_size) {
    if(seconds == 0) {
        snprintf(buf, buf_size, "%s", tt_tr("Off", "Aus"));
    } else if(seconds % 60 == 0) {
        snprintf(buf, buf_size, "%lu min", (unsigned long)(seconds / 60));
    } else {
        snprintf(buf, buf_size, "%lu s", (unsigned long)seconds);
    }
}

void tt_alert_short_text(uint32_t seconds, char* buf, size_t buf_size) {
    if(seconds == 0) {
        snprintf(buf, buf_size, "-");
    } else if(seconds % 60 == 0) {
        snprintf(buf, buf_size, "%lum", (unsigned long)(seconds / 60));
    } else {
        snprintf(buf, buf_size, "%lus", (unsigned long)seconds);
    }
}

/* ---- Vibrationsmuster ----------------------------------------------------- */

/* Warnung 1: 1x lang (500 ms) */
static const NotificationSequence tt_sequence_warn1 = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_delay_500,
    &message_vibro_off,
    NULL,
};

/* Warnung 2: 2x lang */
static const NotificationSequence tt_sequence_warn2 = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_delay_500,
    &message_vibro_off,
    &message_delay_250,
    &message_vibro_on,
    &message_delay_500,
    &message_vibro_off,
    NULL,
};

/* Ende: 3x sehr lang (1 s) */
static const NotificationSequence tt_sequence_end = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_delay_1000,
    &message_vibro_off,
    &message_delay_250,
    &message_vibro_on,
    &message_delay_1000,
    &message_vibro_off,
    &message_delay_250,
    &message_vibro_on,
    &message_delay_1000,
    &message_vibro_off,
    NULL,
};

/* Überzogen: 4x kurz */
static const NotificationSequence tt_sequence_overtime = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

/* "Geht nicht": 2x sehr kurz */
static const NotificationSequence tt_sequence_deny = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

/* Optionaler Ton (Standard aus): ein kurzer Piep nach der Vibration */
static const NotificationSequence tt_sequence_beep = {
    &message_note_c7,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void tt_alert_play(NotificationApp* notifications, TtTimerAlert alert, bool with_sound) {
    furi_assert(notifications);
    const NotificationSequence* sequence = NULL;
    switch(alert) {
    case TtTimerAlertWarn1:
        sequence = &tt_sequence_warn1;
        break;
    case TtTimerAlertWarn2:
        sequence = &tt_sequence_warn2;
        break;
    case TtTimerAlertEnd:
        sequence = &tt_sequence_end;
        break;
    case TtTimerAlertOvertime:
        sequence = &tt_sequence_overtime;
        break;
    default:
        return;
    }
    notification_message(notifications, sequence);
    if(with_sound) notification_message(notifications, &tt_sequence_beep);
}

void tt_alert_deny(NotificationApp* notifications) {
    furi_assert(notifications);
    notification_message(notifications, &tt_sequence_deny);
}
