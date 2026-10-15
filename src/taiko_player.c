#include "pcm_audio.h"
#include "taiko_gpio.h"
#include "taiko_player_view.h"
#include "taiko_sfx.h"
#include "tja_parser.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <stdio.h>
#include <string.h>

#define SONGS_PATH EXT_PATH("apps_data/taiko_player/Songs")

typedef struct {
    Storage* storage;
    DialogsApp* dialogs;
    Gui* gui;
    ViewPort* viewport;
    FuriMessageQueue* input;
    FuriMutex* mutex;
    TaikoGpio* gpio;
    TaikoMenu menu;
    TaikoSfx sfx;
    uint8_t volume;
    TaikoPlayerView view;
} TaikoPlayer;

static void draw(Canvas* canvas, void* context) {
    TaikoPlayer* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    TaikoPlayerView view = app->view;
    furi_mutex_release(app->mutex);
    taiko_player_view_draw(canvas, &view);
}

static void input(InputEvent* event, void* context) {
    TaikoPlayer* app = context;
    if((event->key == InputKeyBack && event->type == InputTypeLong) ||
       event->type == InputTypeShort ||
       (event->type == InputTypeRepeat &&
        (event->key == InputKeyUp || event->key == InputKeyDown))) {
        furi_message_queue_put(app->input, event, 0);
    }
}

