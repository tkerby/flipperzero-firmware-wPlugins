/*
 * Talk Timer – Präsentations-Klicker mit Redezeit-Timer per USB- oder
 * Bluetooth-HID. Einstiegspunkt, Navigation und Zusammenbau der Views.
 */
#include "talk_timer.h"

#include <dolphin/dolphin.h>

/* Intervall für Anzeige-Aktualisierung, Warnungen und Verbindungsabfrage */
#define TT_TICK_MS 250

/* Haltezeit eines gesendeten Tastendrucks, damit der Rechner ihn sicher erkennt */
#define TT_KEY_HOLD_MS 30

/*
 * Sprachregel: Die Firmware-Schriften kennen nur ASCII. Texte in Firmware-Modulen
 * (Menüs, Dialog, Tastenleiste) verwenden deshalb deutsche Wörter OHNE Umlaute,
 * keine Umschreibungen wie "ue". Echte Umlaute gibt es nur im selbst
 * gezeichneten Hauptscreen (views/tt_main_view.c).
 */

/* Einträge des Startmenüs (Reihenfolge = Position in der Liste) */
typedef enum {
    TtStartItemStart,
    TtStartItemTalkTime,
    TtStartItemProfile,
    TtStartItemConn,
    TtStartItemPresets,
    TtStartItemSettings,
} TtStartItem;

/* Einträge des Einstellungsmenüs */
typedef enum {
    TtSettingsItemBack, /* einzige Zeile ohne Wert: OK oder Pfeil links = zurück */
    TtSettingsItemWarn1,
    TtSettingsItemWarn2,
    TtSettingsItemEndAlert,
    TtSettingsItemOvertime,
    TtSettingsItemSound,
    TtSettingsItemDisplay,
    TtSettingsItemAutostart,
    TtSettingsItemTest, /* Aktion: alle Vibrationsmuster nacheinander abspielen */
    TtSettingsItemForgetBt, /* Aktion: eigene Bluetooth-Kopplung löschen */
    TtSettingsItemLanguage,
} TtSettingsItem;

/* Zeilen ohne einstellbaren Wert: dort heißt "Pfeil links" zurück */
static bool tt_settings_row_is_action(uint8_t index) {
    return index == TtSettingsItemBack || index == TtSettingsItemTest ||
           index == TtSettingsItemForgetBt;
}

/* ---- Hilfsfunktionen ----------------------------------------------------- */

static void tt_switch_view(TalkTimerApp* app, TtViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

static const char* tt_on_off(bool on) {
    return on ? tt_tr("On", "An") : tt_tr("Off", "Aus");
}

static void tt_set_minutes_text(VariableItem* item, uint32_t minutes) {
    char text[12];
    snprintf(text, sizeof(text), "%lu min", (unsigned long)minutes);
    variable_item_set_current_value_text(item, text);
}

/* ---- Startmenü ----------------------------------------------------------- */

static void tt_start_talk_time_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    /* Index 0 entspricht der kleinsten Redezeit */
    app->settings.talk_minutes = variable_item_get_current_value_index(item) + TT_TALK_MIN_MINUTES;
    tt_set_minutes_text(item, app->settings.talk_minutes);
}

static void tt_start_conn_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.conn = (TtConn)variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, tt_conn_name(app->settings.conn));
}

