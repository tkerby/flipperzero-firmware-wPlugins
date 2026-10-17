/* Kota: Claude plan usage on the Flipper Zero.
 *
 * The Mac host (host/kota.py) connects over the Flipper BLE serial profile and
 * sends one line per update:
 *
 *   U <session_pct> <session_reset_s> <week_pct> <week_reset_s>\n
 *
 * Reset times are seconds from "now"; the app counts them down locally between
 * updates. While connected and not yet fed, the app sends "KOTA <version>\n" every
 * second so the host knows it is talking to Kota and not the default RPC profile.
 */

#include <furi.h>
#include <furi_hal_bt.h>
#include <gui/gui.h>
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>
#include <stdlib.h>
#include <string.h>

#define TAG          "Kota"
#define RX_BUF_SIZE  128
#define HELLO        "KOTA 0.1\n"
#define BAR_W        98
#define BAR_H        9

typedef struct {
    bool has_data;
    uint8_t session_pct;
    uint8_t week_pct;
    uint32_t session_reset_s;
    uint32_t week_reset_s;
    uint32_t received_tick;
} Usage;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    bool ble_ok;
    volatile bool connected;
    volatile bool got_data; /* host sent a line since this connect */
    Usage usage;
    char rx[RX_BUF_SIZE];
    size_t rx_len;
} Kota;

/* ── protocol ─────────────────────────────────────────────── */

static uint32_t clamp_u32(long v, long max) {
    if(v < 0) return 0;
    if(v > max) return (uint32_t)max;
    return (uint32_t)v;
}

static void kota_parse_line(Kota* app, const char* line) {
    if(line[0] != 'U') return;
    const char* p = line + 1;
    long v[4];
    for(int i = 0; i < 4; i++) {
        char* end;
        v[i] = strtol(p, &end, 10);
        if(end == p) {
            FURI_LOG_W(TAG, "malformed line: %s", line);
            return;
        }
        p = end;
    }

    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    app->usage.session_pct = clamp_u32(v[0], 100);
    app->usage.session_reset_s = clamp_u32(v[1], 30L * 24 * 3600);
    app->usage.week_pct = clamp_u32(v[2], 100);
    app->usage.week_reset_s = clamp_u32(v[3], 30L * 24 * 3600);
    app->usage.received_tick = furi_get_tick();
    app->usage.has_data = true;
    furi_mutex_release(app->mutex);
    app->got_data = true;

    view_port_update(app->view_port);
}

/* Runs on the BLE thread. Never call ble_profile_serial_tx() from here: the
 * stack holds the same mutex and it deadlocks. Replies go through the main loop. */
static uint16_t kota_ble_rx(SerialServiceEvent event, void* context) {
    Kota* app = context;
    if(event.event != SerialServiceEventTypeDataReceived) return 0;

    for(uint16_t i = 0; i < event.data.size; i++) {
        char c = (char)event.data.buffer[i];
        if(c == '\n') {
            app->rx[app->rx_len] = '\0';
            if(app->rx_len) kota_parse_line(app, app->rx);
            app->rx_len = 0;
        } else if(app->rx_len < RX_BUF_SIZE - 1) {
            app->rx[app->rx_len++] = c;
        }
    }
    return RX_BUF_SIZE;
}

/* The BT service opens an RPC session on every connect and takes over the
 * serial callback, so take it back here. */
static void kota_bt_status(BtStatus status, void* context) {
    Kota* app = context;
    bool connected = (status == BtStatusConnected);
    if(connected && app->profile) {
        ble_profile_serial_set_event_callback(app->profile, RX_BUF_SIZE, kota_ble_rx, app);
        ble_profile_serial_set_rpc_active(app->profile, false);
        app->rx_len = 0;
    }
    app->got_data = false;
    app->connected = connected;
    view_port_update(app->view_port);
}

static void kota_ble_start(Kota* app) {
    app->bt = furi_record_open(RECORD_BT);
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    app->profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(!app->profile) {
        FURI_LOG_E(TAG, "serial profile failed to start");
        return;
    }
    ble_profile_serial_set_event_callback(app->profile, RX_BUF_SIZE, kota_ble_rx, app);
    ble_profile_serial_set_rpc_active(app->profile, false);
    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(app->bt, kota_bt_status, app);
    app->ble_ok = true;
}

static void kota_ble_stop(Kota* app) {
    if(app->profile) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_profile_restore_default(app->bt);
        app->profile = NULL;
    }
    furi_record_close(RECORD_BT);
}

