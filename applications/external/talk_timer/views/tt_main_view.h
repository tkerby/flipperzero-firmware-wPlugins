#pragma once

#include <gui/view.h>
#include "../tt_settings.h"
#include "../tt_timer.h"

/*
 * Hauptscreen: große Restzeit, Fortschrittsbalken, Status, Verbindungs-Icon
 * und Tastenleiste. Die View kennt weder Timer noch HID; sie meldet nur
 * Ereignisse an die App und zeigt an, was die App ihr übergibt.
 */
typedef struct TtMainView TtMainView;

typedef enum {
    TtMainEventNext, /* Rechts gedrückt: nächste Folie */
    TtMainEventPrev, /* Links gedrückt: vorherige Folie */
    TtMainEventStartPause, /* OK kurz */
    TtMainEventReset, /* OK lang */
    TtMainEventBlack, /* Hoch kurz: schwarzer Bildschirm */
    TtMainEventStartShow, /* Hoch lang: Präsentation starten */
    TtMainEventEndShow, /* Runter lang: Präsentation beenden */
    TtMainEventExit, /* Zurück lang: verlassen (App entscheidet über Dialog) */
} TtMainEvent;

typedef void (*TtMainViewCallback)(TtMainEvent event, void* context);

TtMainView* tt_main_view_alloc(void);
void tt_main_view_free(TtMainView* main_view);
View* tt_main_view_get_view(TtMainView* main_view);
void tt_main_view_set_callback(TtMainView* main_view, TtMainViewCallback callback, void* context);

/* Übernimmt Profil, Verbindung und Warnzeiten für die Anzeige */
void tt_main_view_set_config(TtMainView* main_view, const TtSettings* settings);

/* Zyklisch aus dem Tick-Callback der App: aktueller Timer- und Verbindungsstand */
void tt_main_view_update(
    TtMainView* main_view,
    TtTimerState state,
    uint32_t total_seconds,
    uint32_t elapsed_seconds,
    bool connected);