static void tt_build_start_menu(TalkTimerApp* app) {
    VariableItemList* list = app->menu_start;
    VariableItem* item;
    variable_item_list_reset(list);

    /* Reine Aktions-Einträge haben genau einen (leeren) Wert */
    item = variable_item_list_add(list, tt_tr("Start talk", "Vortrag starten"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    item = variable_item_list_add(
        list,
        tt_tr("Talk time", "Redezeit"),
        TT_TALK_MAX_MINUTES - TT_TALK_MIN_MINUTES + 1,
        tt_start_talk_time_changed,
        app);
    variable_item_set_current_value_index(item, app->settings.talk_minutes - TT_TALK_MIN_MINUTES);
    tt_set_minutes_text(item, app->settings.talk_minutes);

    /*
     * Profil: Die Namen sind für den schmalen Wertebereich der Liste zu lang
     * (unter der offiziellen Firmware liefen sie über die Beschriftung), deshalb
     * Aktionszeile "Profile: <Name>" mit eigenem Auswahlmenü.
     */
    snprintf(
        app->profile_label,
        sizeof(app->profile_label),
        "%s: %s",
        tt_tr("Profile", "Profil"),
        tt_profile_get(app->settings.profile)->name);
    item = variable_item_list_add(list, app->profile_label, 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    item = variable_item_list_add(
        list, tt_tr("Connection", "Verbindung"), TtConnCount, tt_start_conn_changed, app);
    variable_item_set_current_value_index(item, app->settings.conn);
    variable_item_set_current_value_text(item, tt_conn_name(app->settings.conn));

    item = variable_item_list_add(list, tt_tr("Time presets", "Zeitvorlagen"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    item = variable_item_list_add(list, tt_tr("Settings", "Einstellungen"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");
}

static void tt_enter_main(TalkTimerApp* app);

static void tt_start_enter_callback(void* context, uint32_t index) {
    TalkTimerApp* app = context;
    switch(index) {
    case TtStartItemStart:
        /* Zeitversetzt außerhalb der Listen-Sperre, siehe TtCustomEventEnterMain */
        view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventEnterMain);
        break;
    case TtStartItemProfile:
        submenu_set_selected_item(app->menu_profiles, app->settings.profile);
        tt_switch_view(app, TtViewProfiles);
        break;
    case TtStartItemPresets:
        tt_switch_view(app, TtViewPresets);
        break;
    case TtStartItemSettings:
        tt_switch_view(app, TtViewSettings);
        break;
    default:
        break;
    }
}

/* ---- Einstellungen ------------------------------------------------------- */

static void tt_settings_warn1_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    char text[12];
    app->settings.warn1_seconds = tt_warn1_seconds(variable_item_get_current_value_index(item));
    tt_alert_option_text(app->settings.warn1_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void tt_settings_warn2_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    char text[12];
    app->settings.warn2_seconds = tt_warn2_seconds(variable_item_get_current_value_index(item));
    tt_alert_option_text(app->settings.warn2_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void tt_settings_overtime_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    char text[12];
    app->settings.overtime_seconds =
        tt_overtime_seconds(variable_item_get_current_value_index(item));
    tt_alert_option_text(app->settings.overtime_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void tt_settings_end_alert_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.end_alert = (variable_item_get_current_value_index(item) != 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.end_alert));
}

static void tt_settings_sound_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.sound = (variable_item_get_current_value_index(item) != 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.sound));
}

static void tt_settings_display_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.keep_display = (variable_item_get_current_value_index(item) != 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.keep_display));
}

static void tt_settings_autostart_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.autostart = (variable_item_get_current_value_index(item) != 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.autostart));
}

static void tt_settings_language_changed(VariableItem* item) {
    TalkTimerApp* app = variable_item_get_context(item);
    app->settings.lang = (TtLang)variable_item_get_current_value_index(item);
    tt_lang_set(app->settings.lang);
    variable_item_set_current_value_text(item, tt_lang_name(app->settings.lang));
    /* Alle Menüs neu beschriften; zeitversetzt, siehe TtCustomEvent */
    view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventRebuildMenus);
}

