// Kindle Click for Flipper Zero: the Flipper becomes a remote that a Kindle
// accepts as a Kindle Click page turner. The key handling and the ready check
// follow the Open-Kindle-Click-nRF52840 firmware.
//
//   Right / Left   page forward / back
//   Up / Down      light up / down (held: repeats)
//   OK held 1 s    the Kindle sleeps
//   Back           menu (Remote, Buttons, Forget Kindle); Back again exits

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <notification/notification_messages.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>

#include "kc_profile.h"

#define KEYS_PATH    APP_DATA_PATH(".bt_kindle.keys")
#define PAIRED_PATH  APP_DATA_PATH("paired")
#define BUTTONS_PATH APP_DATA_PATH("buttons")

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
    KeyNone = KeyCount,
    KeyActionCount,
} KeyId;

typedef enum {
    BtnRight,
    BtnLeft,
    BtnUp,
    BtnDown,
    BtnOk,
    BtnOkHold,
    BtnCount,
    BtnNone = 0xFF,
} BtnId;

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

static const uint8_t DEFAULT_MAP[BtnCount] = {
    [BtnRight] = KeyNext,
    [BtnLeft] = KeyPrev,
    [BtnUp] = KeyBriUp,
    [BtnDown] = KeyBriDown,
    [BtnOk] = KeyNone,
    [BtnOkHold] = KeySleep,
};

static const char* const BTN_LABELS[BtnCount] = {
    [BtnRight] = "Right",
    [BtnLeft] = "Left",
    [BtnUp] = "Up",
    [BtnDown] = "Down",
    [BtnOk] = "OK",
    [BtnOkHold] = "OK hold",
};

static const char* const KEY_NAMES[KeyActionCount] = {
    [KeyNext] = "Next",
    [KeyPrev] = "Back",
    [KeyBriUp] = "Light+",
    [KeyBriDown] = "Light-",
    [KeySleep] = "Sleep",
    [KeyNone] = "Off",
};

typedef enum {
    KcViewRemote,
    KcViewMenu,
    KcViewButtons,
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
    uint8_t held; // buttons held on the Flipper, as 1 << BtnId
    uint8_t ok_r; // center circle fill radius (0..10) while holding OK
    uint8_t map[BtnCount];
} KcModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* remote;
    Submenu* menu;
    VariableItemList* buttons;
    VariableItem* btn_items[BtnCount];
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

    uint8_t map[BtnCount];
    bool down[KeyCount]; // key state the Kindle should see
    uint32_t down_at[KeyCount];
    bool release_wanted[KeyCount];
    bool dirty; // report not yet accepted by the stack

    KeyId pending; // page turn queued until the Kindle is ready
    uint32_t pending_at;
    uint32_t ok_down_at; // OK held: fires BtnOkHold after SLEEP_HOLD_MS
    bool ok_fired;
    uint8_t held; // buttons held on the Flipper, as 1 << BtnId

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
    app->held = (app->held & ~(1 << BtnOk)) | (1 << BtnOkHold);
    KeyId k = app->map[BtnOkHold];
    if(k == KeySleep) {
        // Never queued: it could put the Kindle to sleep right after it wakes.
        if(link_ready(app)) tap(app, KeySleep);
    } else if(k == KeyBriUp || k == KeyBriDown) {
        key_down(app, k);
    } else if(k < KeyCount) {
        tap(app, k);
    }
}

static void btn_down(KindleClick* app, BtnId btn) {
    KeyId k = app->map[btn];
    if(k == KeySleep) {
        if(link_ready(app)) tap(app, KeySleep);
    } else if(k < KeyCount) {
        key_down(app, k);
    }
}

static void btn_up(KindleClick* app, BtnId btn) {
    KeyId k = app->map[btn];
    if(k >= KeySleep) return;
    for(int b = 0; b < BtnCount; b++) {
        if((app->held & (1 << b)) && app->map[b] == k) return;
    }
    key_up(app, k);
}

// ---- Buttons config ---------------------------------------------------------
static void load_buttons(KindleClick* app) {
    memcpy(app->map, DEFAULT_MAP, sizeof(app->map));
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, BUTTONS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[BtnCount];
        if(storage_file_read(file, buf, sizeof(buf)) == sizeof(buf)) {
            bool ok = true;
            for(int b = 0; b < BtnCount; b++) {
                if(buf[b] >= KeyActionCount) ok = false;
            }
            if(ok) memcpy(app->map, buf, sizeof(app->map));
        }
    }
    storage_file_free(file);
}

