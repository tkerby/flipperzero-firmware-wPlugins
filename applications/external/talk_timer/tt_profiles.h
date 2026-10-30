#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Profile der Präsentationsprogramme: Welche Taste(nkombination) löst welche
 * Aktion aus. Reine Datentabelle in tt_profiles.c, dort stehen auch die
 * Quellen mit Abrufdatum.
 */
typedef enum {
    TtProfilePptWin,
    TtProfilePptMac,
    TtProfileKeynote,
    TtProfileSlidesWin,
    TtProfileSlidesMac,
    TtProfileGeneric,
    TtProfileCount,
} TtProfileId;

/* Aktionen, die der Hauptscreen auslösen kann */
typedef enum {
    TtActionNext, /* nächste Folie / Animation */
    TtActionPrev, /* vorherige Folie / Animation */
    TtActionStart, /* Präsentation ab aktueller Folie starten */
    TtActionBlack, /* schwarzer Bildschirm an/aus */
    TtActionEnd, /* Präsentation beenden */
    TtActionCount,
} TtAction;

typedef struct {
    const char* name; /* ausgeschriebener Name für das Auswahlmenü (ASCII) */
    const char* short_name; /* Kurzname für die Kopfzeile des Hauptscreens */
    /* HID-Code inkl. Modifier-Bits je Aktion; HID_KEYBOARD_NONE = nicht verfügbar */
    uint16_t keys[TtActionCount];
} TtProfile;

const TtProfile* tt_profile_get(TtProfileId id);

/* HID-Code der Aktion im Profil; HID_KEYBOARD_NONE (0), wenn es kein Kürzel gibt */
uint16_t tt_profile_key(TtProfileId id, TtAction action);