static void tt_build_settings_menu(TalkTimerApp* app) {
    VariableItemList* list = app->menu_settings;
    VariableItem* item;
    char text[12];
    uint8_t index;
    variable_item_list_reset(list);

    /* Reihenfolge muss zu TtSettingsItem passen */
    item = variable_item_list_add(list, tt_tr("< Done", "< Fertig"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    /* Warnzeiten: Tabellen sind aufsteigend, Links = weniger, Rechts = mehr */
    item = variable_item_list_add(
        list, tt_tr("Warning 1", "Warnung 1"), TT_WARN1_OPTIONS, tt_settings_warn1_changed, app);
    index = 0;
    tt_warn1_index(app->settings.warn1_seconds, &index);
    variable_item_set_current_value_index(item, index);
    tt_alert_option_text(app->settings.warn1_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(
        list, tt_tr("Warning 2", "Warnung 2"), TT_WARN2_OPTIONS, tt_settings_warn2_changed, app);
    index = 0;
    tt_warn2_index(app->settings.warn2_seconds, &index);
    variable_item_set_current_value_index(item, index);
    tt_alert_option_text(app->settings.warn2_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(
        list, tt_tr("End alert", "Alarm am Ende"), 2, tt_settings_end_alert_changed, app);
    variable_item_set_current_value_index(item, app->settings.end_alert ? 1 : 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.end_alert));

    item = variable_item_list_add(
        list,
        tt_tr("Overtime alert", "Alarm danach"),
        TT_OVERTIME_OPTIONS,
        tt_settings_overtime_changed,
        app);
    index = 0;
    tt_overtime_index(app->settings.overtime_seconds, &index);
    variable_item_set_current_value_index(item, index);
    tt_alert_option_text(app->settings.overtime_seconds, text, sizeof(text));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(list, tt_tr("Sound", "Ton"), 2, tt_settings_sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.sound));

    item = variable_item_list_add(
        list, tt_tr("Display on", "Display an"), 2, tt_settings_display_changed, app);
    variable_item_set_current_value_index(item, app->settings.keep_display ? 1 : 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.keep_display));

    /* OK startet Timer UND Präsentation (siehe tt_main_view_callback) */
    item = variable_item_list_add(list, "Autostart", 2, tt_settings_autostart_changed, app);
    variable_item_set_current_value_index(item, app->settings.autostart ? 1 : 0);
    variable_item_set_current_value_text(item, tt_on_off(app->settings.autostart));

    /* Aktionszeilen mit leerem Wert: so bekommt die Beschriftung die volle Breite */
    item = variable_item_list_add(list, tt_tr("Test alarms", "Alarme testen"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    item = variable_item_list_add(list, tt_tr("Forget BT pairing", "BT vergessen"), 1, NULL, app);
    variable_item_set_current_value_text(item, "");

    /* Zeigt die AKTIVE Sprache; Links/Rechts schaltet um */
    item = variable_item_list_add(
        list, tt_tr("Language", "Sprache"), TtLangCount, tt_settings_language_changed, app);
    variable_item_set_current_value_index(item, app->settings.lang);
    variable_item_set_current_value_text(item, tt_lang_name(app->settings.lang));
}

/* Pause zwischen den Mustern beim Test */
static const NotificationSequence tt_sequence_gap = {
    &message_delay_1000,
    NULL,
};

static void tt_settings_enter_callback(void* context, uint32_t index) {
    TalkTimerApp* app = context;
    if(index == TtSettingsItemBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventMenuBack);
    } else if(index == TtSettingsItemTest) {
        /* Reihenfolge wie im Vortrag; die Notification-Warteschlange spielt sie nacheinander */
        tt_alert_play(app->notifications, TtTimerAlertWarn1, app->settings.sound);
        notification_message(app->notifications, &tt_sequence_gap);
        tt_alert_play(app->notifications, TtTimerAlertWarn2, app->settings.sound);
        notification_message(app->notifications, &tt_sequence_gap);
        tt_alert_play(app->notifications, TtTimerAlertEnd, app->settings.sound);
    } else if(index == TtSettingsItemForgetBt) {
        /* Im Menü läuft der Transport nie, die Keys-Datei ist also frei */
        tt_transport_forget_bt_pairing();
        /* Bewusst ohne Ton: nur Vibration als Bestätigung */
        notification_message(app->notifications, &sequence_double_vibro);
    }
}

/* ---- Presets ------------------------------------------------------------- */

static void tt_presets_callback(void* context, InputType input_type, uint32_t index) {
    TalkTimerApp* app = context;
    if(index >= TT_PRESET_COUNT) return;
    TtPreset* preset = &app->settings.presets[index];

    if(input_type == InputTypeShort) {
        /* Preset übernehmen: Redezeit UND die dazu gespeicherten Warnzeiten */
        app->settings.talk_minutes = preset->minutes;
        app->settings.warn1_seconds = preset->warn1_seconds;
        app->settings.warn2_seconds = preset->warn2_seconds;
        app->preset_applied = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventRebuildMenus);
        tt_switch_view(app, TtViewStart);
    } else if(input_type == InputTypeLong) {
        /* Aktuelle Redezeit samt aktueller Warnzeiten in diesem Slot sichern */
        preset->minutes = app->settings.talk_minutes;
        preset->warn1_seconds = app->settings.warn1_seconds;
        preset->warn2_seconds = app->settings.warn2_seconds;
        tt_settings_save(&app->settings);
        notification_message(app->notifications, &sequence_single_vibro);
        view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventRebuildPresets);
    }
}

