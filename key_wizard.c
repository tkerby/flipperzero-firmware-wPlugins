#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "key_types.h"
#include "key_saver.h"
#include "key_scanner.h"

typedef enum {
    ScreenScanning = 0,
    ScreenKeyFound,
    ScreenSaved,
    ScreenQuickKeys,
} WizardScreen;

typedef struct {
    WizardScreen screen;
    ScanMode scan_mode;
    DetectedKey current_key;
    size_t quick_key_idx;
    uint32_t anim_frame;
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
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "UNIVERSAL KEY WIZARD");

    ScanMode mode = app->state.scan_mode;
    KeyCategory phase = key_scanner_get_current_phase(app->scanner);

    // Mode Selector Header (updates immediately)
    canvas_set_font(canvas, FontSecondary);
    char mode_str[48];
    snprintf(mode_str, sizeof(mode_str), "Mode: <%s>", scan_mode_to_string(mode));
    canvas_draw_str(canvas, 2, 21, mode_str);

    // Active Reader Indicators
    bool rfid_active = (mode == ScanModeRFIDOnly) || (mode == ScanModeAuto && phase == KeyCategoryLFRFID);
    bool nfc_active = (mode == ScanModeNFCOnly) || (mode == ScanModeAuto && phase == KeyCategoryNFC);
    bool ibutton_active = (mode == ScanModeIButtonOnly) || (mode == ScanModeAuto && phase == KeyCategoryIButton);

    char line[48];
    snprintf(line, sizeof(line), "%s 125 kHz RFID", rfid_active ? "[*]" : "[ ]");
    canvas_draw_str(canvas, 4, 32, line);

    snprintf(line, sizeof(line), "%s 13.56 MHz NFC", nfc_active ? "[*]" : "[ ]");
    canvas_draw_str(canvas, 4, 41, line);

    snprintf(line, sizeof(line), "%s 1-Wire iButton", ibutton_active ? "[*]" : "[ ]");
    canvas_draw_str(canvas, 4, 50, line);

    // Footer
    canvas_draw_line(canvas, 0, 53, 128, 53);
    canvas_draw_str(canvas, 2, 62, "[< >] Mode   [^] Keys List");
}

static void draw_screen_key_found(Canvas* canvas, KeyWizardApp* app) {
    DetectedKey* key = &app->state.current_key;

    // Header banner
    canvas_draw_rbox(canvas, 0, 0, 128, 13, 2);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "KEY DETECTED!");
    canvas_invert_color(canvas);

    canvas_set_font(canvas, FontSecondary);

    // Type badge
    char type_line[48];
    snprintf(type_line, sizeof(type_line), "Type: %s", key_category_to_string(key->category));
    canvas_draw_str(canvas, 2, 24, type_line);

    // Protocol
    char proto_line[48];
    snprintf(proto_line, sizeof(proto_line), "Proto: %s", key->protocol_name);
    canvas_draw_str(canvas, 2, 34, proto_line);

    // Data / UID
    char data_line[140];
    snprintf(data_line, sizeof(data_line), "UID: %s", key->data_hex);
    canvas_draw_str(canvas, 2, 44, data_line);

    // Footer
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_draw_str(canvas, 2, 62, "[OK] Save to native  [<] Back");
}

static void draw_screen_saved(Canvas* canvas, KeyWizardApp* app) {
    DetectedKey* key = &app->state.current_key;

    // Header banner
    canvas_draw_rbox(canvas, 0, 0, 128, 13, 2);
    canvas_invert_color(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "SAVED TO ASSETS!");
    canvas_invert_color(canvas);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, "Stored in native folder:");

    // Show path
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 34, key->saved_path);

    canvas_set_font(canvas, FontSecondary);
    const char* hint = "Available in Flipper's app!";
    if(key->category == KeyCategoryLFRFID) {
        hint = "Open via native 125 kHz RFID!";
    } else if(key->category == KeyCategoryNFC) {
        hint = "Open via native NFC app!";
    } else if(key->category == KeyCategoryIButton) {
        hint = "Open via native iButton app!";
    }
    canvas_draw_str(canvas, 2, 46, hint);

    // Footer
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_draw_str(canvas, 2, 62, "[OK] Scan next key");
}

static void draw_screen_quick_keys(Canvas* canvas, KeyWizardApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "UNIVERSAL KEYS (LIST)");

    canvas_set_font(canvas, FontSecondary);
    size_t sel = app->state.quick_key_idx;

    for(size_t i = 0; i < QUICK_KEYS_COUNT; i++) {
        int y = 20 + (i * 8);
        if(i == sel) {
            canvas_draw_box(canvas, 0, y - 6, 128, 8);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, 2, y, QUICK_KEYS[i].title);
            canvas_invert_color(canvas);
        } else {
            canvas_draw_str(canvas, 2, y, QUICK_KEYS[i].title);
        }
    }

    // Details of selected item
    canvas_draw_line(canvas, 0, 46, 128, 46);
    char detail[64];
    snprintf(detail, sizeof(detail), "%s: %s", QUICK_KEYS[sel].proto, QUICK_KEYS[sel].uid);
    canvas_draw_str(canvas, 2, 53, detail);

    // Footer (informational only, no save click)
    canvas_draw_line(canvas, 0, 55, 128, 55);
    canvas_draw_str(canvas, 2, 63, "[^/v] Scroll     [Back] Return");
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
                    } else if(app->state.screen == ScreenQuickKeys || app->state.screen == ScreenKeyFound) {
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
                        app->state.scan_mode = (app->state.scan_mode == 0) ? (ScanModeMax - 1) : (app->state.scan_mode - 1);
                        key_scanner_set_mode(app->scanner, app->state.scan_mode);
                    } else if(app->state.screen == ScreenQuickKeys) {
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
                    }
                    break;
                case InputKeyDown:
                    if(app->state.screen == ScreenQuickKeys && app->state.quick_key_idx + 1 < QUICK_KEYS_COUNT) {
                        app->state.quick_key_idx++;
                    }
                    break;
                case InputKeyOk:
                    if(app->state.screen == ScreenKeyFound) {
                        // Save directly into the native Flipper storage folder
                        char saved_path[128];
                        if(key_saver_save_to_native_asset(&app->state.current_key, saved_path, sizeof(saved_path))) {
                            notification_message(app->notifications, &sequence_audiovisual_alert);
                            notification_message(app->notifications, &sequence_double_vibro);
                            app->state.screen = ScreenSaved;
                        }
                    } else if(app->state.screen == ScreenSaved) {
                        app->state.screen = ScreenScanning;
                        key_scanner_start(app->scanner);
                    }
                    // Note: on ScreenQuickKeys and ScreenScanning, [OK] does NOT create or save anything!
                    break;
                default:
                    break;
                }
                view_port_update(app->view_port);
            }
        }

        // Periodic background scanning polling
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