static void save_buttons(KindleClick* app) {
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, BUTTONS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, app->map, sizeof(app->map));
    }
    storage_file_free(file);
}

static void sync_button_items(KindleClick* app) {
    for(int b = 0; b < BtnCount; b++) {
        uint8_t k = app->map[b];
        variable_item_set_current_value_index(app->btn_items[b], k);
        variable_item_set_current_value_text(app->btn_items[b], KEY_NAMES[k]);
    }
}

static void button_change_callback(VariableItem* item) {
    KindleClick* app = variable_item_get_context(item);
    for(int b = 0; b < BtnCount; b++) {
        if(app->btn_items[b] == item) {
            uint8_t k = variable_item_get_current_value_index(item);
            app->map[b] = k;
            variable_item_set_current_value_text(item, KEY_NAMES[k]);
            save_buttons(app);
            return;
        }
    }
}

static void buttons_enter_callback(void* context, uint32_t index) {
    KindleClick* app = context;
    if(index < BtnCount) {
        app->map[index] = (app->map[index] + 1) % KeyActionCount;
    } else {
        memcpy(app->map, DEFAULT_MAP, sizeof(app->map));
    }
    sync_button_items(app);
    save_buttons(app);
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
    uint8_t ok_r = 0;
    if(app->ok_down_at) {
        if(app->ok_fired) {
            ok_r = 10;
        } else {
            uint32_t dt = furi_get_tick() - app->ok_down_at;
            ok_r = dt >= SLEEP_HOLD_MS ? 10 : (1 + (dt * 9) / SLEEP_HOLD_MS);
        }
    } else if(app->held & (1 << BtnOk)) {
        ok_r = 10;
    }
    KcModel now = {
        .status = app->ready           ? KcStatusReady :
                  app->link->connected ? KcStatusConnecting :
                  app->paired          ? KcStatusWaiting :
                                         KcStatusPairing,
        .queued = app->pending != KeyNone,
        .battery = app->battery,
        .held = app->held,
        .ok_r = ok_r,
    };
    memcpy(now.map, app->map, sizeof(now.map));
    if(now.status == app->shown.status && now.queued == app->shown.queued &&
       now.battery == app->shown.battery && now.held == app->shown.held &&
       now.ok_r == app->shown.ok_r && memcmp(now.map, app->shown.map, sizeof(now.map)) == 0)
        return;
    app->shown = now;
    with_view_model(app->remote, KcModel * model, { *model = now; }, true);
}

// Glyphs: one byte per row, leftmost pixel in the lowest bit.
static const uint8_t GLYPH_BLUETOOTH[] =
    {0x04, 0x0C, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x0C, 0x04}; // 5x9
static const uint8_t DIGITS_3X5[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, // 0
    {0x2, 0x6, 0x2, 0x2, 0x7}, // 1
    {0x7, 0x1, 0x7, 0x4, 0x7}, // 2
    {0x7, 0x1, 0x7, 0x1, 0x7}, // 3
    {0x5, 0x5, 0x7, 0x1, 0x1}, // 4
    {0x7, 0x4, 0x7, 0x1, 0x7}, // 5
    {0x7, 0x4, 0x7, 0x5, 0x7}, // 6
    {0x7, 0x1, 0x1, 0x1, 0x1}, // 7
    {0x7, 0x5, 0x7, 0x5, 0x7}, // 8
    {0x7, 0x5, 0x7, 0x1, 0x7}, // 9
};

static void draw_digit_3x5(Canvas* canvas, int x, int y, uint8_t d, Color col) {
    canvas_set_color(canvas, col);
    for(int ry = 0; ry < 5; ry++) {
        uint8_t row = DIGITS_3X5[d][ry];
        for(int rx = 0; rx < 3; rx++) {
            if(row & (1 << (2 - rx))) canvas_draw_dot(canvas, x + rx, y + ry);
        }
    }
}

