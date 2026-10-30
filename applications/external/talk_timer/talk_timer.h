#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "tt_settings.h"
#include "tt_profiles.h"
#include "tt_alerts.h"
#include "tt_timer.h"
#include "tt_transport.h"
#include "views/tt_main_view.h"

/* IDs der Views im ViewDispatcher */
typedef enum {
    TtViewStart, /* Startmenü */
    TtViewSettings, /* Einstellungen */
    TtViewPresets, /* Redezeit-Presets */
    TtViewProfiles, /* Auswahl des Präsentationsprogramms */
    TtViewMain, /* Hauptscreen */
    TtViewDialog, /* Bestätigung beim Verlassen */
} TtViewId;

/*
 * Eigene Events: Menüs werden nicht direkt aus den Modul-Callbacks umgebaut
 * (die laufen teils unter dem Modell-Lock des Moduls), sondern zeitversetzt
 * im Event-Callback des ViewDispatchers.
 */
typedef enum {
    TtCustomEventRebuildMenus,
    TtCustomEventRebuildPresets,
    TtCustomEventKeyRelease, /* FuriTimer abgelaufen: gesendete Taste wieder loslassen */
    /*
     * Hauptscreen öffnen/verlassen: startet bzw. stoppt USB/Bluetooth und wartet
     * dabei auf andere Dienste. Das darf NICHT im Enter-Callback der
     * VariableItemList laufen (der hält die Modell-Sperre, die GUI wartet beim
     * Zeichnen darauf, der Bluetooth-Dienst wartet auf die GUI -> Deadlock).
     */
    TtCustomEventEnterMain,
    TtCustomEventLeaveMain,
    TtCustomEventMenuBack, /* Pfeil links in einem Untermenü: eine Ebene zurück */
} TtCustomEvent;

/* Zentrale App-Struktur */
typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;

    VariableItemList* menu_start;
    VariableItemList* menu_settings;
    Submenu* menu_presets;
    Submenu* menu_profiles;
    TtMainView* main_view;

    /*
     * Untermenüs liegen in einem ViewStack unter einer unsichtbaren View, die
     * nur "Pfeil links" abfängt (= zurück). Die Module selbst bieten das nicht.
     */
    ViewStack* stack_settings;
    ViewStack* stack_presets;
    ViewStack* stack_profiles;
    View* left_catcher_settings;
    View* left_catcher_presets;
    View* left_catcher_profiles;

    DialogEx* dialog;

    TtTimer timer;
    TtTransport* transport; /* NULL, solange der Hauptscreen nicht aktiv ist */
    bool connected; /* zuletzt gesehener Verbindungsstatus */
    FuriTimer* release_timer; /* lässt eine gesendete Taste zeitversetzt los */
    uint16_t pending_key; /* aktuell gedrückte Taste (HID-Code), 0 = keine */
    bool backlight_enforced; /* "Display anlassen" ist gerade aktiv */
    bool show_started; /* die App hat das Start-Kürzel gesendet (und noch kein Ende) */

    TtSettings settings;
    TtViewId current_view;
    bool preset_applied; /* Preset gewählt: Cursor im Startmenü auf "Start" setzen */

    /* Zeile "Profile: <Name>" im Startmenü (die Liste merkt sich nur den Zeiger) */
    char profile_label[40];

    /* Beschriftungen der Preset-Einträge (müssen so lange leben wie das Menü) */
    char preset_labels[TT_PRESET_COUNT][32];
} TalkTimerApp;
