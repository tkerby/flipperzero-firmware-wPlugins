// Kindle Click for Flipper Zero: the Flipper becomes a remote that a Kindle
// accepts as a Kindle Click page turner. The key handling and the ready check
// follow the Open-Kindle-Click-nRF52840 firmware.
//
//   Right / Left   page forward / back
//   Up / Down      light up / down (held: repeats)
//   OK held 1 s    the Kindle sleeps
//   Back           menu (Remote, Forget Kindle); Back again exits

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification_messages.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>

#include "kc_profile.h"

#define KEYS_PATH   APP_DATA_PATH(".bt_kindle.keys")
#define PAIRED_PATH APP_DATA_PATH("paired")

// The Kindle drops a press that comes within 50 ms of the previous press of
// the same key; releases are never filtered. This only keeps press and
// release in separate reports.
#define MIN_PRESS_MS        30
#define SLEEP_HOLD_MS       1000
// A page turn pressed while the Kindle is away is delivered once it is ready.
// One still queued after this was pressed while the Kindle slept.
#define PENDING_TTL_MS      30000
// The Kindle drops reports until it has rediscovered GATT, opened HID and
// recreated uhid. Ready = quiet after the later of its last notification-
// enabling CCCD write and its last link change, else a fallback.
#define READY_QUIET_MS      800
#define READY_FALLBACK_MS   5000
// The Kindle only learns the level from notifications, and only once it has
// subscribed and opened HID.
#define BATTERY_PERIOD_MS   60000
#define BATTERY_RENOTIFY_MS (15 * 60 * 1000)
// The GAP advertises every 80-100 ms for 60 s after each start, then every
// 1-2.5 s. An awake Kindle reconnected after a median 12.5 s at 100 ms, so
// restart before it slows down.
#define ADV_RESTART_MS      55000
#define TICK_MS             10

typedef enum {
    KeyNext,
    KeyPrev,
    KeyBriUp,
    KeyBriDown,
    KeySleep,
    KeyCount,
    KeyNone = 0xFF,
} KeyId;

typedef struct {
    bool consumer; // false = keyboard page
    uint16_t usage;
} KeyDef;

// Linux hid-input maps these to the evdev codes in input_manager's table.
static const KeyDef KEYS[KeyCount] = {
    {false, 0x4E}, // page forward (KEY_PAGEDOWN)
    {false, 0x4B}, // page backward (KEY_PAGEUP)
    {true, 0x006F}, // brightness up (KEY_BRIGHTNESSUP)
    {true, 0x0070}, // brightness down (KEY_BRIGHTNESSDOWN)
    {true, 0x0030}, // sleep (KEY_POWER)
};

typedef enum {
    KcViewRemote,
    KcViewMenu,
    KcViewForget,
} KcViewId;

typedef enum {
    KcStatusPairing,
    KcStatusWaiting,
    KcStatusConnecting,
    KcStatusReady,
} KcStatus;

typedef struct {
    KcStatus status;
    bool queued;
    uint8_t battery;
    uint8_t held; // keys held on the Flipper, as 1 << KeyId
} KcModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* remote;
    Submenu* menu;
    DialogEx* forget;
    NotificationApp* notifications;
    Storage* storage;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    KcLink* link;

    bool paired;
    uint32_t generation; // last connection seen
    bool connected;
    bool ready; // latched per connection

    bool down[KeyCount]; // key state the Kindle should see
    uint32_t down_at[KeyCount];
    bool release_wanted[KeyCount];
    bool dirty; // report not yet accepted by the stack

    KeyId pending; // page turn queued until the Kindle is ready
    uint32_t pending_at;
    uint32_t ok_down_at; // OK held: the Kindle sleeps after SLEEP_HOLD_MS
    bool ok_fired;
    uint8_t held; // keys held on the Flipper, as 1 << KeyId

    uint8_t battery;
    uint32_t battery_read_at;
    bool battery_sent; // notified on this connection
    uint32_t battery_sent_cccd; // link->cccd_at when we notified
    uint32_t battery_sent_at;

    uint32_t adv_since;
    KcModel shown;
} KindleClick;