static void draw_battery(Canvas* canvas, int x, int y, uint8_t pct) {
    if(pct > 100) pct = 100;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, 20, 11, 2);
    canvas_draw_box(canvas, x + 20, y + 3, 2, 5);

    int fill = (pct * 16 + 50) / 100;
    if(pct >= 10 && pct < 100) {
        if(fill > 3 && fill < 7)
            fill = 3;
        else if(fill >= 7 && fill <= 10)
            fill = 8;
        else if(fill > 10 && fill < 13)
            fill = 13;
    } else if(pct < 10) {
        if(fill > 5 && fill < 10) fill = 5;
    }
    if(fill > 0) canvas_draw_box(canvas, x + 2, y + 2, fill, 7);

    int ty = y + 3;
    if(pct == 100) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_dot(canvas, x + 6, ty);
        canvas_draw_dot(canvas, x + 5, ty + 1);
        for(int ry = 1; ry < 5; ry++)
            canvas_draw_dot(canvas, x + 6, ty + ry);
        draw_digit_3x5(canvas, x + 8, ty, 0, ColorWhite);
        draw_digit_3x5(canvas, x + 12, ty, 0, ColorWhite);
    } else if(pct >= 10) {
        draw_digit_3x5(canvas, x + 6, ty, pct / 10, fill >= 8 ? ColorWhite : ColorBlack);
        draw_digit_3x5(canvas, x + 11, ty, pct % 10, fill >= 13 ? ColorWhite : ColorBlack);
    } else {
        draw_digit_3x5(canvas, x + 8, ty, pct, fill >= 10 ? ColorWhite : ColorBlack);
    }
    canvas_set_color(canvas, ColorBlack);
}
static const uint8_t ACT_NEXT[] =
    {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01}; // 6x11
static const uint8_t ACT_PREV[] =
    {0x20, 0x30, 0x38, 0x3C, 0x3E, 0x3F, 0x3E, 0x3C, 0x38, 0x30, 0x20}; // 6x11
static const uint8_t ACT_BRI_UP[] = {
    0x40, 0x00, 0x42, 0x08, 0x04, 0x04, 0xE0, 0x00, 0x10, 0x01, 0x08, 0x02, 0x0B,
    0x1A, 0x08, 0x02, 0x10, 0x01, 0xE0, 0x00, 0x04, 0x04, 0x42, 0x08, 0x40, 0x00,
}; // 13x13 (bright sun, 7x7 core, 2px rays)
// clang-format off
static const uint8_t ACT_BRI_DOWN[] = {
    0x10, 0x00, 0x82, 0x00, 0x38, 0x00, 0x44, 0x00, 0x45,
    0x01, 0x44, 0x00, 0x38, 0x00, 0x82, 0x00, 0x10, 0x00,
}; // 9x9 (dim sun, 5x5 core, 1px dot rays)
static const uint8_t ACT_SLEEP[] = {
    0x78, 0x00, 0x46, 0x00, 0x22, 0x00, 0x91, 0x00, 0x51,
    0x01, 0x91, 0x00, 0x22, 0x00, 0x46, 0x00, 0x78, 0x00,
}; // 9x9 (moon + star)
// clang-format on

static void draw_ellipse(Canvas* canvas, int x0, int y0, int rx, int ry, bool fill) {
    int32_t rxrx2 = 2 * rx * rx, ryry2 = 2 * ry * ry;
    int32_t x = rx, y = 0;
    int32_t xchg = (1 - 2 * rx) * ry * ry, ychg = rx * rx, err = 0;
    int32_t stopx = ryry2 * rx, stopy = 0;
    while(stopx >= stopy) {
        if(fill) {
            canvas_draw_box(canvas, x0 - x, y0 - y, 2 * x + 1, 1);
            canvas_draw_box(canvas, x0 - x, y0 + y, 2 * x + 1, 1);
        } else {
            canvas_draw_dot(canvas, x0 + x, y0 + y);
            canvas_draw_dot(canvas, x0 - x, y0 + y);
            canvas_draw_dot(canvas, x0 + x, y0 - y);
            canvas_draw_dot(canvas, x0 - x, y0 - y);
        }
        y++;
        stopy += rxrx2;
        err += ychg;
        ychg += rxrx2;
        if(2 * err + xchg > 0) {
            x--;
            stopx -= ryry2;
            err += xchg;
            xchg += ryry2;
        }
    }
    x = 0;
    y = ry;
    xchg = ry * ry;
    ychg = (1 - 2 * ry) * rx * rx;
    err = 0;
    stopx = 0;
    stopy = rxrx2 * ry;
    while(stopx <= stopy) {
        if(fill) {
            canvas_draw_box(canvas, x0 - x, y0 - y, 2 * x + 1, 1);
            canvas_draw_box(canvas, x0 - x, y0 + y, 2 * x + 1, 1);
        } else {
            canvas_draw_dot(canvas, x0 + x, y0 + y);
            canvas_draw_dot(canvas, x0 - x, y0 + y);
            canvas_draw_dot(canvas, x0 + x, y0 - y);
            canvas_draw_dot(canvas, x0 - x, y0 - y);
        }
        x++;
        stopx += ryry2;
        err += xchg;
        xchg += ryry2;
        if(2 * err + ychg > 0) {
            y--;
            stopy -= rxrx2;
            err += ychg;
            ychg += rxrx2;
        }
    }
}

