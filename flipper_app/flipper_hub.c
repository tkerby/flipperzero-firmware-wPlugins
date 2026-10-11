#include "hub_http.h"
#include "hub_git.h"
#include "hub_gitobj.h"
#include "hub_req.h"
#include "hub_plugin.h"
#include "hub_transport.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "hub_mem.h"
#include "gitphin_logo_xbm.h"

typedef struct {
    HubStats stats;
    volatile bool running;
    volatile bool usb_ready;
    /* screen animation: the frame moves only while the worker heartbeat moves */
    uint32_t last_beat;
    uint32_t frames; /* redraw counter */
    uint8_t spin;
} FlipperHub;

/* 8-dot spinner around (cx, cy): bright head, two trail dots. */
static void hub_draw_spinner(Canvas* canvas, int cx, int cy, uint8_t pos) {
    static const int8_t dx[8] = {0, 4, 5, 4, 0, -4, -5, -4};
    static const int8_t dy[8] = {-5, -4, 0, 4, 5, 4, 0, -4};
    canvas_draw_disc(canvas, cx + dx[pos & 7], cy + dy[pos & 7], 1);
    canvas_draw_dot(canvas, cx + dx[(pos + 7) & 7], cy + dy[(pos + 7) & 7]);
    canvas_draw_dot(canvas, cx + dx[(pos + 6) & 7], cy + dy[(pos + 6) & 7]);
}