// ---- Link readiness ---------------------------------------------------------
static bool link_ready(KindleClick* app) {
    KcLink* link = app->link;
    if(!link->connected || !link->secured) return false;
    if(app->ready) return true;
    uint32_t now = furi_get_tick();
    uint32_t c = link->cccd_at, d = link->link_change_at;
    uint32_t last = !c ? d : !d ? c : (int32_t)(c - d) > 0 ? c : d;
    // Signed: a stamp from the BLE thread can be ahead of now.
    bool quiet = last && !link->fast_interval && (int32_t)(now - last) >= READY_QUIET_MS;
    if(quiet || now - link->secured_at >= READY_FALLBACK_MS) app->ready = true;
    return app->ready;
}

// ---- HID reports ------------------------------------------------------------
// The report carries the full key state, so resending the latest one is
// always correct; a refused report is retried on every tick.
static bool send_report(KindleClick* app) {
    uint8_t report[10] = {0}; // modifiers, reserved, 6 keys, consumer usage
    uint8_t n = 2;
    uint16_t usage = 0;
    for(int k = 0; k < KeyCount; k++) {
        if(!app->down[k]) continue;
        if(KEYS[k].consumer)
            usage = KEYS[k].usage; // last one wins
        else if(n < 8)
            report[n++] = (uint8_t)KEYS[k].usage;
    }
    report[8] = usage & 0xFF;
    report[9] = usage >> 8;
    return kc_profile_send_report(app->profile, report);
}

static void flush_reports(KindleClick* app) {
    if(!app->link->connected) {
        app->dirty = false;
        return;
    }
    if(app->dirty && send_report(app)) app->dirty = false;
}

static void key_down(KindleClick* app, KeyId k) {
    if(!link_ready(app)) {
        // Like a real remote: reconnect, then deliver the page turn. Only page
        // turns are worth replaying, and only to a Kindle we are paired with.
        if((k == KeyNext || k == KeyPrev) && app->paired) {
            app->pending = k;
            app->pending_at = furi_get_tick();
            notification_message(app->notifications, &sequence_blink_blue_10);
        }
        return;
    }
    if(app->down[k]) { // pressed again before its release went out
        app->release_wanted[k] = false; // keep holding
        return;
    }
    app->down[k] = true;
    app->down_at[k] = furi_get_tick();
    app->release_wanted[k] = false;
    app->dirty = true;
    flush_reports(app);
    notification_message(app->notifications, &sequence_blink_green_10);
}

static void key_up(KindleClick* app, KeyId k) {
    if(app->down[k]) app->release_wanted[k] = true; // sent by service_releases()
}

static void service_releases(KindleClick* app) {
    for(int k = 0; k < KeyCount; k++) {
        if(app->down[k] && app->release_wanted[k] &&
           furi_get_tick() - app->down_at[k] >= MIN_PRESS_MS) {
            app->down[k] = false;
            app->release_wanted[k] = false;
            app->dirty = true;
        }
    }
    flush_reports(app);
}

static void tap(KindleClick* app, KeyId k) {
    key_down(app, k);
    key_up(app, k);
}

static void release_all(KindleClick* app) {
    bool any = false;
    for(int k = 0; k < KeyCount; k++) {
        if(app->down[k]) any = true;
        app->down[k] = false;
        app->release_wanted[k] = false;
    }
    app->ok_down_at = 0;
    app->held = 0;
    if(any) {
        app->dirty = true;
        flush_reports(app);
    }
}

static void service_pending(KindleClick* app) {
    if(app->pending == KeyNone) return;
    if(furi_get_tick() - app->pending_at > PENDING_TTL_MS) {
        app->pending = KeyNone; // the Kindle didn't come back: asleep?
        return;
    }
    if(link_ready(app)) {
        KeyId k = app->pending;
        app->pending = KeyNone;
        tap(app, k);
    }
}

static void service_sleep_hold(KindleClick* app) {
    if(!app->ok_down_at || app->ok_fired) return;
    if(furi_get_tick() - app->ok_down_at < SLEEP_HOLD_MS) return;
    app->ok_fired = true;
    // Never queued: it could put the Kindle to sleep right after it wakes.
    if(link_ready(app)) tap(app, KeySleep);
}

