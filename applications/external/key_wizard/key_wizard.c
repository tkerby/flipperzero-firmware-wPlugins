#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "key_types.h"
#include "key_saver.h"
#include "key_scanner.h"
#include "key_wizard_icons.h"

typedef enum {
    ScreenScanning = 0,
    ScreenKeyFound,
    ScreenSaved,
    ScreenQuickKeys,
    ScreenAbout,
} WizardScreen;

typedef struct {
    WizardScreen screen;
    ScanMode scan_mode;
    DetectedKey current_key;
    size_t quick_key_idx;
    uint32_t anim_frame;
    int32_t about_scroll;
} WizardState;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    KeyScanner* scanner;
    WizardState state;
    bool is_running;
} KeyWizardApp;

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} WizardEvent;

static const struct {
    KeyCategory cat;
    const char* title;
    const char* proto;
    const char* uid;
} QUICK_KEYS[] = {
    {KeyCategoryIButton, "Dallas Universal", "Dallas", "FF FF FF FF FF FF FF 01"},
    {KeyCategoryLFRFID, "EM4100 Universal", "EM4100", "00 00 00 00 00"},
    {KeyCategoryIButton, "Cyfral Master", "Cyfral", "01 23 45 67 89 AB CD EF"},
    {KeyCategoryNFC, "Mifare Blank", "Mifare Classic", "DE AD BE EF"},
};
#define QUICK_KEYS_COUNT (sizeof(QUICK_KEYS) / sizeof(QUICK_KEYS[0]))

static void draw_screen_scanning(Canvas* canvas, KeyWizardApp* app) {
    // 1. Sleek Inverted Title Header
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "KEY WIZARD");
    canvas_invert_color(canvas);

    ScanMode mode = app->state.scan_mode;
    KeyCategory phase = key_scanner_get_current_phase(app->scanner);

    // 2. Mode Selector Pill Box (128px centered)
    canvas_draw_rframe(canvas, 8, 14, 112, 12, 3);
    canvas_set_font(canvas, FontSecondary);
    char mode_str[36];
    snprintf(mode_str, sizeof(mode_str), "<  %s  >", scan_mode_to_string(mode));
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignBottom, mode_str);

    // 3. Three Clean Antenna Rows with Radio Wave Animation
    bool rfid_active = (mode == ScanModeRFIDOnly) ||
                       (mode == ScanModeAuto && phase == KeyCategoryLFRFID);
    bool nfc_active = (mode == ScanModeNFCOnly) ||
                      (mode == ScanModeAuto && phase == KeyCategoryNFC);
    bool ibutton_active = (mode == ScanModeIButtonOnly) ||
                          (mode == ScanModeAuto && phase == KeyCategoryIButton);

    const char* wave = (app->state.anim_frame % 4 == 0) ? ")" :
                       (app->state.anim_frame % 4 == 1) ? "))" :
                       (app->state.anim_frame % 4 == 2) ? ")))" :
                                                          ")";

    // --- Row 1: RFID ---
    if(rfid_active) {
        canvas_draw_disc(canvas, 9, 32, 3);
        canvas_draw_str(canvas, 16, 35, "125 kHz RFID");
        canvas_draw_str_aligned(canvas, 120, 35, AlignRight, AlignBottom, wave);
    } else {
        canvas_draw_circle(canvas, 9, 32, 3);
        canvas_draw_str(canvas, 16, 35, "125 kHz RFID");
        canvas_draw_str_aligned(canvas, 120, 35, AlignRight, AlignBottom, "idle");
    }

    // --- Row 2: NFC ---
    if(nfc_active) {
        canvas_draw_disc(canvas, 9, 41, 3);
        canvas_draw_str(canvas, 16, 44, "13.56 MHz NFC");
        canvas_draw_str_aligned(canvas, 120, 44, AlignRight, AlignBottom, wave);
    } else {
        canvas_draw_circle(canvas, 9, 41, 3);
        canvas_draw_str(canvas, 16, 44, "13.56 MHz NFC");
        canvas_draw_str_aligned(canvas, 120, 44, AlignRight, AlignBottom, "idle");
    }

    // --- Row 3: iButton ---
    if(ibutton_active) {
        canvas_draw_disc(canvas, 9, 50, 3);
        canvas_draw_str(canvas, 16, 53, "1-Wire iButton");
        canvas_draw_str_aligned(canvas, 120, 53, AlignRight, AlignBottom, wave);
    } else {
        canvas_draw_circle(canvas, 9, 50, 3);
        canvas_draw_str(canvas, 16, 53, "1-Wire iButton");
        canvas_draw_str_aligned(canvas, 120, 53, AlignRight, AlignBottom, "idle");
    }

    // 4. Subtle Bottom Navigation Bar
    canvas_draw_line(canvas, 0, 55, 128, 55);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "[<>]Mode");
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[v]About");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "[^]Keys");
}