/* Keep the original note index (and therefore hand alternation) after a seek. */
static size_t note_at(const TjaChart* chart, uint32_t position) {
    size_t low = 0, high = chart->note_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(chart->notes[mid].time_ms < (int64_t)position)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

static int read_tja_line(void* context, char* line, size_t capacity) {
    Stream* stream = context;
    size_t length = 0;
    uint8_t c;
    while(stream_read(stream, &c, 1) == 1) {
        if(c == '\n') {
            line[length] = '\0';
            return 1;
        }
        if(length + 1 >= capacity) return -1;
        line[length++] = (char)c;
    }
    line[length] = '\0';
    if(buffered_file_stream_get_error(stream) != FSE_OK) return -1;
    return length ? 1 : 0;
}

static void show_error(TaikoPlayer* app, const char* message) {
    DialogMessage* dialog = dialog_message_alloc();
    dialog_message_set_header(dialog, "Cannot play", 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(dialog, message, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(dialog, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, dialog);
    dialog_message_free(dialog);
}

static FuriString* audio_path(const char* tja_path, const char* wave) {
    FuriString* path = furi_string_alloc();
    const char* slash = strrchr(tja_path, '/');
    if(slash) furi_string_set_strn(path, tja_path, slash - tja_path + 1);
    /* Conversion keeps the WAVE basename and replaces its extension with .wav. */
    char name[256];
    snprintf(name, sizeof(name), "%s", wave);
    for(char* p = name; *p; p++) {
        if(*p == '\\') *p = '/';
    }
    char* extension = strrchr(name, '.');
    char* separator = strrchr(name, '/');
    if(extension && (!separator || extension > separator)) *extension = '\0';
    furi_string_cat_str(path, name);
    furi_string_cat_str(path, ".wav");
    return path;
}

/* True means return to the file browser; false means exit the app. */
static bool play_song(TaikoPlayer* app, const char* path) {
    taiko_gpio_reset(app->gpio);
    furi_message_queue_reset(app->input);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memset(&app->view, 0, sizeof(app->view));
    snprintf(app->view.title, sizeof(app->view.title), "Loading chart...");
    app->view.loading = true;
    app->view.volume = app->volume;
    furi_mutex_release(app->mutex);
    view_port_enabled_set(app->viewport, true);
    view_port_update(app->viewport);

    Stream* stream = buffered_file_stream_alloc(app->storage);
    TjaChart chart = {0};
    const char* error = NULL;
    bool loaded = buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(loaded)
        loaded = tja_parse(read_tja_line, stream, &chart, &error);
    else
        error = "Cannot open TJA file";
    buffered_file_stream_close(stream);
    stream_free(stream);
    if(!loaded) {
        show_error(app, error);
        tja_chart_free(&chart);
        return true;
    }

    FuriString* wave = audio_path(path, chart.wave);
    TaikoMixer mixer = {
        .don = app->sfx.don,
        .ka = app->sfx.ka,
        .notes = chart.notes,
        .note_count = chart.note_count,
        .don_volume = app->menu.don_volume,
        .ka_volume = app->menu.ka_volume,
    };
    PcmAudio* audio =
        pcm_audio_start(app->storage, furi_string_get_cstr(wave), 0, &mixer, app->volume);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(
        app->view.title,
        sizeof(app->view.title),
        "%s",
        *chart.title ? chart.title : "Taiko Player");
    snprintf(app->view.course, sizeof(app->view.course), "%s", chart.course);
    furi_mutex_release(app->mutex);
    size_t note = 0;
    bool next_song = false;
    bool outputs_finished = false;
    bool paused = false;
    bool resume_after_menu = false;
    uint32_t paused_position = 0;
    uint32_t duration = 0;
    const uint32_t frame_ticks = furi_ms_to_ticks(20);
    uint32_t next_frame = furi_get_tick();
    while(true) {
        PcmAudioState state = paused ? PcmAudioPaused : pcm_audio_state(audio);
        if(state == PcmAudioError) {
            error = pcm_audio_error(audio);
            next_song = true;
            break;
        }
        if(state == PcmAudioFinished && !outputs_finished) {
            taiko_gpio_reset(app->gpio);
            outputs_finished = true;
        }
        uint32_t position = paused ? paused_position : pcm_audio_position_ms(audio);
        if(audio) duration = pcm_audio_duration_ms(audio);
        if(state == PcmAudioPlaying) {
            while(note < chart.note_count && chart.notes[note].time_ms <= (int32_t)position) {
                bool right = (note % 2) == 0;
                TjaNote event = chart.notes[note++];
                if(event.time_ms < 0 || (int32_t)position - event.time_ms > 120) continue;
                bool rim = event.kind == 2 || event.kind == 4;
                taiko_gpio_pulse(app->gpio, right, rim);
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->view.hit_until = furi_get_tick() + furi_ms_to_ticks(TAIKO_HIT_MS);
                app->view.hit_right = right;
                app->view.hit_rim = rim;
                furi_mutex_release(app->mutex);
            }
        }
        uint32_t now = furi_get_tick();
        if((int32_t)(now - next_frame) >= 0) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->view.loading = state == PcmAudioLoading;
            app->view.finished = state == PcmAudioFinished;
            app->view.paused = paused;
            app->view.volume = app->volume;
            app->view.menu = app->menu;
            app->view.position_ms = position;
            if(state == PcmAudioFinished) app->view.hit_until = 0;
            app->view.lane_count = taiko_lane_collect(
                chart.notes,
                chart.note_count,
                position,
                app->view.lane_notes,
                TAIKO_LANE_MAX_NOTES);
            furi_mutex_release(app->mutex);
            view_port_update(app->viewport);
            next_frame += frame_ticks;
            /* Skip missed refresh slots instead of submitting a burst of old frames. */
            if((int32_t)(now - next_frame) >= 0) next_frame = now + frame_ticks;
        }

        InputEvent event;
        if(furi_message_queue_get(app->input, &event, furi_ms_to_ticks(2)) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeLong) break;
            if(!app->menu.open && (event.key == InputKeyUp || event.key == InputKeyDown)) {
                if(event.key == InputKeyUp && app->volume < 100) app->volume += 10;
                if(event.key == InputKeyDown && app->volume > 0) app->volume -= 10;
                if(audio) pcm_audio_set_volume(audio, app->volume);
                next_frame = furi_get_tick();
                continue;
            }
            if(state == PcmAudioLoading) continue;
            if(event.key == InputKeyBack || app->menu.open) {
                if(!app->menu.open) {
                    resume_after_menu = state == PcmAudioPlaying;
                    taiko_gpio_reset(app->gpio);
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->view.hit_until = 0;
                    furi_mutex_release(app->mutex);
                    if(resume_after_menu) {
                        paused_position = pcm_audio_position_ms(audio);
                        pcm_audio_free(audio);
                        audio = NULL;
                        paused = true;
                    }
                    taiko_menu_open(&app->menu);
                } else {
                    TaikoMenuAction action = TaikoMenuActionNone;
                    if(event.key == InputKeyBack) {
                        app->menu.open = false;
                        action = TaikoMenuActionReturn;
                    } else if(event.key == InputKeyUp || event.key == InputKeyDown) {
                        taiko_menu_move(&app->menu, event.key == InputKeyDown);
                    } else if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                        taiko_menu_adjust(&app->menu, event.key == InputKeyRight);
                    } else if(event.key == InputKeyOk) {
                        action = taiko_menu_activate(&app->menu);
                    }
                    mixer.don_volume = app->menu.don_volume;
                    mixer.ka_volume = app->menu.ka_volume;
                    if(action == TaikoMenuActionExit) break;
                    if(action == TaikoMenuActionRestart) {
                        if(audio) pcm_audio_free(audio);
                        audio = pcm_audio_start(
                            app->storage, furi_string_get_cstr(wave), 0, &mixer, app->volume);
                        paused = false;
                        note = 0;
                        outputs_finished = false;
                    } else if(action == TaikoMenuActionReturn && resume_after_menu) {
                        audio = pcm_audio_start(
                            app->storage,
                            furi_string_get_cstr(wave),
                            paused_position,
                            &mixer,
                            app->volume);
                        paused = false;
                    }
                }
                next_frame = furi_get_tick();
                continue;
            }
            if(event.key != InputKeyOk && event.key != InputKeyLeft && event.key != InputKeyRight)
                continue;
            position = paused ? paused_position : pcm_audio_position_ms(audio);
            taiko_gpio_reset(app->gpio);
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->view.hit_until = 0;
            furi_mutex_release(app->mutex);
            if(event.key == InputKeyOk) {
                if(paused) {
                    audio = pcm_audio_start(
                        app->storage, furi_string_get_cstr(wave), position, &mixer, app->volume);
                    paused = false;
                } else if(state == PcmAudioFinished) {
                    pcm_audio_free(audio);
                    audio = pcm_audio_start(
                        app->storage, furi_string_get_cstr(wave), 0, &mixer, app->volume);
                    note = 0;
                } else {
                    paused_position = position;
                    pcm_audio_free(audio);
                    audio = NULL;
                    paused = true;
                }
            } else {
                int64_t step = event.key == InputKeyLeft ?
                                   -(int64_t)taiko_menu_backward_ms(&app->menu) :
                                   taiko_menu_forward_ms(&app->menu);
                int64_t target = (int64_t)position + step;
                if(target < 0) target = 0;
                if(target > duration) target = duration;
                if(audio) pcm_audio_free(audio);
                audio = NULL;
                paused_position = (uint32_t)target;
                if(!paused)
                    audio = pcm_audio_start(
                        app->storage,
                        furi_string_get_cstr(wave),
                        paused_position,
                        &mixer,
                        app->volume);
                note = note_at(&chart, paused_position);
            }
            outputs_finished = false;
            next_frame = furi_get_tick();
        }
    }
    taiko_gpio_reset(app->gpio);
    if(audio) pcm_audio_free(audio);
    furi_string_free(wave);
    tja_chart_free(&chart);
    if(error) show_error(app, error);
    return next_song;
}

