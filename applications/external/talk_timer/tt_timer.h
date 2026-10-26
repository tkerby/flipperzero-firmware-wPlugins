#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Timerlogik ohne GUI. Die Zeit wird aus furi_get_tick() berechnet (nicht aus
 * gezählten Ticks), läuft also driftfrei, auch wenn ein Tick ausfällt.
 * tt_timer_poll() wird zyklisch aufgerufen und meldet fällige Warnungen genau
 * einmal.
 */
typedef enum {
    TtTimerIdle, /* noch nicht gestartet bzw. zurückgesetzt */
    TtTimerRunning,
    TtTimerPaused,
} TtTimerState;

typedef enum {
    TtTimerAlertNone,
    TtTimerAlertWarn1, /* früher Hinweis */
    TtTimerAlertWarn2, /* kurz vor Schluss */
    TtTimerAlertEnd, /* Redezeit abgelaufen */
    TtTimerAlertOvertime, /* wiederholte Erinnerung während der Überziehung */
} TtTimerAlert;

typedef struct {
    TtTimerState state;
    uint32_t total_seconds;
    uint32_t accumulated_ms; /* gelaufene Zeit vor dem aktuellen Lauf */
    uint32_t run_start_tick; /* Tick beim letzten Start (nur bei Running gültig) */

    uint32_t warn1_seconds; /* Sekunden vor Ende, 0 = aus */
    uint32_t warn2_seconds;
    uint32_t overtime_seconds; /* Wiederholintervall, 0 = aus */
    bool end_alert;

    bool warn1_done;
    bool warn2_done;
    bool end_done;
    uint32_t overtime_done; /* Anzahl bereits gemeldeter Überziehungs-Erinnerungen */
} TtTimer;

/* Setzt Redezeit und Warnungen und stellt den Timer auf Anfang */
void tt_timer_configure(
    TtTimer* timer,
    uint32_t total_seconds,
    uint32_t warn1_seconds,
    uint32_t warn2_seconds,
    uint32_t overtime_seconds,
    bool end_alert);

void tt_timer_start(TtTimer* timer);
void tt_timer_pause(TtTimer* timer);
void tt_timer_reset(TtTimer* timer);

/* Gelaufene Zeit in Sekunden (läuft nach Ablauf weiter = Überziehung) */
uint32_t tt_timer_elapsed_seconds(const TtTimer* timer);

/* true, sobald der Timer gestartet wurde und nicht zurückgesetzt ist */
bool tt_timer_is_started(const TtTimer* timer);

/* Liefert die nächste fällige Warnung genau einmal, sonst TtTimerAlertNone */
TtTimerAlert tt_timer_poll(TtTimer* timer);
