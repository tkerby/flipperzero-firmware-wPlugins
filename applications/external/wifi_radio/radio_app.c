// Flipper Radio — station console (FAP).
//
// Picks an MP3 off the SD card and streams it to the ESP32 devboard, which
// re-broadcasts it as a WiFi station anyone nearby can tune into with a browser.
// Neither chip decodes audio: the Flipper paces bytes at the file's constant
// bitrate and the listener's browser does the rest.
//
// The screens are a ViewDispatcher stack, not one fullscreen viewport. That is
// what makes Back mean "up one level" (player -> menu -> out) instead of
// quitting the app from wherever you happen to be standing.

#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>

#include "mp3_pacer.h"
#include "radio_proto.h"
#include "radio_uart.h"

#define TAG "FlipperRadio"

#define MUSIC_PATH     "/ext/music"
#define TICK_PERIOD_MS 200
#define PING_PERIOD_MS 2000

// No frame of any kind for this long means the board is gone. The ESP sends
// CREDIT every 250 ms unprompted, so three seconds of silence is unambiguous.
#define LINK_TIMEOUT_MS 3000

// The firmware broadcasts "Flipper Radio[ - <suffix>]" and accepts only the
// suffix over the wire -- it re-applies the prefix itself so the station can
// never be renamed into an imitation of someone else's network (ssid_name.h).
// 32-byte SSID limit minus "Flipper Radio" and " - " leaves this much.
#define STATION_SUFFIX_MAX 16
#define STATION_FILE       APP_DATA_PATH("station.txt")

// Screen furniture. The button hints own the bottom ~13 px of the 64 px
// display, so every line of text has to finish above them -- overlapping the
// stats line with the "Track" badge is exactly what made this screen unreadable.
#define SCREEN_W    128
#define ROW_TITLE   11
#define ROW_BAR     16
#define ROW_STATS   33
#define ROW_STATE   44
#define TITLE_MAX_W 108 // leaves room for the link dot in the corner

typedef enum {
    RadioViewMenu,
    RadioViewPlayer,
    RadioViewSettings,
    RadioViewTextInput,
    RadioViewAbout,
} RadioViewId;

typedef enum {
    MenuChooseTrack,
    MenuNowPlaying,
    MenuSettings,
    MenuAbout,
} RadioMenuItem;

typedef enum {
    SettingsStationName,
    SettingsResetName,
} RadioSettingsItem;

// Everything the player screen draws, copied here once per tick so the draw
// callback never has to reach across threads for it.
typedef struct {
    char title[RADIO_META_MAX];
    Mp3PacerState state;
    uint16_t bitrate_kbps;
    uint64_t sent;
    uint64_t total;
    bool starved;
    bool linked;
    bool has_track;
    uint8_t listeners;
    uint8_t buf_percent;
} PlayerModel;

typedef struct {
    ViewDispatcher* views;
    Gui* gui;
    Submenu* menu;
    Submenu* settings;
    TextInput* text_input;
    Widget* about;
    View* player;

    RadioUart* uart;
    Mp3Pacer* pacer;

    uint32_t last_ping;
    char suffix[STATION_SUFFIX_MAX + 1];
    char suffix_edit[STATION_SUFFIX_MAX + 1];
    char menu_label[32];

    FuriMutex* lock;
    // --- guarded by lock ---
    char title[RADIO_META_MAX];
    uint8_t listeners;
    uint8_t buf_percent;
    uint32_t last_rx_tick;
    bool ever_linked;
} RadioApp;

// --- tiny JSON field reader -------------------------------------------------
// The STATS payload is a fixed shape we generate ourselves ({"listeners":N,
// "buf":N}), so a full parser would be far more machinery than the job needs.
// Returns `def` if the key is absent or not followed by digits.
static int json_int(const char* s, const char* key, int def) {
    const char* at = strstr(s, key);
    if(!at) return def;
    at += strlen(key);
    while(*at == '"' || *at == ':' || *at == ' ')
        at++;
    if(*at < '0' || *at > '9') return def;
    int v = 0;
    while(*at >= '0' && *at <= '9')
        v = v * 10 + (*at++ - '0');
    return v;
}

// --- link -------------------------------------------------------------------