static void tt_build_presets_menu(TalkTimerApp* app) {
    uint32_t selected = submenu_get_selected_item(app->menu_presets);
    submenu_reset(app->menu_presets);
    submenu_set_header(app->menu_presets, tt_tr("Hold OK = save", "OK lang = sichern"));
    for(uint32_t i = 0; i < TT_PRESET_COUNT; i++) {
        /* Beschriftung: Redezeit und dahinter die beiden Warnzeiten, z.B. "20 min  5m/1m" */
        char warn1[8];
        char warn2[8];
        tt_alert_short_text(app->settings.presets[i].warn1_seconds, warn1, sizeof(warn1));
        tt_alert_short_text(app->settings.presets[i].warn2_seconds, warn2, sizeof(warn2));
        snprintf(
            app->preset_labels[i],
            sizeof(app->preset_labels[i]),
            "%lu min  %s/%s",
            (unsigned long)app->settings.presets[i].minutes,
            warn1,
            warn2);
        submenu_add_item_ex(app->menu_presets, app->preset_labels[i], i, tt_presets_callback, app);
    }
    if(selected < TT_PRESET_COUNT) submenu_set_selected_item(app->menu_presets, selected);
}

/* ---- Profile ------------------------------------------------------------- */

static void tt_profiles_callback(void* context, uint32_t index) {
    TalkTimerApp* app = context;
    if(index >= TtProfileCount) return;
    app->settings.profile = (TtProfileId)index;
    /* Startmenü neu beschriften; zeitversetzt, siehe TtCustomEvent */
    view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventRebuildMenus);
    tt_switch_view(app, TtViewStart);
}

static void tt_build_profiles_menu(TalkTimerApp* app) {
    submenu_reset(app->menu_profiles);
    submenu_set_header(app->menu_profiles, tt_tr("Presentation app", "Programm"));
    for(uint32_t i = 0; i < TtProfileCount; i++) {
        submenu_add_item(
            app->menu_profiles, tt_profile_get((TtProfileId)i)->name, i, tt_profiles_callback, app);
    }
    submenu_set_selected_item(app->menu_profiles, app->settings.profile);
}

/* ---- Pfeil links = zurück (Untermenüs) ----------------------------------- */

/*
 * Eingabe-Callback der unsichtbaren View, die im ViewStack ÜBER dem Menü liegt
 * und deshalb zuerst gefragt wird. Verbraucht nur "Links kurz", wenn das als
 * "zurück" gemeint sein kann; alles andere geht unverändert ans Menü.
 */
static bool tt_left_catcher_input(InputEvent* event, void* context) {
    TalkTimerApp* app = context;
    if(event->key != InputKeyLeft || event->type != InputTypeShort) return false;

    if(app->current_view == TtViewSettings) {
        /* In Zeilen mit Wert heißt Links "weniger"; zurück nur in Zeilen ohne Wert */
        if(!tt_settings_row_is_action(
               variable_item_list_get_selected_item_index(app->menu_settings)))
            return false;
    } else if(app->current_view != TtViewPresets && app->current_view != TtViewProfiles) {
        return false;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventMenuBack);
    return true;
}

