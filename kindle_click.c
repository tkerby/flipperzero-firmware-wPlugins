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

#define TAG "KindleClick"

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
    const char* name;
    bool consumer; // false = keyboard page
    uint16_t usage;
} KeyDef;

// Linux hid-input maps these to the evdev codes in input_manager's table.
static const KeyDef KEYS[KeyCount] = {
    {"page forward (KEY_PAGEDOWN)", false, 0x4E},
    {"page backward (KEY_PAGEUP)", false, 0x4B},
    {"brightness up (KEY_BRIGHTNESSUP)", true, 0x006F},
    {"brightness down (KEY_BRIGHTNESSDOWN)", true, 0x0070},
    {"sleep (KEY_POWER)", true, 0x0030},
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
    uint32_t ready_at;

    bool down[KeyCount]; // key state the Kindle should see
    uint32_t down_at[KeyCount];
    bool release_wanted[KeyCount];
    bool dirty; // report not yet accepted by the stack

    KeyId pending; // page turn queued until the Kindle is ready
    uint32_t pending_at;
    uint32_t ok_down_at; // OK held: the Kindle sleeps after SLEEP_HOLD_MS
    bool ok_fired;

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
    if(quiet || now - link->secured_at >= READY_FALLBACK_MS) {
        app->ready = true;
        app->ready_at = now;
        uint32_t t0 = link->connected_at;
        FURI_LOG_I(
            TAG,
            "[link] +%lu ms ready for keys (%s): encrypted +%lu, link change %c%lu, %u CCCD writes, last %c%lu",
            now - t0,
            quiet ? "host settled" : "fallback",
            link->secured_at - t0,
            d ? '+' : '-',
            d ? d - t0 : 0,
            link->cccd_writes,
            c ? '+' : '-',
            c ? c - t0 : 0);
    }
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
            FURI_LOG_I(TAG, "[key] %s queued until the Kindle is ready", KEYS[k].name);
        } else {
            FURI_LOG_I(TAG, "[key] %s ignored: not connected", KEYS[k].name);
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
    FURI_LOG_I(TAG, "[key] %s down", KEYS[k].name);
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
            FURI_LOG_I(TAG, "[key] %s up", KEYS[k].name);
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
    if(any) {
        app->dirty = true;
        flush_reports(app);
    }
}

static void service_pending(KindleClick* app) {
    if(app->pending == KeyNone) return;
    if(furi_get_tick() - app->pending_at > PENDING_TTL_MS) {
        FURI_LOG_I(
            TAG,
            "[key] dropped queued %s (the Kindle didn't reconnect: asleep?)",
            KEYS[app->pending].name);
        app->pending = KeyNone;
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
    if(!link_ready(app)) {
        FURI_LOG_I(TAG, "[key] sleep ignored: not connected");
        return;
    }
    tap(app, KeySleep);
}

// ---- Battery ----------------------------------------------------------------
static void notify_battery(KindleClick* app, const char* why) {
    if(!kc_profile_set_battery(app->profile, app->battery)) return;
    app->battery_sent = true;
    app->battery_sent_cccd = app->link->cccd_at;
    app->battery_sent_at = furi_get_tick();
    FURI_LOG_I(TAG, "[bat] %u%% notified (%s)", app->battery, why);
}

static void service_battery(KindleClick* app) {
    uint32_t now = furi_get_tick();
    if(!app->battery_read_at || now - app->battery_read_at >= BATTERY_PERIOD_MS) {
        app->battery_read_at = now | 1;
        uint8_t pct = furi_hal_power_get_pct();
        bool changed = pct != app->battery;
        app->battery = pct;
        if(changed && link_ready(app)) notify_battery(app, "changed");
    }
    if(!link_ready(app)) return;
    // Notify after its CCCD writes settle.
    uint32_t c = app->link->cccd_at;
    const char* why = NULL;
    if(!app->battery_sent)
        why = "connected";
    else if(c != app->battery_sent_cccd && (int32_t)(now - c) >= 1000)
        why = "subscribed";
    else if(now - app->battery_sent_at >= BATTERY_RENOTIFY_MS)
        why = "periodic";
    if(why) notify_battery(app, why);
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
        FURI_LOG_D(TAG, "[ble] advertising restarted");
    }
}

static void forget_kindle(KindleClick* app) {
    FURI_LOG_I(TAG, "[ble] forgetting the Kindle (also Forget the remote on the Kindle)");
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
    };
    if(now.status == app->shown.status && now.queued == app->shown.queued &&
       now.battery == app->shown.battery)
        return;
    app->shown = now;
    with_view_model(app->remote, KcModel * model, { *model = now; }, true);
}

static void remote_draw(Canvas* canvas, void* model_ptr) {
    KcModel* model = model_ptr;
    static const char* const STATUS[][2] = {
        [KcStatusPairing] = {"Pair it in the Kindle's", "Bluetooth devices"},
        [KcStatusWaiting] = {"Waiting for the Kindle", ""},
        [KcStatusConnecting] = {"Connecting...", ""},
        [KcStatusReady] = {"Connected", ""},
    };
    char battery[8];
    snprintf(battery, sizeof(battery), "%u%%", model->battery);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Kindle Click");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignBottom, battery);
    canvas_draw_str(canvas, 0, 23, STATUS[model->status][0]);
    canvas_draw_str(canvas, 0, 33, model->queued ? "Page turn queued" : STATUS[model->status][1]);
    canvas_draw_str(canvas, 0, 46, "Right / Left: page");
    canvas_draw_str(canvas, 0, 55, "Up / Down: light");
    canvas_draw_str(canvas, 0, 64, "Hold OK: Kindle sleeps");
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
        if(event->type == InputTypePress) {
            app->ok_down_at = furi_get_tick() | 1;
            app->ok_fired = false;
        } else if(event->type == InputTypeRelease) {
            app->ok_down_at = 0;
        }
        return true;
    default:
        return false; // Back: to the menu
    }
    if(event->type == InputTypePress)
        key_down(app, key);
    else if(event->type == InputTypeRelease)
        key_up(app, key);
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
    link_ready(app); // logs the moment it becomes ready
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
    FURI_LOG_I(
        TAG,
        "Kindle Click " KC_VERSION " started (%s)",
        app->paired ? "waiting for the Kindle" : "pairing mode");

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