/* The worker owns the transport: CDC callbacks wake the thread that made it. */
static int32_t hub_worker(void* ctx) {
    FlipperHub* hub = ctx;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    HubTransport* t = hub_transport_alloc();
    hub->usb_ready = true;
    hub_beat();
    hub_gitobj_beat = hub_beat_hook; /* the object reader keeps the heartbeat alive */
    hub_git_cleanup_temp(storage); /* leftovers of an interrupted write */
    hub_git_mark_all(storage);
    uint32_t last_conn = furi_get_tick();

    while(hub->running) {
        hub_beat();
        hub_plugin_tick();
        if(hub_transport_accept(t, 200)) {
            hub_http_handle(t, storage, &hub->stats);
            last_conn = furi_get_tick();
        } else if(furi_get_tick() - last_conn >= 10000) {
            hub_git_idle_tick(storage); /* one short batch, then look for a request again */
        }
    }

    hub_gitobj_beat = NULL;
    hub_git_flush(storage); /* no stale info/refs after a replug */
    hub_git_free_state();
    hub_plugin_unload();
    hub_transport_free(t);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

/* Centered text with its baseline at y. */
static void hub_ctr(Canvas* c, int y, const char* s) {
    canvas_draw_str(c, 64 - canvas_string_width(c, s) / 2, y, s);
}

/* Right-aligned at x 124, baseline y. */
static void hub_rt(Canvas* c, int y, const char* s) {
    canvas_draw_str(c, 124 - canvas_string_width(c, s), y, s);
}

/* Shorten to fit 124 px: "GET /git/very/lon..." */
static void hub_fit(Canvas* c, char* s) {
    size_t n = strlen(s);
    while(n > 4 && canvas_string_width(c, s) > 124) {
        s[--n] = '\0';
        memcpy(s + n - 3, "...", 3);
    }
}

/* Bar x 4..123, h px high. total 0 = a block that bounces (unknown size). */
static void hub_bar(Canvas* c, int y, int h, uint32_t done, uint32_t total, uint32_t now) {
    canvas_draw_frame(c, 4, y, 120, h);
    if(total) {
        if(done > total) done = total;
        canvas_draw_box(c, 5, y + 1, (uint32_t)(118.0f * done / total), h - 2);
    } else {
        uint32_t p = (now / 12) % 188; /* 2 x 94 */
        if(p > 94) p = 188 - p;
        canvas_draw_box(c, 5 + p, y + 1, 24, h - 2);
    }
}

/*
 * Screen 128x64, black on white. FontPrimary caps 8 px, FontSecondary 7 px.
 * Baselines: header bar y0..11 (title baseline 9); state 23; then either
 *   idle:     url 32, last route 41, line y44, "Req" / "Heap" 53, hint 62
 *   transfer: bar y27..34, thin bar y37..39, "N obj, X KB" + rate 48, route 58
 */
static void hub_draw(Canvas* canvas, void* ctx) {
    FlipperHub* hub = ctx;

    uint32_t requests;
    char last[sizeof(hub->stats.last)];
    requests = 0;
    last[0] = '\0';
    if(furi_mutex_acquire(hub->stats.lock, 100) == FuriStatusOk) {
        requests = hub->stats.requests;
        strlcpy(last, hub->stats.last, sizeof(last));
        furi_mutex_release(hub->stats.lock);
    }
    /* alive or stuck: judged from the worker heartbeat, not from the GUI timer */
    uint32_t now = furi_get_tick();
    uint32_t beat = hub_beat_tick;
    uint32_t age = now - beat;
    uint8_t act = hub_activity;
    bool stuck = hub->usb_ready && (age > 60000 || (age > 10000 && !hub_long_op));
    if(beat != hub->last_beat) {
        hub->last_beat = beat;
        /* idle: slow (every 2nd redraw), busy: every redraw */
        if(act != HUB_ACT_IDLE || (hub->frames & 1)) hub->spin++;
    } /* no new beat: the spinner stays where it is */
    hub->frames++;
    static bool stuck_logged;
    if(stuck && !stuck_logged) FURI_LOG_E("FlipperHub", "worker stuck: no heartbeat for %lu s", (unsigned long)(age / 1000));
    stuck_logged = stuck;

    /* progress snapshot (the worker writes it without a lock) */
    uint8_t kind = hub_prog.kind;
    uint32_t done = hub_prog.done, total = hub_prog.total, objs = hub_prog.objects;
    uint32_t pstart = hub_prog.start, plast = hub_prog.last;
    bool big = kind == HUB_PG_BIG;
    uint32_t kb = (hub_prog.bytes + (big ? 0 : done)) >> 10;
    bool live = act == HUB_ACT_REQUEST || now - plast < 500;
    bool show = !stuck && kind != HUB_PG_NONE && (act == HUB_ACT_REQUEST || now - plast < 2500);

    char line[32];
    canvas_clear(canvas);
    canvas_draw_box(canvas, 0, 0, 128, 12); /* header bar */
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_xbm(canvas, 2, 1, GITPHIN_LOGO_W, GITPHIN_LOGO_H, gitphin_logo_xbm);
    canvas_draw_str(canvas, 17, 9, "Gitphin");
    if(hub->usb_ready) hub_draw_spinner(canvas, 119, 6, hub->spin);
    canvas_set_color(canvas, ColorBlack);

    /* state */
    const char* st;
    if(!hub->usb_ready) st = "Starting USB";
    else if(stuck) st = "STUCK";
    else if(show) st = live ? (kind == HUB_PG_PUSH ? "Pushing" : (kind == HUB_PG_PULL ? "Pulling" : "Transfer")) : "Done";
    else if(act == HUB_ACT_PACK) st = "Packing";
    else if(act == HUB_ACT_REQUEST) st = "Busy";
    else st = requests ? "Ready" : "Waiting for PC";
    if(stuck) {
        canvas_draw_box(canvas, 30, 14, 68, 12); /* inverted */
        canvas_set_color(canvas, ColorWhite);
    }
    if(show) canvas_draw_str(canvas, 4, 23, st);
    else hub_ctr(canvas, 23, st);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    strlcpy(line, last[0] ? last : "-", sizeof(line));
    if(show) {
        /* pull of a packed repo: the whole clone; otherwise the current file */
        uint32_t stot = hub_prog.sess_total;
        bool whole = kind == HUB_PG_PULL && stot > 0;
        uint32_t bd = whole ? MIN(hub_prog.sess_done + done, stot) : done;
        uint32_t bt = whole ? stot : total;
        bool ind = live && !bt;
        if(live && !ind) {
            snprintf(line, sizeof(line), "%lu%%", (unsigned long)(100.0f * bd / bt));
            canvas_set_font(canvas, FontPrimary);
            hub_rt(canvas, 23, line);
            canvas_set_font(canvas, FontSecondary);
        }
        hub_bar(canvas, 27, 8, live ? bd : 1, ind ? 0 : (live ? bt : 1), now);
        if(whole && live && total > 8192) hub_bar(canvas, 37, 3, done, total, now);
        const char* pre = live ? "" : "Done: ";
        if(big && live) {
            snprintf(line, sizeof(line), "%lu of %lu KB", (unsigned long)(done >> 10), (unsigned long)(total >> 10));
        } else if(big) {
            snprintf(line, sizeof(line), "Done: %lu KB", (unsigned long)kb);
        } else {
            snprintf(line, sizeof(line), "%s%lu %s, %lu KB", pre, (unsigned long)objs, kind == HUB_PG_PUSH ? "obj" : "files", (unsigned long)kb);
        }
        uint32_t ms = (live ? now : plast) - pstart;
        if(live && ms >= 1000 && !big) {
            canvas_draw_str(canvas, 4, 48, line);
            snprintf(line, sizeof(line), "%lu KB/s", (unsigned long)(kb * 1000 / ms));
            hub_rt(canvas, 48, line);
        } else {
            hub_ctr(canvas, 48, line);
        }
        strlcpy(line, last, sizeof(line));
        hub_fit(canvas, line);
        hub_ctr(canvas, 58, line);
    } else {
        if(hub->usb_ready) hub_ctr(canvas, 32, "localhost:8080");
        hub_fit(canvas, line);
        hub_ctr(canvas, 41, line);
        canvas_draw_line(canvas, 4, 44, 123, 44);
        snprintf(line, sizeof(line), "Req %lu", (unsigned long)requests);
        canvas_draw_str(canvas, 4, 53, line);
        snprintf(line, sizeof(line), "Heap %luK", (unsigned long)(memmgr_get_free_heap() >> 10));
        hub_rt(canvas, 53, line);
        hub_ctr(canvas, 62, "Back: exit");
    }
}

static void hub_input(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 50);
}

