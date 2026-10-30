#include "tt_settings.h"
#include "tt_alerts.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define TAG "TtSettings"

/* Ablageort: /ext/apps_data/talk_timer/ (APP_DATA_PATH löst auf die App-ID auf) */
#define TT_SETTINGS_DIR     EXT_PATH("apps_data/talk_timer")
#define TT_SETTINGS_FILE    APP_DATA_PATH("settings.txt")
#define TT_SETTINGS_HEADER  "Talk Timer Settings"
#define TT_SETTINGS_VERSION 1

/* Schlüssel in der Datei */
#define KEY_CONN     "Connection"
#define KEY_PROFILE  "Profile"
#define KEY_LANG     "Language"
#define KEY_MINUTES  "TalkMinutes"
#define KEY_WARN1    "Warn1Seconds"
#define KEY_WARN2    "Warn2Seconds"
#define KEY_OVERTIME "OvertimeSeconds"
#define KEY_END      "EndAlert"
#define KEY_SOUND    "Sound"
#define KEY_DISPLAY  "KeepDisplay"
#define KEY_AUTO     "Autostart"
#define KEY_PRESETS  "PresetMinutes"
#define KEY_PRESETS1 "PresetWarn1Seconds"
#define KEY_PRESETS2 "PresetWarn2Seconds"

/* Standard-Presets: Warnzeiten wachsen mit der Redezeit */
static const TtPreset tt_preset_defaults[TT_PRESET_COUNT] = {
    {.minutes = 5, .warn1_seconds = 120, .warn2_seconds = 30},
    {.minutes = 10, .warn1_seconds = 180, .warn2_seconds = 60},
    {.minutes = 20, .warn1_seconds = 300, .warn2_seconds = 60},
    {.minutes = 30, .warn1_seconds = 300, .warn2_seconds = 60},
    {.minutes = 45, .warn1_seconds = 600, .warn2_seconds = 120},
};

static bool tt_minutes_valid(uint32_t minutes) {
    return minutes >= TT_TALK_MIN_MINUTES && minutes <= TT_TALK_MAX_MINUTES;
}

/* Liest einen uint32 und übernimmt ihn nur, wenn er kleiner als limit ist */
static void tt_read_limited(FlipperFormat* ff, const char* key, uint32_t limit, uint32_t* target) {
    uint32_t value = 0;
    /* Schlüssel dürfen fehlen oder in anderer Reihenfolge stehen */
    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, key, &value, 1) && value < limit) *target = value;
}

void tt_settings_load(TtSettings* settings) {
    furi_assert(settings);

    /* Standardwerte, falls nichts gespeichert ist */
    settings->conn = TtConnUsb;
    settings->profile = TtProfileKeynote;
    settings->lang = TtLangEn;
    settings->talk_minutes = TT_TALK_DEFAULT_MINUTES;
    settings->warn1_seconds = TT_WARN1_DEFAULT_SECONDS;
    settings->warn2_seconds = TT_WARN2_DEFAULT_SECONDS;
    settings->overtime_seconds = TT_OVERTIME_DEFAULT_SECONDS;
    settings->end_alert = true;
    settings->sound = false;
    settings->keep_display = true;
    settings->autostart = true;
    for(size_t i = 0; i < TT_PRESET_COUNT; i++) {
        settings->presets[i] = tt_preset_defaults[i];
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, TT_SETTINGS_FILE)) break;

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(furi_string_cmp_str(header, TT_SETTINGS_HEADER) != 0) break;
        if(version != TT_SETTINGS_VERSION) break;

        uint32_t value;

        value = settings->conn;
        tt_read_limited(ff, KEY_CONN, TtConnCount, &value);
        settings->conn = (TtConn)value;

        value = settings->profile;
        tt_read_limited(ff, KEY_PROFILE, TtProfileCount, &value);
        settings->profile = (TtProfileId)value;

        value = settings->lang;
        tt_read_limited(ff, KEY_LANG, TtLangCount, &value);
        settings->lang = (TtLang)value;

        value = settings->talk_minutes;
        tt_read_limited(ff, KEY_MINUTES, TT_TALK_MAX_MINUTES + 1, &value);
        if(tt_minutes_valid(value)) settings->talk_minutes = value;

        /* Warnzeiten nur übernehmen, wenn sie in den Auswahltabellen vorkommen */
        uint8_t index;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, KEY_WARN1, &value, 1) && tt_warn1_index(value, &index))
            settings->warn1_seconds = value;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, KEY_WARN2, &value, 1) && tt_warn2_index(value, &index))
            settings->warn2_seconds = value;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, KEY_OVERTIME, &value, 1) &&
           tt_overtime_index(value, &index))
            settings->overtime_seconds = value;

        value = settings->end_alert;
        tt_read_limited(ff, KEY_END, 2, &value);
        settings->end_alert = (value != 0);

        value = settings->sound;
        tt_read_limited(ff, KEY_SOUND, 2, &value);
        settings->sound = (value != 0);

        value = settings->keep_display;
        tt_read_limited(ff, KEY_DISPLAY, 2, &value);
        settings->keep_display = (value != 0);

        value = settings->autostart;
        tt_read_limited(ff, KEY_AUTO, 2, &value);
        settings->autostart = (value != 0);

        /* Presets nur übernehmen, wenn alle drei Listen vollständig und gültig sind */
        uint32_t minutes[TT_PRESET_COUNT];
        uint32_t warn1[TT_PRESET_COUNT];
        uint32_t warn2[TT_PRESET_COUNT];
        bool valid = true;
        flipper_format_rewind(ff);
        valid &= flipper_format_read_uint32(ff, KEY_PRESETS, minutes, TT_PRESET_COUNT);
        flipper_format_rewind(ff);
        valid &= flipper_format_read_uint32(ff, KEY_PRESETS1, warn1, TT_PRESET_COUNT);
        flipper_format_rewind(ff);
        valid &= flipper_format_read_uint32(ff, KEY_PRESETS2, warn2, TT_PRESET_COUNT);
        for(size_t i = 0; valid && i < TT_PRESET_COUNT; i++) {
            valid = tt_minutes_valid(minutes[i]) && tt_warn1_index(warn1[i], &index) &&
                    tt_warn2_index(warn2[i], &index);
        }
        if(valid) {
            for(size_t i = 0; i < TT_PRESET_COUNT; i++) {
                settings->presets[i].minutes = minutes[i];
                settings->presets[i].warn1_seconds = warn1[i];
                settings->presets[i].warn2_seconds = warn2[i];
            }
        }
        FURI_LOG_I(TAG, "Einstellungen geladen");
    } while(false);

    furi_string_free(header);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

