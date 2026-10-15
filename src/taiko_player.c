#include "pcm_audio.h"
#include "taiko_gpio.h"
#include "tja_parser.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <stdio.h>
#include <string.h>

#define SONGS_PATH EXT_PATH("apps_data/taiko_player/Songs")

typedef struct {
    char title[128];
    char course[24];
    uint32_t position_ms;
    uint32_t hit_until;
    bool hit_right;
    bool hit_rim;
    bool loading;
    bool finished;
} PlayerView;

typedef struct {
    Storage* storage;
    DialogsApp* dialogs;
    Gui* gui;
    ViewPort* viewport;
    FuriMessageQueue* input;
    FuriMutex* mutex;
    TaikoGpio* gpio;
    PlayerView view;
} TaikoPlayer;

/* Leave a pixel of space around the outlines and divider so that the two
 * note types and the active hand remain distinct on the monochrome screen. */
static void draw_hit(Canvas* canvas, bool right, bool rim) {
    const int radius = rim ? 19 : 13;
    const int inner_radius = rim ? 15 : 0;
    for(int y = -radius; y <= radius; y++) {
        int end = radius;
        while(end * end + y * y > radius * radius)
            end--;
        int start = 2;
        while(start <= end && start * start + y * y < inner_radius * inner_radius)
            start++;
        if(start > end) continue;
        int x1 = right ? 64 + start : 64 - end;
        int x2 = right ? 64 + end : 64 - start;
        canvas_draw_line(canvas, x1, 33 + y, x2, 33 + y);
    }
}

static void draw(Canvas* canvas, void* context) {
    TaikoPlayer* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    PlayerView view = app->view;
    furi_mutex_release(app->mutex);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    FuriString* title = furi_string_alloc_set(view.title);
    elements_string_fit_width(canvas, title, 124);
    canvas_draw_str(canvas, 2, 9, furi_string_get_cstr(title));
    furi_string_free(title);

    uint32_t now = furi_get_tick();
    if(view.hit_until && (int32_t)(view.hit_until - now) > 0)
        draw_hit(canvas, view.hit_right, view.hit_rim);
    canvas_draw_circle(canvas, 64, 33, 14);
    canvas_draw_circle(canvas, 64, 33, 20);
    canvas_draw_line(canvas, 64, 13, 64, 53);
    char time[16];
    uint32_t seconds = view.position_ms / 1000;
    snprintf(time, sizeof(time), "%lu:%02lu", seconds / 60, seconds % 60);
    canvas_draw_str(canvas, 1, 32, time);
    FuriString* course = furi_string_alloc_set(view.course);
    elements_string_fit_width(canvas, course, 40);
    canvas_draw_str(canvas, 88, 32, furi_string_get_cstr(course));
    furi_string_free(course);
    if(view.loading || view.finished) {
        canvas_draw_str_aligned(
            canvas, 64, 36, AlignCenter, AlignBottom, view.loading ? "Load" : "End");
    }
    canvas_draw_str_aligned(
        canvas,
        64,
        63,
        AlignCenter,
        AlignBottom,
        view.finished ? "OK: next   Hold Back: exit" : "Hold Back: exit");
}

static void input(InputEvent* event, void* context) {
    TaikoPlayer* app = context;
    if((event->key == InputKeyBack && event->type == InputTypeLong) ||
       (event->key == InputKeyOk && event->type == InputTypeShort)) {
        furi_message_queue_put(app->input, event, 0);
    }
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
    PcmAudio* audio = pcm_audio_start(app->storage, furi_string_get_cstr(wave));
    furi_string_free(wave);
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
    while(true) {
        PcmAudioState state = pcm_audio_state(audio);
        if(state == PcmAudioError) {
            error = pcm_audio_error(audio);
            next_song = true;
            break;
        }
        if(state == PcmAudioFinished && !outputs_finished) {
            taiko_gpio_reset(app->gpio);
            outputs_finished = true;
        }
        uint32_t position = pcm_audio_position_ms(audio);
        uint32_t now = furi_get_tick();
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->view.loading = state == PcmAudioLoading;
        app->view.finished = state == PcmAudioFinished;
        app->view.position_ms = position;
        if(state == PcmAudioFinished) app->view.hit_until = 0;
        if(state == PcmAudioPlaying) {
            while(note < chart.note_count && chart.notes[note].time_ms <= (int32_t)position) {
                bool right = (note % 2) == 0;
                TjaNote event = chart.notes[note++];
                /* Do not flash events from before the audio, or stale events after a stall. */
                if(event.time_ms < 0 || (int32_t)position - event.time_ms > 120) continue;
                app->view.hit_until = now + furi_ms_to_ticks(TAIKO_HIT_MS);
                app->view.hit_right = right;
                app->view.hit_rim = event.kind == 2 || event.kind == 4;
                taiko_gpio_pulse(app->gpio, app->view.hit_right, app->view.hit_rim);
            }
        }
        furi_mutex_release(app->mutex);
        view_port_update(app->viewport);

        InputEvent event;
        if(furi_message_queue_get(app->input, &event, furi_ms_to_ticks(20)) == FuriStatusOk) {
            if(event.key == InputKeyBack) break;
            if(event.key == InputKeyOk && state == PcmAudioFinished) {
                next_song = true;
                break;
            }
        }
    }
    taiko_gpio_reset(app->gpio);
    pcm_audio_free(audio);
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