static View* tt_left_catcher_alloc(TalkTimerApp* app) {
    /* Kein Draw-Callback: die View zeichnet nichts und verdeckt das Menü nicht */
    View* view = view_alloc();
    view_set_context(view, app);
    view_set_input_callback(view, tt_left_catcher_input);
    return view;
}

/* ---- Hauptscreen: Transport, Tasten, Timer -------------------------------- */

static void tt_update_main_view(TalkTimerApp* app) {
    tt_main_view_update(
        app->main_view,
        app->timer.state,
        app->timer.total_seconds,
        tt_timer_elapsed_seconds(&app->timer),
        app->connected);
}

/* "Display anlassen": nur solange der Timer läuft und die Option gesetzt ist */
static void tt_update_backlight(TalkTimerApp* app) {
    bool enforce = app->settings.keep_display && app->timer.state == TtTimerRunning &&
                   app->current_view != TtViewStart;
    if(enforce == app->backlight_enforced) return;
    app->backlight_enforced = enforce;
    notification_message(
        app->notifications,
        enforce ? &sequence_display_backlight_enforce_on :
                  &sequence_display_backlight_enforce_auto);
}

static void tt_release_pending_key(TalkTimerApp* app) {
    if(app->pending_key && app->transport) {
        tt_transport_release(app->transport, app->pending_key);
    }
    app->pending_key = 0;
}

/* Läuft im Timer-Thread: nur ein Event absetzen, losgelassen wird im GUI-Thread */
static void tt_release_timer_callback(void* context) {
    TalkTimerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventKeyRelease);
}

/*
 * Sendet die Taste(nkombination) der Aktion aus der Profil-Tabelle: drücken und
 * per FuriTimer nach TT_KEY_HOLD_MS wieder loslassen (kein Warten im GUI-Thread).
 */
static bool tt_send_action(TalkTimerApp* app, TtAction action) {
    uint16_t key = tt_profile_key(app->settings.profile, action);
    if(key == 0 || !app->transport || !tt_transport_is_connected(app->transport)) {
        /* Kein Kürzel im Profil oder keine Verbindung */
        tt_alert_deny(app->notifications);
        return false;
    }
    /* Falls die vorige Taste noch gedrückt ist: erst loslassen */
    furi_timer_stop(app->release_timer);
    tt_release_pending_key(app);

    tt_transport_press(app->transport, key);
    app->pending_key = key;
    furi_timer_start(app->release_timer, furi_ms_to_ticks(TT_KEY_HOLD_MS));
    return true;
}

static void tt_transport_start(TalkTimerApp* app) {
    if(app->transport) return;
    app->transport = tt_transport_alloc(app->settings.conn);
    app->connected = tt_transport_is_connected(app->transport);
}

static void tt_transport_stop(TalkTimerApp* app) {
    if(!app->transport) return;
    furi_timer_stop(app->release_timer);
    app->pending_key = 0;
    /* tt_transport_free lässt alle Tasten los und setzt USB/Bluetooth zurück */
    tt_transport_free(app->transport);
    app->transport = NULL;
    app->connected = false;
}

/* Auswahl merken, Timer vorbereiten, Transport starten, Hauptscreen öffnen */
static void tt_enter_main(TalkTimerApp* app) {
    tt_settings_save(&app->settings);
    tt_timer_configure(
        &app->timer,
        app->settings.talk_minutes * 60U,
        app->settings.warn1_seconds,
        app->settings.warn2_seconds,
        app->settings.overtime_seconds,
        app->settings.end_alert);
    tt_main_view_set_config(app->main_view, &app->settings);
    app->show_started = false;
    tt_transport_start(app);
    tt_update_main_view(app);
    tt_switch_view(app, TtViewMain);
}

/* Hauptscreen endgültig verlassen: Timer verwerfen, USB/Bluetooth zurücksetzen */
static void tt_leave_main(TalkTimerApp* app) {
    tt_timer_reset(&app->timer);
    tt_transport_stop(app);
    tt_switch_view(app, TtViewStart);
    tt_update_backlight(app);
}