bool tt_settings_save(const TtSettings* settings) {
    furi_assert(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* Ordner anlegen, falls die App zum ersten Mal läuft */
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, TT_SETTINGS_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, TT_SETTINGS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, TT_SETTINGS_HEADER, TT_SETTINGS_VERSION)) break;

        uint32_t value = settings->conn;
        if(!flipper_format_write_uint32(ff, KEY_CONN, &value, 1)) break;
        value = settings->profile;
        if(!flipper_format_write_uint32(ff, KEY_PROFILE, &value, 1)) break;
        value = settings->lang;
        if(!flipper_format_write_uint32(ff, KEY_LANG, &value, 1)) break;
        value = settings->talk_minutes;
        if(!flipper_format_write_uint32(ff, KEY_MINUTES, &value, 1)) break;
        value = settings->warn1_seconds;
        if(!flipper_format_write_uint32(ff, KEY_WARN1, &value, 1)) break;
        value = settings->warn2_seconds;
        if(!flipper_format_write_uint32(ff, KEY_WARN2, &value, 1)) break;
        value = settings->overtime_seconds;
        if(!flipper_format_write_uint32(ff, KEY_OVERTIME, &value, 1)) break;
        value = settings->end_alert;
        if(!flipper_format_write_uint32(ff, KEY_END, &value, 1)) break;
        value = settings->sound;
        if(!flipper_format_write_uint32(ff, KEY_SOUND, &value, 1)) break;
        value = settings->keep_display;
        if(!flipper_format_write_uint32(ff, KEY_DISPLAY, &value, 1)) break;
        value = settings->autostart;
        if(!flipper_format_write_uint32(ff, KEY_AUTO, &value, 1)) break;
        uint32_t minutes[TT_PRESET_COUNT];
        uint32_t warn1[TT_PRESET_COUNT];
        uint32_t warn2[TT_PRESET_COUNT];
        for(size_t i = 0; i < TT_PRESET_COUNT; i++) {
            minutes[i] = settings->presets[i].minutes;
            warn1[i] = settings->presets[i].warn1_seconds;
            warn2[i] = settings->presets[i].warn2_seconds;
        }
        if(!flipper_format_write_uint32(ff, KEY_PRESETS, minutes, TT_PRESET_COUNT)) break;
        if(!flipper_format_write_uint32(ff, KEY_PRESETS1, warn1, TT_PRESET_COUNT)) break;
        if(!flipper_format_write_uint32(ff, KEY_PRESETS2, warn2, TT_PRESET_COUNT)) break;
        ok = true;
    } while(false);

    if(!ok) FURI_LOG_E(TAG, "Einstellungen konnten nicht gespeichert werden");

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

const char* tt_conn_name(TtConn conn) {
    switch(conn) {
    case TtConnUsb:
        return "USB";
    case TtConnBt:
        /* Kurz halten: der Wertebereich der VariableItemList ist nur ca. 36 px breit */
        return "BT";
    default:
        return "?";
    }
}