// Worker-thread context: keep it short, and never touch the GUI from here.
static void radio_on_frame(uint8_t type, const uint8_t* payload, uint16_t len, void* ctx) {
    RadioApp* app = ctx;

    furi_mutex_acquire(app->lock, FuriWaitForever);
    app->last_rx_tick = furi_get_tick();
    app->ever_linked = true;
    furi_mutex_release(app->lock);

    switch(type) {
    case RADIO_MSG_CREDIT:
        if(len >= 2) {
            const uint16_t credit = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
            mp3_pacer_credit(app->pacer, credit);
        }
        break;

    case RADIO_MSG_STATS: {
        // Arrives as raw bytes with no terminator; make it a string first.
        char json[RADIO_META_MAX];
        size_t n = len < sizeof(json) - 1 ? len : sizeof(json) - 1;
        memcpy(json, payload, n);
        json[n] = '\0';

        const int listeners = json_int(json, "listeners", 0);
        const int buf = json_int(json, "buf", 0);

        furi_mutex_acquire(app->lock, FuriWaitForever);
        app->listeners = (uint8_t)(listeners < 0 ? 0 : (listeners > 255 ? 255 : listeners));
        app->buf_percent = (uint8_t)(buf < 0 ? 0 : (buf > 100 ? 100 : buf));
        furi_mutex_release(app->lock);
        break;
    }

    case RADIO_MSG_PONG:
    default:
        break;
    }
}

static bool radio_link_up(RadioApp* app) {
    furi_mutex_acquire(app->lock, FuriWaitForever);
    const bool up = app->ever_linked &&
                    (furi_get_tick() - app->last_rx_tick) < furi_ms_to_ticks(LINK_TIMEOUT_MS);
    furi_mutex_release(app->lock);
    return up;
}

// --- station name -----------------------------------------------------------