static void draw_screen_key_found(Canvas* canvas, KeyWizardApp* app) {
    DetectedKey* key = &app->state.current_key;

    // Header banner with rounded corners
    canvas_draw_rbox(canvas, 0, 0, 128, 13, 2);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 8, 1, &I_key_icon);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "KEY DETECTED!");
    canvas_draw_icon(canvas, 110, 1, &I_key_icon);
    canvas_invert_color(canvas);

    // Rounded Card Background
    canvas_draw_rframe(canvas, 2, 15, 124, 38, 3);

    // Badge: Category and Protocol
    canvas_set_font(canvas, FontPrimary);
    char badge[48];
    snprintf(
        badge, sizeof(badge), "%s (%s)", key_category_to_string(key->category), key->protocol_name);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, badge);

    canvas_draw_line(canvas, 8, 29, 120, 29);

    // UID Display with Keyboard font
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, "Data / UID:");

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, key->data_hex);

    // Save Action Footer
    canvas_draw_rbox(canvas, 12, 54, 104, 10, 2);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Save to native folder");
    canvas_invert_color(canvas);
}

static void draw_screen_saved(Canvas* canvas, KeyWizardApp* app) {
    DetectedKey* key = &app->state.current_key;

    // Header banner
    canvas_draw_rbox(canvas, 0, 0, 128, 13, 2);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "SAVED TO ASSETS!");
    canvas_invert_color(canvas);

    canvas_draw_rframe(canvas, 2, 15, 124, 38, 3);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, "Stored in native folder:");

    // Show path with monospace
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, key->saved_path);

    canvas_set_font(canvas, FontSecondary);
    const char* hint = "Open via native app!";
    if(key->category == KeyCategoryLFRFID) {
        hint = "Ready in 125 kHz RFID!";
    } else if(key->category == KeyCategoryNFC) {
        hint = "Ready in NFC app!";
    } else if(key->category == KeyCategoryIButton) {
        hint = "Ready in iButton app!";
    }
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, hint);

    // Footer
    canvas_draw_rbox(canvas, 18, 54, 92, 10, 2);
    canvas_invert_color(canvas);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[OK] Scan Next Key");
    canvas_invert_color(canvas);
}

static void draw_screen_quick_keys(Canvas* canvas, KeyWizardApp* app) {
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "UNIVERSAL KEYS");
    canvas_invert_color(canvas);

    size_t sel = app->state.quick_key_idx;

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < QUICK_KEYS_COUNT; i++) {
        int y = 20 + (i * 8);
        if(i == sel) {
            canvas_draw_rbox(canvas, 2, y - 6, 124, 8, 1);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, 6, y, QUICK_KEYS[i].title);
            canvas_invert_color(canvas);
        } else {
            canvas_draw_str(canvas, 6, y, QUICK_KEYS[i].title);
        }
    }

    // Detail box for selected item
    canvas_draw_line(canvas, 0, 46, 128, 46);
    char detail[64];
    snprintf(detail, sizeof(detail), "%s: %s", QUICK_KEYS[sel].proto, QUICK_KEYS[sel].uid);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, detail);

    // Footer
    canvas_draw_line(canvas, 0, 56, 128, 56);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 63, "[^/v] Scroll");
    canvas_draw_str_aligned(canvas, 124, 63, AlignRight, AlignBottom, "[Back] Return");
}

static const char* const about_lines[] = {
    "Key Wizard v1.2.0",
    "Multi-Reader Sorter",
    "-------------------",
    "Supported Technologies:",
    "• 125 kHz RFID",
    "  EM4100, HID, Indala",
    "• 13.56 MHz NFC",
    "  Mifare, NTAG, ISO14443",
    "• 1-Wire iButton",
    "  Dallas, Cyfral, Metakom",
    "-------------------",
    "Direct Asset Saving:",
    "• /ext/lfrfid/*.rfid",
    "• /ext/nfc/*.nfc",
    "• /ext/ibutton/*.ibtn",
    "-------------------",
    "Author:",
    "Vazgen Grigoryan",
    "License: MIT",
    "-------------------",
    "Donate Bitcoin (BTC):",
    "bc1q7fu5pck45x2v03e",
    "6yyqzf2hexu598s58c8zczg",
    "-------------------",
    "GitHub Project:",
    "vazgen3201/flipper-",
    "key-wizard",
};
#define ABOUT_LINES_COUNT   (sizeof(about_lines) / sizeof(about_lines[0]))
#define ABOUT_VISIBLE_LINES 4

