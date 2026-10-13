#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <cli/cli.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "decoder.h"
#include "art.h"
#include "bridge.h"
#define SAVE_DIR     "/ext/apps_data/megaman"
#define SAVE_PATH    SAVE_DIR "/progress.bin"
#define SAVE_TEMP    SAVE_DIR "/progress.tmp"
#define SAVE_MAGIC   0x4d4d0002U
#define KNOWN_CAP    64
#define XP_PER_LEVEL 10
#define DISPLAY_CAP  (MM_OUTPUT_CAP * 4)
typedef enum {
    PageSplash,
    PageMenu,
    PageResult,
    PageInspect,
    PageLink
} Page;
typedef struct {
    uint32_t magic, total_xp, meals;
    uint32_t hashes[KNOWN_CAP];
    uint32_t next_hash;
} Progress;
typedef struct {
    ViewDispatcher* dispatcher;
    View* home;
    TextInput* keyboard;
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    CliRegistry* cli;
    FuriMutex* lock;
    Page page;
    DecodeMode mode;
    uint8_t frame, splash_ticks, reaction_ticks;
    uint16_t scroll;
    bool editing, save_ok;
    Progress progress;
    MegaBridge bridge;
    uint8_t reaction;
    char input[MM_INPUT_CAP], display[DISPLAY_CAP], message[32], detail[64];
} App;
typedef struct {
    App* app;
} HomeModel;
static const char* names[DecodeCount] = {"Auto chain", "Hex", "Base64", "Brainfuck", "ROT13"};
static void refresh(App* app) {
    view_get_model(app->home);
    view_commit_model(app->home, true);
}
static void load_progress(App* app) {
    app->progress.magic = SAVE_MAGIC;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Progress saved;
        if(storage_file_size(file) == sizeof(saved) &&
           storage_file_read(file, &saved, sizeof(saved)) == sizeof(saved) &&
           saved.magic == SAVE_MAGIC && saved.next_hash < KNOWN_CAP &&
           saved.total_xp <= 100000000U && saved.meals <= 100000000U)
            app->progress = saved;
    }
    storage_file_close(file);
    storage_file_free(file);
}
static bool save_progress(App* app) {
    storage_common_mkdir(app->storage, SAVE_DIR);
    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    /* Save only progression, not input, decoded payloads or event logs. */
    if(storage_file_open(file, SAVE_TEMP, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, &app->progress, sizeof(app->progress)) ==
             sizeof(app->progress);
        if(ok) ok = storage_file_sync(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(ok) ok = storage_common_rename(app->storage, SAVE_TEMP, SAVE_PATH) == FSE_OK;
    return ok;
}
/* USB CLI bridge. The higher total XP wins, preserving fractional progress
 * without ever lowering either level. Register while this app is open only.
 * UseShellThread avoids creating a pending detached command worker.
 */
static void sync_command(PipeSide* pipe, FuriString* args, void* context) {
    App* app = context;
    const char* input = furi_string_get_cstr(args);
    char* end = NULL;
    unsigned long incoming = strtoul(input, &end, 10);
    if(end == input || *end != 0 || incoming > 100000000UL || *input == '-') {
        const char* error = "[MEGAMAN_SYNC] ERROR invalid XP\r\n";
        if(pipe_spaces_available(pipe) >= strlen(error)) pipe_send(pipe, error, strlen(error));
        return;
    }
    furi_mutex_acquire(app->lock, FuriWaitForever);
    if(incoming > app->progress.total_xp) {
        app->progress.total_xp = incoming;
        app->save_ok = save_progress(app);
    } else if(!app->save_ok) {
        app->save_ok = save_progress(app);
    }
    char response[96];
    int length = snprintf(
        response,
        sizeof(response),
        "[MEGAMAN_SYNC] XP=%lu LV=%lu SAVE=%u\r\n",
        (unsigned long)app->progress.total_xp,
        (unsigned long)(1 + app->progress.total_xp / XP_PER_LEVEL),
        app->save_ok ? 1 : 0);
    /* Do not block on a disconnected/full serial pipe while holding state. */
    if(length > 0 && pipe_spaces_available(pipe) >= (size_t)length)
        pipe_send(pipe, response, (size_t)length);
    furi_mutex_release(app->lock);
}

static void format_output(App* app, const DecodeResult* r) {
    size_t used = 0;
    for(size_t i = 0; i < r->length && used + 4 < sizeof(app->display); ++i) {
        uint8_t c = r->bytes[i];
        if(c >= 32 && c <= 126)
            app->display[used++] = c;
        else if(c == '\n')
            app->display[used++] = '\n';
        else if(c == '\r') {
        } else {
            snprintf(app->display + used, sizeof(app->display) - used, "\\x%02X", c);
            used += 4;
        }
    }
    app->display[used] = 0;
    if(!used) strcpy(app->display, "(empty output)");
}
static void run_decode(App* app) {
    DecodeResult* r = malloc(sizeof(DecodeResult));
    app->scroll = 0;
    app->display[0] = app->detail[0] = 0;
    app->page = PageResult;
    app->reaction_ticks = 14;
    app->reaction = 3;
    if(!r) {
        strcpy(app->message, "Not enough memory");
        return;
    }
    /* Concatenate whitespace as the supplied baseline's input collector does. */
    if(app->mode == DecodeAuto || app->mode == DecodeBase64 || app->mode == DecodeHex) {
        size_t out = 0;
        for(size_t i = 0; app->input[i]; ++i)
            if(!isspace((unsigned char)app->input[i])) app->input[out++] = app->input[i];
        app->input[out] = 0;
    }
    mm_decode(app->mode, app->input, r);
    if(!r->success) {
        strcpy(app->message, "Stopped. No XP.");
        snprintf(app->display, sizeof(app->display), "%s", r->error);
        strcpy(app->detail, "Back: choose decoder");
    } else {
        format_output(app, r);
        uint32_t hash = mm_meal_hash(app->mode, app->input);
        bool known = false;
        for(size_t i = 0; i < KNOWN_CAP; ++i)
            if(app->progress.hashes[i] == hash) known = true;
        if(!r->length ||
           (r->length == strlen(app->input) && memcmp(r->bytes, app->input, r->length) == 0))
            strcpy(app->message, "No new data. No XP.");
        else if(known) {
            strcpy(app->message, "Already eaten.");
            app->reaction = 2;
        } else {
            uint32_t old_level = app->progress.total_xp / XP_PER_LEVEL;
            app->progress.hashes[app->progress.next_hash] = hash;
            app->progress.next_hash = (app->progress.next_hash + 1) % KNOWN_CAP;
            if(app->progress.meals < 100000000U) app->progress.meals++;
            uint32_t gain = 2U * r->layers;
            app->progress.total_xp = app->progress.total_xp > 100000000U - gain ?
                                         100000000U :
                                         app->progress.total_xp + gain;
            app->reaction_ticks = 14;
            app->reaction = 1;
            app->save_ok = save_progress(app);
            if(app->progress.total_xp / XP_PER_LEVEL > old_level) {
                strcpy(app->message, "LEVEL UP!");
                app->reaction = 4;
            } else
                snprintf(app->message, sizeof(app->message), "Data eaten! +%u XP", 2 * r->layers);
        }
        snprintf(
            app->detail,
            sizeof(app->detail),
            "%u layer(s), %u bytes",
            r->layers,
            (unsigned)r->length);
    }
    free(r);
}
static unsigned output_lines(const char* text) {
    unsigned lines = 0;
    while(*text) {
        unsigned count = 0;
        while(*text && *text != '\n' && count < 16) {
            text++;
            count++;
        }
        if(*text == '\n') text++;
        lines++;
    }
    return lines;
}
static void draw_lines(Canvas* canvas, const char* text, unsigned scroll) {
    unsigned number = 0, shown = 0;
    while(*text && shown < 3) {
        char line[17];
        unsigned n = 0;
        while(*text && *text != '\n' && n < 16)
            line[n++] = *text++;
        if(*text == '\n') text++;
        line[n] = 0;
        if(number++ >= scroll) canvas_draw_str(canvas, 34, 32 + shown++ * 10, line);
    }
}
static void draw(Canvas* canvas, void* model) {
    App* app = ((HomeModel*)model)->app;
    furi_mutex_acquire(app->lock, FuriWaitForever);
    canvas_clear(canvas);
    if(app->page == PageSplash) {
        canvas_draw_bitmap(canvas, 0, 0, 128, 64, splash);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 52, 128, 12);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 61, "DIGITAL ENIGMA - OK");
    } else {
        const uint8_t sequence[] = {0, 1, 2, 3, 4, 5, 4, 3, 2, 1};
        canvas_draw_bitmap(canvas, 0, 10, 30, 50, walk_frames[sequence[app->frame % 10]]);
        canvas_set_font(canvas, FontSecondary);
        if(app->reaction_ticks) {
            canvas_draw_frame(canvas, 27, 27, 6, 6);
            canvas_draw_str(
                canvas,
                29,
                21,
                app->reaction == 3 ? "!" :
                app->reaction == 2 ? "=" :
                app->reaction == 4 ? "*" :
                                     "+");
        }
        char status[32];
        snprintf(
            status,
            sizeof(status),
            "LV%lu XP%lu/10%s",
            (unsigned long)(1 + app->progress.total_xp / XP_PER_LEVEL),
            (unsigned long)(app->progress.total_xp % XP_PER_LEVEL),
            app->save_ok ? "" : " !");
        canvas_draw_str(canvas, 34, 9, status);
        if(app->page == PageMenu) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 35, 24, names[app->mode]);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 35, 36, "OK: type input");
            canvas_draw_str(canvas, 35, 47, "Right: inspect .txt");
            canvas_draw_str(canvas, 0, 63, "Left: PC job status");
            canvas_draw_str(canvas, 35, 58, "U/D mode Back exit");
        } else if(app->page == PageLink) {
            furi_mutex_acquire(app->bridge.lock, FuriWaitForever);
            const char* states[] = {"Idle", "Running", "Complete", "Error", "Stopped"};
            canvas_draw_str(canvas, 34, 23, states[app->bridge.state]);
            canvas_draw_str(canvas, 34, 35, app->bridge.label);
            char percent[24];
            if(app->bridge.percent >= 0)
                snprintf(percent, sizeof(percent), "%d%%", app->bridge.percent);
            else
                snprintf(percent, sizeof(percent), "Progress unknown");
            canvas_draw_str(canvas, 34, 47, percent);
            canvas_draw_str(canvas, 0, 63, "Desktop USB jobs. Back");
            furi_mutex_release(app->bridge.lock);
        } else {
            canvas_draw_str(canvas, 34, 20, app->message);
            draw_lines(canvas, app->display, app->scroll);
            canvas_draw_str(
                canvas,
                0,
                63,
                app->page == PageInspect ? "OK eat U/D scroll Back" : "U/D scroll L info Back");
        }
    }
    furi_mutex_release(app->lock);
}
static bool input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false;
    if(event->type == InputTypeRepeat && event->key != InputKeyUp && event->key != InputKeyDown)
        return true;
    view_dispatcher_send_custom_event(app->dispatcher, 10 + event->key);
    return true;
}
static void input_done(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, 1);
}
static void open_keyboard(App* app) {
    app->editing = true;
    text_input_reset(app->keyboard);
    text_input_set_header_text(app->keyboard, names[app->mode]);
    text_input_set_minimum_length(app->keyboard, 1);
    text_input_set_result_callback(
        app->keyboard, input_done, app, app->input, sizeof(app->input), false);
    view_dispatcher_switch_to_view(app->dispatcher, 1);
}
static void open_file(App* app) {
    FuriString* path = furi_string_alloc_set("/ext");
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".txt", NULL);
    bool selected = dialog_file_browser_show(app->dialogs, path, path, &options);
    if(selected) {
        File* file = storage_file_alloc(app->storage);
        bool ok = false;
        if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t size = storage_file_size(file);
            if(size && size < sizeof(app->input)) {
                size_t n = storage_file_read(file, app->input, (size_t)size);
                app->input[n] = 0;
                ok = n == size && memchr(app->input, 0, n) == NULL;
            }
        }
        storage_file_close(file);
        storage_file_free(file);
        furi_mutex_acquire(app->lock, FuriWaitForever);
        if(ok) {
            app->page = PageInspect;
            app->scroll = 0;
            snprintf(app->message, sizeof(app->message), "%u bytes", (unsigned)strlen(app->input));
            const char* likely = "Text / unknown";
            size_t compact = 0;
            bool hex = true, b64 = true, bf = true;
            for(size_t i = 0; app->input[i]; i++) {
                unsigned char c = app->input[i];
                if(isspace(c)) continue;
                compact++;
                if(!isxdigit(c)) hex = false;
                if(!isalnum(c) && c != '+' && c != '/' && c != '=') b64 = false;
                if(!strchr("><+-.,[]", c)) bf = false;
            }
            if(bf && compact)
                likely = "Likely Brainfuck";
            else if(hex && compact && !(compact % 2))
                likely = "Likely Hex";
            else if(b64 && compact && compact % 4 != 1)
                likely = "Likely Base64";
            snprintf(
                app->detail, sizeof(app->detail), "%s; selected: %s", likely, names[app->mode]);
            snprintf(app->display, sizeof(app->display), "%s\n%s", likely, app->input);
        } else {
            app->page = PageResult;
            app->scroll = 0;
            strcpy(app->message, "File rejected.");
            strcpy(app->display, "Use text of 1-1024 bytes without NUL bytes.");
        }
        furi_mutex_release(app->lock);
    }
    furi_string_free(path);
    refresh(app);
}
static bool custom(void* context, uint32_t event) {
    App* app = context;
    if(event == 1) {
        app->editing = false;
        furi_mutex_acquire(app->lock, FuriWaitForever);
        run_decode(app);
        furi_mutex_release(app->lock);
        view_dispatcher_switch_to_view(app->dispatcher, 0);
    } else {
        InputKey key = event - 10;
        bool keyboard = false, file = false, info = false;
        furi_mutex_acquire(app->lock, FuriWaitForever);
        if(app->page == PageSplash) {
            app->page = PageMenu;
            app->splash_ticks = 0;
        } else if(app->page == PageMenu) {
            if(key == InputKeyUp)
                app->mode = (app->mode + DecodeCount - 1) % DecodeCount;
            else if(key == InputKeyDown)
                app->mode = (app->mode + 1) % DecodeCount;
            else if(key == InputKeyOk)
                keyboard = true;
            else if(key == InputKeyRight)
                file = true;
            else if(key == InputKeyLeft)
                app->page = PageLink;
        } else {
            if(app->page == PageInspect && key == InputKeyOk)
                run_decode(app);
            else if(key == InputKeyUp && app->scroll)
                app->scroll--;
            else if(key == InputKeyDown && (unsigned)app->scroll + 3U < output_lines(app->display))
                app->scroll++;
            else if(key == InputKeyLeft)
                info = true;
        }
        furi_mutex_release(app->lock);
        if(keyboard) open_keyboard(app);
        if(file) open_file(app);
        if(info) {
            DialogMessage* msg = dialog_message_alloc();
            char summary[160];
            snprintf(
                summary,
                sizeof(summary),
                "%s\n%s\nMeals: %lu\n%s",
                names[app->mode],
                app->detail,
                (unsigned long)app->progress.meals,
                app->save_ok ? "Progress saved" : "Save failed: session only");
            dialog_message_set_text(msg, summary, 2, 3, AlignLeft, AlignTop);
            dialog_message_show(app->dialogs, msg);
            dialog_message_free(msg);
        }
    }
    refresh(app);
    return true;
}
static bool back(void* context) {
    App* app = context;
    furi_mutex_acquire(app->lock, FuriWaitForever);
    bool exit = !app->editing && app->page == PageMenu;
    app->editing = false;
    app->page = PageMenu;
    furi_mutex_release(app->lock);
    if(exit)
        view_dispatcher_stop(app->dispatcher);
    else
        view_dispatcher_switch_to_view(app->dispatcher, 0);
    refresh(app);
    return true;
}
static void tick(void* context) {
    App* app = context;
    furi_mutex_acquire(app->lock, FuriWaitForever);
    app->frame++;
    if(app->reaction_ticks) app->reaction_ticks--;
    if(app->splash_ticks && --app->splash_ticks == 0) app->page = PageMenu;
    furi_mutex_release(app->lock);
    if(!app->editing) refresh(app);
}
int32_t megaman_app(void* context) {
    UNUSED(context);
    App* app = calloc(1, sizeof(App));
    if(!app) return -1;
    app->save_ok = true;
    app->splash_ticks = 24;
    app->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    load_progress(app);
    app->bridge.storage = app->storage;
    app->bridge.lock = furi_mutex_alloc(FuriMutexTypeNormal);
    app->bridge.percent = -1;
    strcpy(app->bridge.label, "No desktop job");
    app->cli = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        app->cli,
        "megaman_sync",
        CliCommandFlagParallelSafe | CliCommandFlagUseShellThread,
        sync_command,
        app);
    cli_registry_add_command(
        app->cli,
        "megaman_files",
        CliCommandFlagParallelSafe | CliCommandFlagUseShellThread,
        mega_files_command,
        &app->bridge);
    cli_registry_add_command(
        app->cli,
        "megaman_read",
        CliCommandFlagParallelSafe | CliCommandFlagUseShellThread,
        mega_read_command,
        &app->bridge);
    cli_registry_add_command(
        app->cli,
        "megaman_job",
        CliCommandFlagParallelSafe | CliCommandFlagUseShellThread,
        mega_job_command,
        &app->bridge);
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custom);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick, 125);
    app->home = view_alloc();
    view_set_context(app->home, app);
    view_allocate_model(app->home, ViewModelTypeLocking, sizeof(HomeModel));
    HomeModel* model = view_get_model(app->home);
    model->app = app;
    view_commit_model(app->home, false);
    view_set_draw_callback(app->home, draw);
    view_set_input_callback(app->home, input);
    app->keyboard = text_input_alloc();
    view_dispatcher_add_view(app->dispatcher, 0, app->home);
    view_dispatcher_add_view(app->dispatcher, 1, text_input_get_view(app->keyboard));
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->dispatcher, 0);
    view_dispatcher_run(app->dispatcher);
    cli_registry_delete_command(app->cli, "megaman_sync");
    cli_registry_delete_command(app->cli, "megaman_files");
    cli_registry_delete_command(app->cli, "megaman_read");
    cli_registry_delete_command(app->cli, "megaman_job");
    /* Drain any in-flight transfer before releasing its storage/context. */
    furi_mutex_acquire(app->bridge.lock, FuriWaitForever);
    furi_mutex_release(app->bridge.lock);
    /* Wait for any in-flight state update before freeing the app. */
    furi_mutex_acquire(app->lock, FuriWaitForever);
    furi_mutex_release(app->lock);
    furi_record_close(RECORD_CLI);
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_dispatcher_remove_view(app->dispatcher, 1);
    text_input_free(app->keyboard);
    view_free(app->home);
    furi_mutex_free(app->bridge.lock);
    view_dispatcher_free(app->dispatcher);
    furi_mutex_free(app->lock);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