static void draw_sector_action(Canvas* canvas, int cx, int cy, KeyId k, uint8_t fill_r) {
    static const struct {
        const uint8_t* glyph;
        uint8_t w, h;
    } ACT_GLYPH[KeyCount] = {
        [KeyNext] = {ACT_NEXT, 6, 11},
        [KeyPrev] = {ACT_PREV, 6, 11},
        [KeyBriUp] = {ACT_BRI_UP, 13, 13},
        [KeyBriDown] = {ACT_BRI_DOWN, 9, 9},
        [KeySleep] = {ACT_SLEEP, 9, 9},
    };
    if(fill_r > 0) {
        draw_ellipse(canvas, cx, cy, fill_r + (fill_r == 10 ? 1 : 0), fill_r, true);
    }
    if(k < KeyCount) {
        int w = ACT_GLYPH[k].w, h = ACT_GLYPH[k].h;
        int x0 = cx - w / 2 + (k == KeyNext ? 1 : 0);
        int y0 = cy - h / 2;
        int stride = (w + 7) / 8;
        int r_in = (int)fill_r - 1;
        for(int ry = 0; ry < h; ry++) {
            for(int rx = 0; rx < w; rx++) {
                if(ACT_GLYPH[k].glyph[ry * stride + (rx / 8)] & (1 << (rx % 8))) {
                    int dx = (x0 + rx) - cx, dy = (y0 + ry) - cy;
                    canvas_set_color(
                        canvas,
                        (r_in >= 0 && dx * dx + dy * dy <= r_in * r_in) ? ColorWhite : ColorBlack);
                    canvas_draw_dot(canvas, x0 + rx, y0 + ry);
                }
            }
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void remote_draw(Canvas* canvas, void* model_ptr) {
    KcModel* model = model_ptr;
    static const char* const HOLD_TEXT[KeyCount] = {
        [KeyNext] = "hold: next",
        [KeyPrev] = "hold: back",
        [KeyBriUp] = "hold: light+",
        [KeyBriDown] = "hold: light-",
        [KeySleep] = "hold: sleep",
    };
    canvas_clear(canvas);

    // Left column: Title, BT + Battery, Status, and OK hold legend (1 px margin at x=0, y=0, y=63).
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 1, 9, "Kindle Click");

    bool linked = model->status >= KcStatusConnecting;
    if(linked)
        canvas_draw_rbox(canvas, 1, 13, 9, 11, 2);
    else
        canvas_draw_rframe(canvas, 1, 13, 9, 11, 2);
    canvas_set_color(canvas, linked ? ColorWhite : ColorBlack);
    canvas_draw_xbm(canvas, 3, 14, 5, 9, GLYPH_BLUETOOTH);
    canvas_set_color(canvas, ColorBlack);

    draw_battery(canvas, 12, 13, model->battery);

    canvas_set_font(canvas, FontPrimary);
    if(model->status == KcStatusReady) {
        canvas_draw_str(canvas, 1, 38, "Connected");
    } else if(model->status == KcStatusConnecting) {
        canvas_draw_str(canvas, 1, 38, "Connecting");
    } else if(model->status == KcStatusWaiting) {
        canvas_draw_str(canvas, 1, 36, "Waiting");
        canvas_set_font(canvas, FontSecondary);
        if(!model->queued) {
            canvas_draw_str(canvas, 2, 46, "for Kindle");
            canvas_draw_dot(canvas, 1, 42);
            canvas_draw_dot(canvas, 4, 39);
        }
    } else {
        canvas_draw_str(canvas, 1, 36, "Not paired");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 1, 46, "Pair on Kindle");
    }
    canvas_set_font(canvas, FontSecondary);
    if(model->queued) canvas_draw_str(canvas, 1, 47, "Turn queued");

    KeyId hold_k = model->map[BtnOkHold];
    if(hold_k < KeyCount) {
        canvas_draw_disc(canvas, 5, 57, 2);
        canvas_draw_str(canvas, 12, 61, HOLD_TEXT[hold_k]);
    }

    // Right side: 65x61 D-pad wheel centered at (94, 31), rx=32, ry=30 (inner rx=11, ry=10).
    const int cx = 94, cy = 31;
    draw_ellipse(canvas, cx, cy, 32, 30, false);
    draw_ellipse(canvas, cx, cy, 11, 10, false);
    canvas_draw_line(canvas, cx - 11, cy - 10, cx - 10, cy - 9);
    canvas_draw_line(canvas, cx + 10, cy - 9, cx + 11, cy - 10);
    canvas_draw_line(canvas, cx - 11, cy + 10, cx - 10, cy + 9);
    canvas_draw_line(canvas, cx + 10, cy + 9, cx + 11, cy + 10);
    canvas_draw_line(canvas, cx - 21, cy - 19, cx - 20, cy - 18);
    canvas_draw_line(canvas, cx + 20, cy - 18, cx + 21, cy - 19);
    canvas_draw_line(canvas, cx - 21, cy + 19, cx - 20, cy + 18);
    canvas_draw_line(canvas, cx + 20, cy + 18, cx + 21, cy + 19);

    draw_sector_action(
        canvas, cx, cy - 20, model->map[BtnUp], (model->held & (1 << BtnUp)) ? 8 : 0);
    draw_sector_action(
        canvas, cx, cy + 20, model->map[BtnDown], (model->held & (1 << BtnDown)) ? 8 : 0);
    draw_sector_action(
        canvas, cx - 21, cy, model->map[BtnLeft], (model->held & (1 << BtnLeft)) ? 8 : 0);
    draw_sector_action(
        canvas, cx + 21, cy, model->map[BtnRight], (model->held & (1 << BtnRight)) ? 8 : 0);

    KeyId ok_k = (model->held & (1 << BtnOkHold)) ? model->map[BtnOkHold] :
                 (model->map[BtnOk] < KeyCount)   ? model->map[BtnOk] :
                                                    model->map[BtnOkHold];
    draw_sector_action(canvas, cx, cy, ok_k, model->ok_r);
}

static bool remote_input(InputEvent* event, void* context) {
    KindleClick* app = context;
    BtnId btn;
    switch(event->key) {
    case InputKeyRight:
        btn = BtnRight;
        break;
    case InputKeyLeft:
        btn = BtnLeft;
        break;
    case InputKeyUp:
        btn = BtnUp;
        break;
    case InputKeyDown:
        btn = BtnDown;
        break;
    case InputKeyOk:
        btn = BtnOk;
        break;
    default:
        return false; // Back: to the menu
    }
    if(event->type == InputTypePress) {
        if(btn == BtnOk && app->map[BtnOkHold] != KeyNone) {
            app->held |= 1 << BtnOk;
            app->ok_down_at = furi_get_tick() | 1;
            app->ok_fired = false;
        } else {
            app->held |= 1 << btn;
            btn_down(app, btn);
        }
    } else if(event->type == InputTypeRelease) {
        if(btn == BtnOk && app->map[BtnOkHold] != KeyNone) {
            app->held &= ~((1 << BtnOk) | (1 << BtnOkHold));
            app->ok_down_at = 0;
            if(!app->ok_fired) {
                btn_down(app, BtnOk);
                btn_up(app, BtnOk);
            } else {
                btn_up(app, BtnOkHold);
            }
        } else {
            app->held &= ~(1 << btn);
            btn_up(app, btn);
        }
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
    load_buttons(app);

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
    submenu_add_item(app->menu, "Buttons", KcViewButtons, menu_callback, app);
    submenu_add_item(app->menu, "Forget Kindle", KcViewForget, menu_callback, app);
    view_set_previous_callback(submenu_get_view(app->menu), to_exit);
    view_dispatcher_add_view(app->view_dispatcher, KcViewMenu, submenu_get_view(app->menu));

    app->buttons = variable_item_list_alloc();
    for(int b = 0; b < BtnCount; b++) {
        app->btn_items[b] = variable_item_list_add(
            app->buttons, BTN_LABELS[b], KeyActionCount, button_change_callback, app);
    }
    variable_item_list_add(app->buttons, "Reset defaults", 1, NULL, app);
    variable_item_list_set_enter_callback(app->buttons, buttons_enter_callback, app);
    sync_button_items(app);
    view_set_previous_callback(variable_item_list_get_view(app->buttons), to_menu);
    view_dispatcher_add_view(
        app->view_dispatcher, KcViewButtons, variable_item_list_get_view(app->buttons));

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
    view_dispatcher_remove_view(app->view_dispatcher, KcViewButtons);
    view_dispatcher_remove_view(app->view_dispatcher, KcViewForget);
    view_free(app->remote);
    submenu_free(app->menu);
    variable_item_list_free(app->buttons);
    dialog_ex_free(app->forget);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