// ---- Battery ----------------------------------------------------------------
static void notify_battery(KindleClick* app) {
    if(!kc_profile_set_battery(app->profile, app->battery)) return;
    app->battery_sent = true;
    app->battery_sent_cccd = app->link->cccd_at;
    app->battery_sent_at = furi_get_tick();
}

static void service_battery(KindleClick* app) {
    uint32_t now = furi_get_tick();
    if(!app->battery_read_at || now - app->battery_read_at >= BATTERY_PERIOD_MS) {
        app->battery_read_at = now | 1;
        uint8_t pct = furi_hal_power_get_pct();
        bool changed = pct != app->battery;
        app->battery = pct;
        if(changed && link_ready(app)) notify_battery(app);
    }
    if(!link_ready(app)) return;
    // On connecting, once its CCCD writes settle, and now and then.
    uint32_t c = app->link->cccd_at;
    if(!app->battery_sent || (c != app->battery_sent_cccd && (int32_t)(now - c) >= 1000) ||
       now - app->battery_sent_at >= BATTERY_RENOTIFY_MS)
        notify_battery(app);
}

// ---- Link and advertising ---------------------------------------------------
static void set_paired(KindleClick* app, bool paired) {
    if(app->paired == paired) return;
    app->paired = paired;
    if(paired) {
        File* file = storage_file_alloc(app->storage);
        storage_file_open(file, PAIRED_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
        storage_file_free(file);
    } else {
        storage_simply_remove(app->storage, PAIRED_PATH);
    }
}

static void service_link(KindleClick* app) {
    KcLink* link = app->link;
    uint32_t now = furi_get_tick();
    if(link->generation != app->generation || link->connected != app->connected) {
        app->generation = link->generation;
        app->connected = link->connected;
        app->ready = false;
        app->battery_sent = false;
        app->dirty = false;
        for(int k = 0; k < KeyCount; k++) {
            app->down[k] = false;
            app->release_wanted[k] = false;
        }
        // The GAP restarts fast advertising after a disconnect.
        if(!app->connected) app->adv_since = now;
    }
    // Encrypted: the Kindle used our bond, or just made it.
    if(link->connected && link->secured) set_paired(app, true);

    if(!link->connected && now - app->adv_since >= ADV_RESTART_MS) {
        furi_hal_bt_stop_advertising();
        furi_hal_bt_start_advertising();
        app->adv_since = now;
    }
}

static void forget_kindle(KindleClick* app) {
    app->pending = KeyNone;
    release_all(app);
    furi_delay_ms(100); // let the key-up notification go out
    bt_disconnect(app->bt);
    furi_delay_ms(200); // let the 2nd core update its NVM
    bt_forget_bonded_devices(app->bt);
    furi_hal_bt_start_advertising();
    app->adv_since = furi_get_tick();
    set_paired(app, false);
}

// ---- Views ------------------------------------------------------------------
static void update_view(KindleClick* app) {
    KcModel now = {
        .status = app->ready           ? KcStatusReady :
                  app->link->connected ? KcStatusConnecting :
                  app->paired          ? KcStatusWaiting :
                                         KcStatusPairing,
        .queued = app->pending != KeyNone,
        .battery = app->battery,
        .held = app->held,
    };
    if(now.status == app->shown.status && now.queued == app->shown.queued &&
       now.battery == app->shown.battery && now.held == app->shown.held)
        return;
    app->shown = now;
    with_view_model(app->remote, KcModel * model, { *model = now; }, true);
}

// Glyphs: one byte per row, leftmost pixel in the lowest bit.
static const uint8_t GLYPH_LEFT[] = {0x08, 0x0C, 0x0E, 0x0F, 0x0E, 0x0C, 0x08}; // 4x7
static const uint8_t GLYPH_RIGHT[] = {0x01, 0x03, 0x07, 0x0F, 0x07, 0x03, 0x01}; // 4x7
static const uint8_t GLYPH_UP[] = {0x08, 0x1C, 0x3E, 0x7F}; // 7x4
static const uint8_t GLYPH_DOWN[] = {0x7F, 0x3E, 0x1C, 0x08}; // 7x4
static const uint8_t GLYPH_BLUETOOTH[] =
    {0x04, 0x0C, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x0C, 0x04}; // 5x9

// A key in the legend: a 9x9 cell, filled while the key is held. No glyph: OK.
static void draw_key(Canvas* canvas, int x, int y, const uint8_t* glyph, int w, int h, bool held) {
    if(held) {
        canvas_draw_rbox(canvas, x, y, 9, 9, 1);
        canvas_set_color(canvas, ColorWhite);
    }
    if(glyph)
        canvas_draw_xbm(canvas, x + (9 - w) / 2, y + (9 - h) / 2, w, h, glyph);
    else
        canvas_draw_disc(canvas, x + 4, y + 4, 2);
    canvas_set_color(canvas, ColorBlack);
}

static void remote_draw(Canvas* canvas, void* model_ptr) {
    KcModel* model = model_ptr;
    static const char* const STATUS[] = {
        [KcStatusPairing] = "Not paired yet",
        [KcStatusWaiting] = "Waiting for Kindle",
        [KcStatusConnecting] = "Connecting...",
        [KcStatusReady] = "Connected",
    };
    canvas_clear(canvas);

    // Header: the name; Bluetooth, solid while linked; the battery and its level.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Kindle Click");
    canvas_set_font(canvas, FontSecondary);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", model->battery);
    int x = 127 - canvas_string_width(canvas, pct);
    canvas_draw_str(canvas, x, 10, pct);
    x -= 18;
    canvas_draw_frame(canvas, x, 2, 14, 8);
    canvas_draw_box(canvas, x + 14, 4, 2, 4);
    canvas_draw_box(canvas, x + 2, 4, (model->battery * 10 + 50) / 100, 4);
    x -= 12;
    bool linked = model->status >= KcStatusConnecting;
    if(linked)
        canvas_draw_rbox(canvas, x, 0, 9, 11, 2);
    else
        canvas_draw_rframe(canvas, x, 0, 9, 11, 2);
    canvas_set_color(canvas, linked ? ColorWhite : ColorBlack);
    canvas_draw_xbm(canvas, x + 2, 1, 5, 9, GLYPH_BLUETOOTH);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    // Both fonts take 7-8 rows above the baseline and 2 below it.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 25, STATUS[model->status]);
    canvas_set_font(canvas, FontSecondary);
    if(model->status == KcStatusPairing) {
        canvas_draw_str(canvas, 0, 37, "On the Kindle: Bluetooth");
        canvas_draw_str(canvas, 0, 48, "devices > Kindle Click,");
        canvas_draw_str(canvas, 0, 59, "confirm the code on both");
        return;
    }
    if(model->queued) canvas_draw_str(canvas, 0, 36, "Page turn queued");
    draw_key(canvas, 0, 43, GLYPH_LEFT, 4, 7, model->held & (1 << KeyPrev));
    draw_key(canvas, 10, 43, GLYPH_RIGHT, 4, 7, model->held & (1 << KeyNext));
    canvas_draw_str(canvas, 22, 51, "page");
    draw_key(canvas, 66, 43, GLYPH_UP, 7, 4, model->held & (1 << KeyBriUp));
    draw_key(canvas, 76, 43, GLYPH_DOWN, 7, 4, model->held & (1 << KeyBriDown));
    canvas_draw_str(canvas, 88, 51, "light");
    draw_key(canvas, 0, 54, NULL, 0, 0, model->held & (1 << KeySleep));
    canvas_draw_str(canvas, 12, 62, "hold: Kindle sleeps");
}

static bool remote_input(InputEvent* event, void* context) {
    KindleClick* app = context;
    KeyId key;
    switch(event->key) {
    case InputKeyRight:
        key = KeyNext;
        break;
    case InputKeyLeft:
        key = KeyPrev;
        break;
    case InputKeyUp:
        key = KeyBriUp;
        break;
    case InputKeyDown:
        key = KeyBriDown;
        break;
    case InputKeyOk:
        key = KeySleep; // after SLEEP_HOLD_MS
        break;
    default:
        return false; // Back: to the menu
    }
    if(event->type == InputTypePress) {
        app->held |= 1 << key;
        if(key == KeySleep) {
            app->ok_down_at = furi_get_tick() | 1;
            app->ok_fired = false;
        } else {
            key_down(app, key);
        }
    } else if(event->type == InputTypeRelease) {
        app->held &= ~(1 << key);
        if(key == KeySleep)
            app->ok_down_at = 0;
        else
            key_up(app, key);
    }
    update_view(app);
    return true;
}

static void remote_exit(void* context) {
    release_all(context);
}

static uint32_t to_menu(void* context) {
    UNUSED(context);
    return KcViewMenu;
}

static uint32_t to_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void menu_callback(void* context, uint32_t index) {
    KindleClick* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, index);
}