static void station_load(RadioApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    app->suffix[0] = '\0';
    if(storage_file_open(file, STATION_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const uint16_t n = storage_file_read(file, app->suffix, STATION_SUFFIX_MAX);
        app->suffix[n] = '\0';
        // A hand-edited file ends with a newline, and a newline inside an SSID
        // is dropped by the firmware's sanitizer but would still show here.
        for(char* c = app->suffix; *c; c++) {
            if(*c == '\r' || *c == '\n') {
                *c = '\0';
                break;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void station_save(RadioApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    // The app data directory does not exist until something writes to it.
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, STATION_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, app->suffix, strlen(app->suffix));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Push the name to the board and remember it here.
//
// Only sent when it actually changes: applying a name tears the softAP down and
// back up on the ESP side, so re-sending the same one on every launch would
// kick every listener off for nothing.
static void station_apply(RadioApp* app) {
    radio_uart_send(
        app->uart, RADIO_MSG_SET_SSID, (const uint8_t*)app->suffix, (uint16_t)strlen(app->suffix));
    station_save(app);
}

// --- player view ------------------------------------------------------------

static void player_draw(Canvas* canvas, void* model) {
    PlayerModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(!m->linked) {
        canvas_draw_str(canvas, 2, ROW_TITLE, "No board");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Seat the devboard on");
        canvas_draw_str(canvas, 2, 39, "the GPIO header.");
        elements_button_center(canvas, "Track");
        return;
    }

    // Link indicator, top right: filled while the board is answering.
    canvas_draw_disc(canvas, 123, 5, 3);

    switch(m->state) {
    case Mp3PacerErrorOpen:
        canvas_draw_str(canvas, 2, ROW_TITLE, "Cannot open file");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Pick another track.");
        break;

    case Mp3PacerErrorFormat:
        canvas_draw_str(canvas, 2, ROW_TITLE, "Not a CBR MP3");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "MPEG1 Layer III only,");
        canvas_draw_str(canvas, 2, 39, "constant bitrate.");
        break;

    default: {
        if(m->has_track && m->title[0]) {
            FuriString* title = furi_string_alloc_set(m->title);
            elements_string_fit_width(canvas, title, TITLE_MAX_W);
            canvas_draw_str(canvas, 2, ROW_TITLE, furi_string_get_cstr(title));
            furi_string_free(title);
        } else {
            canvas_draw_str(canvas, 2, ROW_TITLE, "On air");
        }

        if(m->total > 0) {
            const float pos = (float)((double)m->sent / (double)m->total);
            elements_progress_bar(canvas, 2, ROW_BAR, 124, pos > 1.0f ? 1.0f : pos);
        }

        canvas_set_font(canvas, FontSecondary);

        char line[32];
        if(m->bitrate_kbps) {
            snprintf(line, sizeof(line), "%u kbps", (unsigned)m->bitrate_kbps);
            canvas_draw_str(canvas, 2, ROW_STATS, line);
        }
        snprintf(line, sizeof(line), "buf %u%%", (unsigned)m->buf_percent);
        canvas_draw_str_aligned(canvas, 126, ROW_STATS, AlignRight, AlignBottom, line);

        snprintf(line, sizeof(line), "%u listening", (unsigned)m->listeners);
        canvas_draw_str(canvas, 2, ROW_STATE, line);

        // One word for what the stream is doing, right where the eye already is
        // after reading the listener count.
        const char* state_word = NULL;
        if(m->state == Mp3PacerPaused) {
            state_word = "paused";
        } else if(m->state == Mp3PacerFinished) {
            state_word = "ended";
        } else if(m->state == Mp3PacerPlaying) {
            state_word = m->starved ? "waiting" : "on air";
        } else if(!m->has_track) {
            state_word = "no track";
        }
        if(state_word) {
            canvas_draw_str_aligned(canvas, 126, ROW_STATE, AlignRight, AlignBottom, state_word);
        }
        break;
    }
    }

    elements_button_center(canvas, "Track");

    // The same button, relabelled -- which is the whole point: whatever stopped
    // the music is also what starts it again, so there is never a state the
    // screen can get into where playing is not one press away.
    if(m->has_track) {
        if(m->state == Mp3PacerPlaying) {
            elements_button_right(canvas, "Pause");
        } else if(m->state == Mp3PacerFinished) {
            elements_button_right(canvas, "Replay");
        } else {
            elements_button_right(canvas, "Play");
        }
    }
}

// Returns true once a track is actually streaming, so the caller can decide
// whether moving to the player screen makes sense.
static bool radio_pick_track(RadioApp* app) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    FuriString* path = furi_string_alloc_set(MUSIC_PATH);

    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".mp3", NULL);
    opts.base_path = MUSIC_PATH;
    opts.hide_ext = false;

    bool started = false;
    if(dialog_file_browser_show(dialogs, path, path, &opts)) {
        const char* chosen = furi_string_get_cstr(path);
        if(mp3_pacer_play(app->pacer, chosen)) {
            // Show the name immediately rather than waiting for the ESP to echo
            // it back on /status — the user just chose it, so it is already known.
            const char* base = chosen;
            for(const char* c = chosen; *c; c++) {
                if(*c == '/') base = c + 1;
            }
            furi_mutex_acquire(app->lock, FuriWaitForever);
            snprintf(app->title, sizeof(app->title), "%s", base);
            furi_mutex_release(app->lock);
            started = true;
        }
    }

    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);
    return started;
}

// One button, every state. Pausing parks the byte offset and playing picks it
// back up, so a pause never costs you the rest of the track.
static void radio_toggle_play(RadioApp* app) {
    Mp3PacerStatus st;
    mp3_pacer_status(app->pacer, &st);

    switch(st.state) {
    case Mp3PacerPlaying:
        mp3_pacer_pause(app->pacer);
        break;
    case Mp3PacerPaused:
        mp3_pacer_resume(app->pacer);
        break;
    case Mp3PacerFinished:
        mp3_pacer_replay(app->pacer);
        break;
    default:
        // Idle with a track behind it (a stream that dropped, say) replays it;
        // with nothing chosen yet there is only one sensible thing to do.
        if(mp3_pacer_has_track(app->pacer)) {
            mp3_pacer_replay(app->pacer);
        } else {
            radio_pick_track(app);
        }
        break;
    }
}

static bool player_input(InputEvent* event, void* ctx) {
    RadioApp* app = ctx;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyOk:
        radio_pick_track(app);
        return true;
    case InputKeyRight:
        radio_toggle_play(app);
        return true;
    default:
        // Back included: unhandled here means the dispatcher walks up to the
        // menu, which is what Back is for.
        return false;
    }
}