static void draw_screen_about(Canvas* canvas, KeyWizardApp* app) {
    // 1. Inverted Header with Icons
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 6, 1, &I_key_icon);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "ABOUT & SUPPORT");
    canvas_draw_icon(canvas, 112, 1, &I_key_icon);
    canvas_invert_color(canvas);

    // 2. Scrollable Content (y = 13 to 53)
    canvas_set_font(canvas, FontSecondary);
    int32_t scroll = app->state.about_scroll;
    if(scroll < 0) scroll = 0;
    if(scroll > (int32_t)(ABOUT_LINES_COUNT - ABOUT_VISIBLE_LINES)) {
        scroll = ABOUT_LINES_COUNT - ABOUT_VISIBLE_LINES;
    }

    for(int32_t i = 0; i < ABOUT_VISIBLE_LINES; i++) {
        int32_t idx = scroll + i;
        if(idx < (int32_t)ABOUT_LINES_COUNT) {
            int32_t y = 22 + (i * 10);
            canvas_draw_str(canvas, 4, y, about_lines[idx]);
        }
    }

    // 3. Native Scrollbar on right edge
    elements_scrollbar_pos(
        canvas, 124, 13, 41, (size_t)scroll, (size_t)(ABOUT_LINES_COUNT - ABOUT_VISIBLE_LINES + 1));

    // 4. Footer navigation
    canvas_draw_line(canvas, 0, 54, 128, 54);
    canvas_draw_str(canvas, 2, 62, "[<]Back");
    canvas_draw_str_aligned(canvas, 122, 62, AlignRight, AlignBottom, "[^v]Scroll");
}

static void key_wizard_render_callback(Canvas* canvas, void* ctx) {
    KeyWizardApp* app = (KeyWizardApp*)ctx;
    if(!app) return;

    switch(app->state.screen) {
    case ScreenScanning:
        draw_screen_scanning(canvas, app);
        break;
    case ScreenKeyFound:
        draw_screen_key_found(canvas, app);
        break;
    case ScreenSaved:
        draw_screen_saved(canvas, app);
        break;
    case ScreenQuickKeys:
        draw_screen_quick_keys(canvas, app);
        break;
    case ScreenAbout:
        draw_screen_about(canvas, app);
        break;
    default:
        break;
    }
}

