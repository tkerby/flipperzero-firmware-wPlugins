#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <notification/notification.h>
#include "tt_timer.h"

/*
 * Warnungen des Timers: Auswahltabellen für die Einstellungen (Sekunden vor
 * Ende bzw. Wiederholintervall bei Überziehung). 0 Sekunden = aus.
 * Alle Tabellen sind AUFSTEIGEND sortiert: In der VariableItemList heißt
 * Links "Index - 1" und Rechts "Index + 1", also Links = weniger Zeit,
 * Rechts = mehr Zeit.
 * Dazu die Vibrationsmuster: jede Warnung ist an der ANZAHL der Pulse erkennbar.
 */
#define TT_WARN1_OPTIONS    7 /* Aus, 1, 2, 3, 5, 10, 15 min */
#define TT_WARN2_OPTIONS    4 /* Aus, 30 s, 1 min, 2 min */
#define TT_OVERTIME_OPTIONS 4 /* Aus, 1, 2, 5 min */

/* Standard: 5 min vor Ende, 1 min vor Ende, keine Wiederholung bei Überziehung */
#define TT_WARN1_DEFAULT_SECONDS    300
#define TT_WARN2_DEFAULT_SECONDS    60
#define TT_OVERTIME_DEFAULT_SECONDS 0

/* Sekunden zur Option (Index außerhalb der Tabelle liefert 0 = aus) */
uint32_t tt_warn1_seconds(uint8_t index);
uint32_t tt_warn2_seconds(uint8_t index);
uint32_t tt_overtime_seconds(uint8_t index);

/* Index zur Sekundenzahl; false, wenn der Wert in der Tabelle nicht vorkommt */
bool tt_warn1_index(uint32_t seconds, uint8_t* index);
bool tt_warn2_index(uint32_t seconds, uint8_t* index);
bool tt_overtime_index(uint32_t seconds, uint8_t* index);

/* Schreibt den Anzeigetext ("Off", "5 min", "30 s") in buf */
void tt_alert_option_text(uint32_t seconds, char* buf, size_t buf_size);

/* Kurzform für enge Stellen ("-", "5m", "30s") */
void tt_alert_short_text(uint32_t seconds, char* buf, size_t buf_size);

/*
 * Spielt das Muster zur Warnung (läuft im Notification-Thread, blockiert die
 * GUI nicht). Vibration immer, Ton nur wenn with_sound gesetzt ist:
 *   Warnung 1   = 1x lang
 *   Warnung 2   = 2x lang
 *   Ende        = 3x sehr lang
 *   Überzogen   = 4x kurz
 */
void tt_alert_play(NotificationApp* notifications, TtTimerAlert alert, bool with_sound);

/* Kurzes doppeltes Tippen: "geht gerade nicht" (z.B. keine Verbindung) */
void tt_alert_deny(NotificationApp* notifications);