/* ── drawing ──────────────────────────────────────────────── */

static void format_duration(char* out, size_t size, uint32_t s) {
    uint32_t m = s / 60;
    if(m >= 24 * 60) {
        snprintf(out, size, "%lud %luh", (unsigned long)(m / 1440), (unsigned long)((m % 1440) / 60));
    } else if(m >= 60) {
        snprintf(out, size, "%luh %02lum", (unsigned long)(m / 60), (unsigned long)(m % 60));
    } else if(m > 0) {
        snprintf(out, size, "%lum", (unsigned long)m);
    } else {
        snprintf(out, size, "now");
    }
}

static void draw_sparkle(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x, y - 4, x, y + 4);
    canvas_draw_line(canvas, x - 4, y, x + 4, y);
    canvas_draw_line(canvas, x - 3, y - 3, x + 3, y + 3);
    canvas_draw_line(canvas, x - 3, y + 3, x + 3, y - 3);
}

static void draw_meter(
    Canvas* canvas,
    int y,
    const char* label,
    uint8_t pct,
    uint32_t reset_s) {
    char buf[16];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, y, label);

    format_duration(buf, sizeof(buf), reset_s);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, BAR_W, y, AlignRight, AlignBottom, buf);

    int bar_y = y + 4;
    canvas_draw_rframe(canvas, 0, bar_y, BAR_W, BAR_H, 2);
    int fill = ((BAR_W - 4) * pct) / 100;
    if(fill > 0) canvas_draw_box(canvas, 2, bar_y + 2, fill, BAR_H - 4);
    /* quarter ticks under the bar */
    for(int q = 1; q < 4; q++) {
        canvas_draw_dot(canvas, (BAR_W * q) / 4, bar_y + BAR_H + 1);
    }

    snprintf(buf, sizeof(buf), "%u%%", pct);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 127, bar_y + BAR_H, AlignRight, AlignBottom, buf);
}

static void kota_draw(Canvas* canvas, void* context) {
    Kota* app = context;
    canvas_clear(canvas);

    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
    Usage u = app->usage;
    furi_mutex_release(app->mutex);
    bool connected = app->connected;

    /* header */
    draw_sparkle(canvas, 4, 5);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 12, 9, "Kota");

    if(connected) {
        canvas_draw_disc(canvas, 123, 4, 3);
    } else {
        canvas_draw_circle(canvas, 123, 4, 3);
    }

    uint32_t age_s = u.has_data ? (furi_get_tick() - u.received_tick) / furi_kernel_get_tick_frequency() : 0;
    char status[20];
    if(!app->ble_ok) {
        snprintf(status, sizeof(status), "BT off");
    } else if(!u.has_data) {
        snprintf(status, sizeof(status), connected ? "linked" : "no link");
    } else if(age_s < 60) {
        snprintf(status, sizeof(status), "live");
    } else {
        char d[12];
        format_duration(d, sizeof(d), age_s);
        snprintf(status, sizeof(status), "%s ago", d);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 117, 8, AlignRight, AlignBottom, status);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(!app->ble_ok) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Turn Bluetooth on");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "in Settings, then reopen");
        return;
    }
    if(!u.has_data) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Waiting for Mac");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "run host/kota.py");
        return;
    }

    uint32_t s_left = u.session_reset_s > age_s ? u.session_reset_s - age_s : 0;
    uint32_t w_left = u.week_reset_s > age_s ? u.week_reset_s - age_s : 0;
    draw_meter(canvas, 23, "Session", u.session_pct, s_left);
    draw_meter(canvas, 48, "Weekly", u.week_pct, w_left);
}

static void kota_input(InputEvent* event, void* context) {
    Kota* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

/* ── entry ────────────────────────────────────────────────── */

int32_t kota_app(void* p) {
    UNUSED(p);
    Kota* app = malloc(sizeof(Kota));
    memset(app, 0, sizeof(Kota));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, kota_draw, app);
    view_port_input_callback_set(app->view_port, kota_input, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    kota_ble_start(app);

    InputEvent event;
    while(true) {
        if(furi_message_queue_get(app->input_queue, &event, 1000) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeShort) break;
        }
        /* Repeat the hello until the host answers: it may subscribe a moment after connecting. */
        if(app->connected && !app->got_data && app->profile) {
            ble_profile_serial_tx(app->profile, (uint8_t*)HELLO, strlen(HELLO));
        }
        view_port_update(app->view_port); /* tick the countdowns */
    }

    kota_ble_stop(app);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