static void forget_callback(DialogExResult result, void* context) {
    KindleClick* app = context;
    if(result == DialogExResultRight) {
        forget_kindle(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, KcViewRemote);
    } else if(result == DialogExResultLeft) {
        view_dispatcher_switch_to_view(app->view_dispatcher, KcViewMenu);
    }
}

static void tick(void* context) {
    KindleClick* app = context;
    service_link(app);
    service_pending(app);
    service_releases(app);
    service_sleep_hold(app);
    service_battery(app);
    link_ready(app); // latches readiness for the status line
    update_view(app);
}

// ---- App --------------------------------------------------------------------
int32_t kindle_click_app(void* p) {
    UNUSED(p);
    KindleClick* app = malloc(sizeof(KindleClick));
    memset(app, 0, sizeof(KindleClick));
    app->pending = KeyNone;
    app->shown.battery = 0xFF; // draw on the first tick

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->bt = furi_record_open(RECORD_BT);
    app->paired = storage_file_exists(app->storage, PAIRED_PATH);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tick, TICK_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->remote = view_alloc();
    view_set_context(app->remote, app);
    view_allocate_model(app->remote, ViewModelTypeLocking, sizeof(KcModel));
    view_set_draw_callback(app->remote, remote_draw);
    view_set_input_callback(app->remote, remote_input);
    view_set_exit_callback(app->remote, remote_exit);
    view_set_previous_callback(app->remote, to_menu);
    view_dispatcher_add_view(app->view_dispatcher, KcViewRemote, app->remote);

    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "Kindle Click");
    submenu_add_item(app->menu, "Remote", KcViewRemote, menu_callback, app);
    submenu_add_item(app->menu, "Forget Kindle", KcViewForget, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), to_exit);
    view_dispatcher_add_view(app->view_dispatcher, KcViewMenu, submenu_get_view(app->menu));

    app->forget = dialog_ex_alloc();
    dialog_ex_set_header(app->forget, "Forget the Kindle?", 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->forget,
        "Then forget the remote on\nthe Kindle too (Bluetooth\ndevices), and pair again.",
        64,
        33,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->forget, "Cancel");
    dialog_ex_set_right_button_text(app->forget, "Forget");
    dialog_ex_set_result_callback(app->forget, forget_callback);
    dialog_ex_set_context(app->forget, app);
    view_set_previous_callback(dialog_ex_get_view(app->forget), to_menu);
    view_dispatcher_add_view(app->view_dispatcher, KcViewForget, dialog_ex_get_view(app->forget));

    // Take over Bluetooth, as the Flipper's own Remote app does.
    bt_disconnect(app->bt);
    furi_delay_ms(200); // let the 2nd core update its NVM
    bt_keys_storage_set_storage_path(app->bt, KEYS_PATH);
    app->profile = bt_profile_start(app->bt, kc_profile, NULL);
    furi_check(app->profile);
    app->link = kc_profile_link(app->profile);
    app->battery = furi_hal_power_get_pct();
    kc_profile_set_battery(app->profile, app->battery);
    furi_hal_bt_start_advertising();
    app->adv_since = furi_get_tick();

    view_dispatcher_switch_to_view(app->view_dispatcher, KcViewRemote);
    view_dispatcher_run(app->view_dispatcher);

    release_all(app);
    furi_delay_ms(100); // let the key-up notification go out
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));

    view_dispatcher_remove_view(app->view_dispatcher, KcViewRemote);
    view_dispatcher_remove_view(app->view_dispatcher, KcViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, KcViewForget);
    view_free(app->remote);
    submenu_free(app->menu);
    dialog_ex_free(app->forget);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