static void tt_dialog_callback(DialogExResult result, void* context) {
    TalkTimerApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventLeaveMain);
    } else {
        tt_switch_view(app, TtViewMain);
    }
}

static void tt_show_exit_dialog(TalkTimerApp* app) {
    /* dialog_ex_reset löscht auch Callback und Kontext -> danach neu setzen */
    dialog_ex_reset(app->dialog);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, tt_dialog_callback);
    dialog_ex_set_header(
        app->dialog, tt_tr("Leave the talk?", "Vortrag verlassen?"), 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog,
        tt_tr(
            "The timer is active\nand will be discarded.",
            "Der Timer ist aktiv\nund wird verworfen."),
        64,
        20,
        AlignCenter,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog, tt_tr("No", "Nein"));
    dialog_ex_set_right_button_text(app->dialog, tt_tr("Yes", "Ja"));
    tt_switch_view(app, TtViewDialog);
}

static void tt_main_view_callback(TtMainEvent event, void* context) {
    TalkTimerApp* app = context;
    switch(event) {
    case TtMainEventNext:
        tt_send_action(app, TtActionNext);
        break;
    case TtMainEventPrev:
        tt_send_action(app, TtActionPrev);
        break;
    case TtMainEventBlack:
        tt_send_action(app, TtActionBlack);
        break;
    case TtMainEventStartShow:
        /* Kurze Bestätigung, weil ein langer Druck sonst keine Rückmeldung hätte */
        if(tt_send_action(app, TtActionStart)) {
            app->show_started = true;
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    case TtMainEventEndShow:
        if(tt_send_action(app, TtActionEnd)) {
            app->show_started = false;
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    case TtMainEventStartPause:
        if(app->timer.state == TtTimerRunning) {
            tt_timer_pause(&app->timer);
            break;
        }
        /*
         * Autostart: Beim ersten Start des Timers auch die Präsentation starten.
         * Nur wenn die App sie nicht schon gestartet hat: In Keynote ist
         * Opt+Cmd+P ein Umschalter und würde eine laufende Präsentation BEENDEN
         * (geprüft 2026-09-17, Keynote 15.3.1). Ohne Verbindung oder ohne
         * Start-Kürzel im Profil läuft nur der Timer los, ohne Fehlermeldung.
         */
        if(app->settings.autostart && app->timer.state == TtTimerIdle && !app->show_started &&
           app->connected && tt_profile_key(app->settings.profile, TtActionStart) != 0) {
            if(tt_send_action(app, TtActionStart)) app->show_started = true;
        }
        tt_timer_start(&app->timer);
        break;
    case TtMainEventReset:
        /* Schutz: ein laufender Timer lässt sich nicht versehentlich zurücksetzen */
        if(app->timer.state == TtTimerRunning) {
            tt_alert_deny(app->notifications);
        } else {
            tt_timer_reset(&app->timer);
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    case TtMainEventExit:
        /* Wurde der Timer gestartet, nur mit Bestätigung verlassen */
        if(tt_timer_is_started(&app->timer)) {
            tt_show_exit_dialog(app);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, TtCustomEventLeaveMain);
        }
        return;
    }
    tt_update_backlight(app);
    tt_update_main_view(app);
}

/* ---- ViewDispatcher-Callbacks -------------------------------------------- */

static bool tt_navigation_callback(void* context);

static bool tt_custom_event_callback(void* context, uint32_t event) {
    TalkTimerApp* app = context;
    switch(event) {
    case TtCustomEventRebuildMenus: {
        /* Cursor-Positionen über den Umbau hinweg behalten */
        uint8_t selected_start = variable_item_list_get_selected_item_index(app->menu_start);
        uint8_t selected_settings = variable_item_list_get_selected_item_index(app->menu_settings);
        tt_build_start_menu(app);
        tt_build_settings_menu(app);
        tt_build_presets_menu(app);
        tt_build_profiles_menu(app);
        /* Nach der Preset-Wahl steht der Cursor auf "Vortrag starten" */
        if(app->preset_applied) selected_start = TtStartItemStart;
        app->preset_applied = false;
        app->transport = NULL;
        app->connected = false;
        app->pending_key = 0;
        app->backlight_enforced = false;
        app->show_started = false;
        tt_timer_configure(&app->timer, app->settings.talk_minutes * 60U, 0, 0, 0, false);
        variable_item_list_set_selected_item(app->menu_start, selected_start);
        variable_item_list_set_selected_item(app->menu_settings, selected_settings);
        return true;
    }
    case TtCustomEventRebuildPresets:
        tt_build_presets_menu(app);
        return true;
    case TtCustomEventEnterMain:
        if(app->current_view == TtViewStart) tt_enter_main(app);
        return true;
    case TtCustomEventLeaveMain:
        if(app->current_view == TtViewMain || app->current_view == TtViewDialog) {
            tt_leave_main(app);
        }
        return true;
    case TtCustomEventKeyRelease:
        tt_release_pending_key(app);
        return true;
    case TtCustomEventMenuBack:
        /* Gleicher Weg wie die Zurück-Taste (inkl. Speichern) */
        if(app->current_view == TtViewSettings || app->current_view == TtViewPresets ||
           app->current_view == TtViewProfiles) {
            tt_navigation_callback(app);
        }
        return true;
    default:
        return false;
    }
}

/* Wird aufgerufen, wenn "Zurück" kurz gedrückt und von der View nicht verbraucht wurde */
static bool tt_navigation_callback(void* context) {
    TalkTimerApp* app = context;
    switch(app->current_view) {
    case TtViewStart:
        return false; /* App beenden */
    case TtViewSettings:
        tt_settings_save(&app->settings);
        tt_switch_view(app, TtViewStart);
        return true;
    case TtViewPresets:
    case TtViewProfiles:
        tt_switch_view(app, TtViewStart);
        return true;
    case TtViewDialog:
        /* Zurück im Dialog heißt "Nein" */
        tt_switch_view(app, TtViewMain);
        return true;
    default:
        /* Der Hauptscreen verbraucht "Zurück" selbst (Schutz vor Fehlbedienung) */
        return true;
    }
}

/*
 * Zyklisch: Verbindungsstatus abfragen (der USB-HID-Callback der Firmware läuft
 * im Interrupt und darf keine GUI-Events absetzen), fällige Warnungen spielen,
 * Anzeige aktualisieren. Läuft auch, während der Dialog offen ist.
 */
static void tt_tick_callback(void* context) {
    TalkTimerApp* app = context;
    if(!app->transport) return;

    bool connected = tt_transport_is_connected(app->transport);
    if(connected != app->connected) {
        app->connected = connected;
        notification_message(
            app->notifications, connected ? &sequence_blink_green_100 : &sequence_blink_red_100);
    }

    TtTimerAlert alert = tt_timer_poll(&app->timer);
    if(alert != TtTimerAlertNone) {
        tt_alert_play(app->notifications, alert, app->settings.sound);
    }
    tt_update_main_view(app);
}

/* ---- Auf- und Abbau ------------------------------------------------------ */

static TalkTimerApp* talk_timer_app_alloc(void) {
    TalkTimerApp* app = malloc(sizeof(TalkTimerApp));

    tt_settings_load(&app->settings);
    tt_lang_set(app->settings.lang);
    app->preset_applied = false;
    app->transport = NULL;
    app->connected = false;
    app->pending_key = 0;
    app->backlight_enforced = false;
    app->show_started = false;
    tt_timer_configure(&app->timer, app->settings.talk_minutes * 60U, 0, 0, 0, false);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, tt_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, tt_custom_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tt_tick_callback, TT_TICK_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->menu_start = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->menu_start, tt_start_enter_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, TtViewStart, variable_item_list_get_view(app->menu_start));

    app->menu_settings = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->menu_settings, tt_settings_enter_callback, app);
    app->left_catcher_settings = tt_left_catcher_alloc(app);
    app->stack_settings = view_stack_alloc();
    view_stack_add_view(app->stack_settings, variable_item_list_get_view(app->menu_settings));
    view_stack_add_view(app->stack_settings, app->left_catcher_settings);
    view_dispatcher_add_view(
        app->view_dispatcher, TtViewSettings, view_stack_get_view(app->stack_settings));

    app->menu_presets = submenu_alloc();
    app->left_catcher_presets = tt_left_catcher_alloc(app);
    app->stack_presets = view_stack_alloc();
    view_stack_add_view(app->stack_presets, submenu_get_view(app->menu_presets));
    view_stack_add_view(app->stack_presets, app->left_catcher_presets);
    view_dispatcher_add_view(
        app->view_dispatcher, TtViewPresets, view_stack_get_view(app->stack_presets));

    app->menu_profiles = submenu_alloc();
    app->left_catcher_profiles = tt_left_catcher_alloc(app);
    app->stack_profiles = view_stack_alloc();
    view_stack_add_view(app->stack_profiles, submenu_get_view(app->menu_profiles));
    view_stack_add_view(app->stack_profiles, app->left_catcher_profiles);
    view_dispatcher_add_view(
        app->view_dispatcher, TtViewProfiles, view_stack_get_view(app->stack_profiles));

    app->main_view = tt_main_view_alloc();
    tt_main_view_set_callback(app->main_view, tt_main_view_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, TtViewMain, tt_main_view_get_view(app->main_view));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TtViewDialog, dialog_ex_get_view(app->dialog));

    app->release_timer = furi_timer_alloc(tt_release_timer_callback, FuriTimerTypeOnce, app);

    tt_build_start_menu(app);
    tt_build_settings_menu(app);
    tt_build_presets_menu(app);
    tt_build_profiles_menu(app);

    return app;
}