// --- screens ----------------------------------------------------------------

// The settings list is rebuilt from inside its own callback, so the two have
// to be introduced to each other first.
static void settings_callback(void* ctx, uint32_t index);
static void settings_rebuild(RadioApp* app);

static uint32_t nav_to_menu(void* ctx) {
    UNUSED(ctx);
    return RadioViewMenu;
}

static uint32_t nav_to_settings(void* ctx) {
    UNUSED(ctx);
    return RadioViewSettings;
}

static uint32_t nav_exit(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

// Rebuilt on entry rather than once at startup: the first item shows the
// current name, so it has to be re-rendered every time the name can have changed.
static void settings_rebuild(RadioApp* app) {
    snprintf(
        app->menu_label,
        sizeof(app->menu_label),
        "Name: %s",
        app->suffix[0] ? app->suffix : "(default)");

    const uint32_t selected = submenu_get_selected_item(app->settings);
    submenu_reset(app->settings);
    submenu_set_header(app->settings, "Settings");
    submenu_add_item(app->settings, app->menu_label, SettingsStationName, settings_callback, app);
    submenu_add_item(app->settings, "Reset name", SettingsResetName, settings_callback, app);
    submenu_set_selected_item(app->settings, selected);
}

static void about_rebuild(RadioApp* app) {
    char text[96];
    snprintf(
        text,
        sizeof(text),
        "Join WiFi:\nFlipper Radio%s%s\n\nThen open\n192.168.4.1\nin any browser.",
        app->suffix[0] ? " - " : "",
        app->suffix[0] ? app->suffix : "");

    widget_reset(app->about);
    widget_add_string_element(app->about, 64, 10, AlignCenter, AlignBottom, FontPrimary, "About");
    widget_add_text_box_element(app->about, 0, 14, 128, 50, AlignLeft, AlignTop, text, false);
}

static void name_entered(void* ctx) {
    RadioApp* app = ctx;
    snprintf(app->suffix, sizeof(app->suffix), "%s", app->suffix_edit);
    station_apply(app);
    settings_rebuild(app);
    view_dispatcher_switch_to_view(app->views, RadioViewSettings);
}

static void settings_callback(void* ctx, uint32_t index) {
    RadioApp* app = ctx;

    switch(index) {
    case SettingsStationName:
        snprintf(app->suffix_edit, sizeof(app->suffix_edit), "%s", app->suffix);
        text_input_set_header_text(app->text_input, "Station name");
        text_input_set_result_callback(
            app->text_input,
            name_entered,
            app,
            app->suffix_edit,
            sizeof(app->suffix_edit),
            false); // keep the current name on screen to edit rather than retype
        view_dispatcher_switch_to_view(app->views, RadioViewTextInput);
        break;

    case SettingsResetName:
        app->suffix[0] = '\0';
        station_apply(app);
        settings_rebuild(app);
        break;

    default:
        break;
    }
}

static void menu_callback(void* ctx, uint32_t index) {
    RadioApp* app = ctx;

    switch(index) {
    case MenuChooseTrack:
        // Only move to the player if a track actually started; a cancelled
        // browser should leave you where you were.
        if(radio_pick_track(app)) view_dispatcher_switch_to_view(app->views, RadioViewPlayer);
        break;

    case MenuNowPlaying:
        view_dispatcher_switch_to_view(app->views, RadioViewPlayer);
        break;

    case MenuSettings:
        settings_rebuild(app);
        view_dispatcher_switch_to_view(app->views, RadioViewSettings);
        break;

    case MenuAbout:
        about_rebuild(app);
        view_dispatcher_switch_to_view(app->views, RadioViewAbout);
        break;

    default:
        break;
    }
}

// --- tick -------------------------------------------------------------------

// Runs on the dispatcher's own thread, so the UART write below is not happening
// on the shared timer service.
static void radio_tick(void* ctx) {
    RadioApp* app = ctx;

    Mp3PacerStatus st;
    mp3_pacer_status(app->pacer, &st);
    const bool linked = radio_link_up(app);
    const bool has_track = mp3_pacer_has_track(app->pacer);

    furi_mutex_acquire(app->lock, FuriWaitForever);
    const uint8_t listeners = app->listeners;
    const uint8_t buf_percent = app->buf_percent;
    char title[RADIO_META_MAX];
    snprintf(title, sizeof(title), "%s", app->title);
    furi_mutex_release(app->lock);

    with_view_model(
        app->player,
        PlayerModel * m,
        {
            snprintf(m->title, sizeof(m->title), "%s", title);
            m->state = st.state;
            m->bitrate_kbps = st.bitrate_kbps;
            m->sent = st.sent;
            m->total = st.total;
            m->starved = st.starved;
            m->linked = linked;
            m->has_track = has_track;
            m->listeners = listeners;
            m->buf_percent = buf_percent;
        },
        true);

    // PING is the only thing that proves the link when no track is playing;
    // without it an idle station could not tell "board absent" from "quiet".
    const uint32_t now = furi_get_tick();
    if(app->uart && (now - app->last_ping) >= furi_ms_to_ticks(PING_PERIOD_MS)) {
        app->last_ping = now;
        radio_uart_send(app->uart, RADIO_MSG_PING, NULL, 0);
    }
}

// --- lifecycle --------------------------------------------------------------

int32_t flipper_wifi_radio_app(void* p) {
    UNUSED(p);

    RadioApp* app = malloc(sizeof(RadioApp));
    memset(app, 0, sizeof(RadioApp));
    app->lock = furi_mutex_alloc(FuriMutexTypeNormal);

    app->uart = radio_uart_open(radio_on_frame, app);
    app->pacer = mp3_pacer_alloc(app->uart);
    station_load(app);

    app->views = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->views, app);
    view_dispatcher_set_tick_event_callback(
        app->views, radio_tick, furi_ms_to_ticks(TICK_PERIOD_MS));

    // Main menu — the screen Back comes home to.
    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "Flipper Radio");
    submenu_add_item(app->menu, "Choose track", MenuChooseTrack, menu_callback, app);
    submenu_add_item(app->menu, "Now playing", MenuNowPlaying, menu_callback, app);
    submenu_add_item(app->menu, "Settings", MenuSettings, menu_callback, app);
    submenu_add_item(app->menu, "About", MenuAbout, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), nav_exit);
    view_dispatcher_add_view(app->views, RadioViewMenu, submenu_get_view(app->menu));

    app->player = view_alloc();
    view_set_context(app->player, app);
    view_allocate_model(app->player, ViewModelTypeLocking, sizeof(PlayerModel));
    view_set_draw_callback(app->player, player_draw);
    view_set_input_callback(app->player, player_input);
    view_set_previous_callback(app->player, nav_to_menu);
    view_dispatcher_add_view(app->views, RadioViewPlayer, app->player);

    app->settings = submenu_alloc();
    settings_rebuild(app);
    view_set_previous_callback(submenu_get_view(app->settings), nav_to_menu);
    view_dispatcher_add_view(app->views, RadioViewSettings, submenu_get_view(app->settings));

    app->text_input = text_input_alloc();
    // Cancelling the keyboard goes back to the settings list, not to the menu.
    view_set_previous_callback(text_input_get_view(app->text_input), nav_to_settings);
    view_dispatcher_add_view(app->views, RadioViewTextInput, text_input_get_view(app->text_input));

    app->about = widget_alloc();
    view_set_previous_callback(widget_get_view(app->about), nav_to_menu);
    view_dispatcher_add_view(app->views, RadioViewAbout, widget_get_view(app->about));

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->views, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->views, RadioViewMenu);

    view_dispatcher_run(app->views);

    // Pacer first: it is still writing to the UART this instant.
    mp3_pacer_free(app->pacer);
    radio_uart_close(app->uart);

    view_dispatcher_remove_view(app->views, RadioViewMenu);
    view_dispatcher_remove_view(app->views, RadioViewPlayer);
    view_dispatcher_remove_view(app->views, RadioViewSettings);
    view_dispatcher_remove_view(app->views, RadioViewTextInput);
    view_dispatcher_remove_view(app->views, RadioViewAbout);
    view_dispatcher_free(app->views);

    submenu_free(app->menu);
    submenu_free(app->settings);
    text_input_free(app->text_input);
    widget_free(app->about);
    view_free(app->player);

    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->lock);
    free(app);
    return 0;
}
