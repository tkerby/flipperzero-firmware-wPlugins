#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tt_i18n.h"
#include "tt_profiles.h"

/* Verbindungsart zum Rechner */
typedef enum {
    TtConnUsb,
    TtConnBt,
    TtConnCount,
} TtConn;

/* Grenzen der Redezeit in Minuten */
#define TT_TALK_MIN_MINUTES     1
#define TT_TALK_MAX_MINUTES     180
#define TT_TALK_DEFAULT_MINUTES 20

/* Anzahl speicherbarer Redezeit-Presets */
#define TT_PRESET_COUNT 5

/*
 * Ein Preset merkt sich die Redezeit samt passender Warnzeiten, damit z.B. ein
 * 5-Minuten-Vortrag nicht mit "5 min vor Ende" warnt.
 */
typedef struct {
    uint32_t minutes;
    uint32_t warn1_seconds;
    uint32_t warn2_seconds;
} TtPreset;

/* Alles, was die App sich merkt */
typedef struct {
    TtConn conn;
    TtProfileId profile;
    TtLang lang;
    uint32_t talk_minutes;

    /* Sekunden vor Ende bzw. Wiederholintervall; 0 = aus (Tabellen in tt_alerts) */
    uint32_t warn1_seconds;
    uint32_t warn2_seconds;
    uint32_t overtime_seconds;
    bool end_alert; /* Vibration bei Ablauf der Redezeit */
    bool sound; /* zusätzlich Ton (Standard: aus) */
    bool keep_display; /* Display anlassen, solange der Timer läuft */
    /* OK startet beim ersten Start des Timers auch die Präsentation */
    bool autostart;

    TtPreset presets[TT_PRESET_COUNT];
} TtSettings;

/* Lädt die Einstellungen; bei Fehler bleiben die Standardwerte stehen */
void tt_settings_load(TtSettings* settings);

/* Speichert unter /ext/apps_data/talk_timer/settings.txt */
bool tt_settings_save(const TtSettings* settings);

/* Kurzer Anzeigename der Verbindung */
const char* tt_conn_name(TtConn conn);