static void key_wizard_input_callback(InputEvent* input_event, void* ctx) {
    KeyWizardApp* app = (KeyWizardApp*)ctx;
    WizardEvent event;
    event.type = EventTypeInput;
    event.input = *input_event;
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

int32_t key_wizard_app(void* p) {
    UNUSED(p);

    KeyWizardApp* app = malloc(sizeof(KeyWizardApp));
    memset(app, 0, sizeof(KeyWizardApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(WizardEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scanner = key_scanner_alloc();

    app->state.scan_mode = ScanModeAuto;

    // Setup GUI
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, key_wizard_render_callback, app);
    view_port_input_callback_set(app->view_port, key_wizard_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    key_scanner_start(app->scanner);

    app->is_running = true;
    WizardEvent event;

    while(app->is_running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 60);

        // Process Input
        if(status == FuriStatusOk && event.type == EventTypeInput) {
            if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                switch(event.input.key) {
                case InputKeyBack:
                    if(app->state.screen == ScreenScanning) {
                        app->is_running = false;
                    } else if(
                        app->state.screen == ScreenAbout || app->state.screen == ScreenQuickKeys ||
                        app->state.screen == ScreenKeyFound) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    } else if(app->state.screen == ScreenSaved) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    }
                    break;
                case InputKeyRight:
                    if(app->state.screen == ScreenScanning) {
                        app->state.scan_mode = (app->state.scan_mode + 1) % ScanModeMax;
                        key_scanner_set_mode(app->scanner, app->state.scan_mode);
                    }
                    break;
                case InputKeyLeft:
                    if(app->state.screen == ScreenScanning) {
                        app->state.scan_mode = (app->state.scan_mode == 0) ?
                                                   (ScanModeMax - 1) :
                                                   (app->state.scan_mode - 1);
                        key_scanner_set_mode(app->scanner, app->state.scan_mode);
                    } else if(
                        app->state.screen == ScreenAbout || app->state.screen == ScreenQuickKeys) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    }
                    break;
                case InputKeyUp:
                    if(app->state.screen == ScreenScanning) {
                        app->state.screen = ScreenQuickKeys;
                        key_scanner_stop(app->scanner);
                    } else if(app->state.screen == ScreenQuickKeys && app->state.quick_key_idx > 0) {
                        app->state.quick_key_idx--;
                    } else if(app->state.screen == ScreenAbout && app->state.about_scroll > 0) {
                        app->state.about_scroll--;
                    }
                    break;
                case InputKeyDown:
                    if(app->state.screen == ScreenScanning) {
                        app->state.screen = ScreenAbout;
                        app->state.about_scroll = 0;
                        key_scanner_stop(app->scanner);
                    } else if(
                        app->state.screen == ScreenQuickKeys &&
                        app->state.quick_key_idx + 1 < QUICK_KEYS_COUNT) {
                        app->state.quick_key_idx++;
                    } else if(
                        app->state.screen == ScreenAbout &&
                        app->state.about_scroll + ABOUT_VISIBLE_LINES <
                            (int32_t)ABOUT_LINES_COUNT) {
                        app->state.about_scroll++;
                    }
                    break;
                case InputKeyOk:
                    if(app->state.screen == ScreenAbout) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    } else if(app->state.screen == ScreenQuickKeys) {
                        size_t sel = app->state.quick_key_idx;
                        memset(&app->state.current_key, 0, sizeof(DetectedKey));
                        app->state.current_key.category = QUICK_KEYS[sel].cat;
                        app->state.current_key.valid = true;
                        strncpy(
                            app->state.current_key.protocol_name,
                            QUICK_KEYS[sel].proto,
                            sizeof(app->state.current_key.protocol_name) - 1);
                        strncpy(
                            app->state.current_key.data_hex,
                            QUICK_KEYS[sel].uid,
                            sizeof(app->state.current_key.data_hex) - 1);
                        if(QUICK_KEYS[sel].cat == KeyCategoryNFC) {
                            uint8_t uid[4] = {0xDE, 0xAD, 0xBE, 0xEF};
                            memcpy(app->state.current_key.data, uid, 4);
                            app->state.current_key.data_len = 4;
                            strncpy(
                                app->state.current_key.atqa,
                                "00 04",
                                sizeof(app->state.current_key.atqa));
                            strncpy(
                                app->state.current_key.sak,
                                "08",
                                sizeof(app->state.current_key.sak));
                            app->state.current_key.atqa_raw[0] = 0x04;
                            app->state.current_key.atqa_raw[1] = 0x00;
                            app->state.current_key.sak_raw = 0x08;
                        } else if(QUICK_KEYS[sel].cat == KeyCategoryLFRFID) {
                            uint8_t uid[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
                            memcpy(app->state.current_key.data, uid, 5);
                            app->state.current_key.data_len = 5;
                        } else if(QUICK_KEYS[sel].cat == KeyCategoryIButton) {
                            if(strstr(QUICK_KEYS[sel].proto, "Cyfral") != NULL) {
                                uint8_t rom[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
                                memcpy(app->state.current_key.data, rom, 8);
                                app->state.current_key.data_len = 8;
                            } else {
                                uint8_t rom[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
                                memcpy(app->state.current_key.data, rom, 8);
                                app->state.current_key.data_len = 8;
                            }
                        }
                        app->state.screen = ScreenKeyFound;
                    } else if(app->state.screen == ScreenKeyFound) {
                        char saved_path[128];
                        if(key_saver_save_to_native_asset(
                               &app->state.current_key, saved_path, sizeof(saved_path))) {
                            notification_message(app->notifications, &sequence_audiovisual_alert);
                            notification_message(app->notifications, &sequence_double_vibro);
                            app->state.screen = ScreenSaved;
                        }
                    } else if(app->state.screen == ScreenSaved) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    }
                    break;
                default:
                    break;
                }
                view_port_update(app->view_port);
            }
        }

        // Periodic background scanning polling & animation
        if(app->state.screen == ScreenScanning) {
            app->state.anim_frame++;
            DetectedKey key;
            if(key_scanner_poll_step(app->scanner, &key)) {
                key_scanner_stop(app->scanner);
                app->state.current_key = key;
                app->state.screen = ScreenKeyFound;
                notification_message(app->notifications, &sequence_success);
                notification_message(app->notifications, &sequence_single_vibro);
            }
            view_port_update(app->view_port);
        }
    }

    // Cleanup
    key_scanner_stop(app->scanner);
    key_scanner_free(app->scanner);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}