static void talk_timer_app_free(TalkTimerApp* app) {
    furi_assert(app);

    /* Falls die App nicht über das Startmenü endet: alles sauber zurücksetzen */
    tt_transport_stop(app);
    furi_timer_stop(app->release_timer);
    furi_timer_free(app->release_timer);
    if(app->backlight_enforced) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }
    notification_message(app->notifications, &sequence_reset_rgb);

    view_dispatcher_remove_view(app->view_dispatcher, TtViewDialog);
    dialog_ex_free(app->dialog);
    view_dispatcher_remove_view(app->view_dispatcher, TtViewStart);
    view_dispatcher_remove_view(app->view_dispatcher, TtViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, TtViewPresets);
    view_dispatcher_remove_view(app->view_dispatcher, TtViewProfiles);
    view_dispatcher_remove_view(app->view_dispatcher, TtViewMain);

    /* Erst die Stacks leeren und freigeben, dann die enthaltenen Views */
    view_stack_remove_view(app->stack_settings, app->left_catcher_settings);
    view_stack_remove_view(app->stack_settings, variable_item_list_get_view(app->menu_settings));
    view_stack_free(app->stack_settings);
    view_stack_remove_view(app->stack_presets, app->left_catcher_presets);
    view_stack_remove_view(app->stack_presets, submenu_get_view(app->menu_presets));
    view_stack_free(app->stack_presets);
    view_stack_remove_view(app->stack_profiles, app->left_catcher_profiles);
    view_stack_remove_view(app->stack_profiles, submenu_get_view(app->menu_profiles));
    view_stack_free(app->stack_profiles);
    view_free(app->left_catcher_settings);
    view_free(app->left_catcher_presets);
    view_free(app->left_catcher_profiles);

    variable_item_list_free(app->menu_start);
    variable_item_list_free(app->menu_settings);
    submenu_free(app->menu_presets);
    submenu_free(app->menu_profiles);
    tt_main_view_free(app->main_view);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

/* ---- Einstiegspunkt ------------------------------------------------------ */

int32_t talk_timer_app(void* p) {
    UNUSED(p);
    TalkTimerApp* app = talk_timer_app_alloc();

    dolphin_deed(DolphinDeedPluginStart);
    tt_switch_view(app, TtViewStart);
    view_dispatcher_run(app->view_dispatcher);

    /* Letzte Auswahl merken */
    tt_settings_save(&app->settings);
    talk_timer_app_free(app);
    return 0;
}