/* "loader close" (and so ufbt launch) asks the app to exit: act as if Back was pressed. */
static bool hub_signal(uint32_t signal, void* arg, void* ctx) {
    UNUSED(arg);
    if(signal != FuriSignalExit) return false;
    InputEvent back = {.type = InputTypeShort, .key = InputKeyBack};
    furi_message_queue_put(ctx, &back, 100);
    return true;
}

int32_t flipper_hub_app(void* p) {
    UNUSED(p);

    FlipperHub* hub = malloc(sizeof(FlipperHub));
    if(!hub) return 0; /* not enough memory to start */
    memset(hub, 0, sizeof(FlipperHub));
    hub->stats.lock = furi_mutex_alloc(FuriMutexTypeNormal);
    hub->running = true;

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, hub_draw, hub);
    view_port_input_callback_set(view_port, hub_input, queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    furi_thread_set_signal_callback(furi_thread_get_current(), hub_signal, queue);

    FuriThread* worker = furi_thread_alloc_ex("FlipperHubWorker", 4 * 1024, hub_worker, hub);
    furi_thread_start(worker);

    InputEvent event;
    while(true) {
        if(furi_message_queue_get(queue, &event, 250) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) break;
        }
        view_port_update(view_port);
    }

    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);
    hub->running = false;
    furi_thread_join(worker);
    furi_thread_free(worker);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(hub->stats.lock);
    free(hub);
    return 0;
}