int32_t taiko_player_app(void* context) {
    UNUSED(context);
    TaikoPlayer* app = malloc(sizeof(TaikoPlayer));
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->gui = furi_record_open(RECORD_GUI);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gpio = taiko_gpio_alloc();
    app->sfx = taiko_sfx_load(app->storage);
    taiko_menu_init(&app->menu);
    app->volume = 70;
    memset(&app->view, 0, sizeof(app->view));
    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, draw, app);
    view_port_input_callback_set(app->viewport, input, app);
    view_port_enabled_set(app->viewport, false);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    storage_simply_mkdir(app->storage, EXT_PATH("apps_data/taiko_player"));
    storage_simply_mkdir(app->storage, SONGS_PATH);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_display_backlight_enforce_on);
    FuriString* path = furi_string_alloc_set(SONGS_PATH);
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".tja", NULL);
    options.base_path = EXT_PATH("");
    options.hide_ext = false;
    while(true) {
        view_port_enabled_set(app->viewport, false);
        if(!dialog_file_browser_show(app->dialogs, path, path, &options)) break;
        if(!play_song(app, furi_string_get_cstr(path))) break;
    }
    furi_string_free(path);
    taiko_gpio_free(app->gpio);
    taiko_sfx_free(&app->sfx);
    view_port_enabled_set(app->viewport, false);
    gui_remove_view_port(app->gui, app->viewport);
    view_port_free(app->viewport);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    notification_message(notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
