#include <dolphin/dolphin.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>

#include <furi_hal_infrared.h>
#include <furi_hal_nfc.h>
#include <furi_hal_subghz.h>

#include <paranoia_icons.h>

typedef enum {
    ParanoiaStateIdle,
    ParanoiaStateRfScan,
    ParanoiaStateNfcScan,
    ParanoiaStateIrScan,
    ParanoiaStateOptionsMenu,
    ParanoiaStateInfoMenu
} ParanoiaState;

typedef enum {
    MenuItemRfEnabled,
    MenuItemNfcEnabled,
    MenuItemIrEnabled,
    MenuItemSensitivity,
    MenuItemExit,
    MenuItemCount
} MenuItem;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    ParanoiaState state;
    bool running;
    int threat_level;
    bool rf_anomaly;
    bool nfc_anomaly;
    bool ir_anomaly;
    FuriMutex* mutex;
    bool rf_scan_enabled;
    bool nfc_scan_enabled;
    bool ir_scan_enabled;
    uint8_t sensitivity;
    MenuItem selected_menu_item;
    uint32_t rf_signal_count;
    uint32_t nfc_signal_count;
    uint32_t ir_signal_count;
} Paranoia;

typedef enum {
    EventTypeTick,
    EventTypeKey,
    EventTypeNotification,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
    NotificationMessage notification;
} ParanoiaEvent;

static void paranoia_draw_info_menu(Canvas* canvas, Paranoia* paranoia) {
    canvas_set_font(canvas, FontPrimary);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 10, "RF Scan: ");
    canvas_draw_str(canvas, 90, 10, paranoia->rf_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 20, "NFC Scan: ");
    canvas_draw_str(canvas, 90, 20, paranoia->nfc_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 30, "IR Scan: ");
    canvas_draw_str(canvas, 90, 30, paranoia->ir_scan_enabled ? "ON" : "OFF");
    canvas_draw_str(canvas, 2, 50, "App Created by: ");
    canvas_draw_str(canvas, 70, 50, "C0d3-5t3w");
    elements_button_left(canvas, "Back");
}

static void paranoia_draw_options_menu(Canvas* canvas, Paranoia* paranoia) {
    canvas_set_font(canvas, FontPrimary);

    canvas_set_font(canvas, FontSecondary);

    const char* menu_titles[] = {
        "RF Scanning:", "NFC Scanning:", "IR Scanning:", "Sensitivity:", "Exit Menu"};

    for(MenuItem i = 0; i < MenuItemCount; i++) {
        if(i == paranoia->selected_menu_item) {
            canvas_draw_str(canvas, 2, 10 + i * 10, ">");
        }
        canvas_draw_str(canvas, 10, 10 + i * 10, menu_titles[i]);

        if(i == MenuItemRfEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->rf_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemNfcEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->nfc_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemIrEnabled) {
            canvas_draw_str(canvas, 90, 10 + i * 10, paranoia->ir_scan_enabled ? "ON" : "OFF");
        } else if(i == MenuItemSensitivity) {
            const char* sensitivity_levels[] = {"Low", "Medium", "High"};
            canvas_draw_str(
                canvas, 90, 10 + i * 10, sensitivity_levels[paranoia->sensitivity - 1]);
        }
    }

    elements_button_center(canvas, "Select");
    elements_button_right(canvas, "Back");
}

static void paranoia_draw_callback(Canvas* canvas, void* context) {
    Paranoia* paranoia = context;
    furi_check(furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(canvas);

    if(paranoia->state == ParanoiaStateOptionsMenu) {
        paranoia_draw_options_menu(canvas, paranoia);
    } else if(paranoia->state == ParanoiaStateInfoMenu) {
        paranoia_draw_info_menu(canvas, paranoia);
    } else {
        canvas_set_font(canvas, FontPrimary);

        canvas_set_font(canvas, FontSecondary);
        switch(paranoia->state) {
        case ParanoiaStateIdle:
            canvas_draw_str(canvas, 2, 10, "Status: Idle");
            break;
        case ParanoiaStateRfScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning RF");
            break;
        case ParanoiaStateNfcScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning NFC");
            break;
        case ParanoiaStateIrScan:
            canvas_draw_str(canvas, 2, 10, "Status: Scanning IR");
            break;
        default:
            break;
        }

        char threat_str[32];
        snprintf(threat_str, sizeof(threat_str), "Threat Level: %d/10", paranoia->threat_level);
        canvas_draw_str(canvas, 2, 24, threat_str);

        char enabled_str[32];
        snprintf(
            enabled_str,
            sizeof(enabled_str),
            "Enabled: %s%s%s",
            paranoia->rf_scan_enabled ? "RF " : "",
            paranoia->nfc_scan_enabled ? "NFC " : "",
            paranoia->ir_scan_enabled ? "IR" : "");
        canvas_draw_str(canvas, 2, 36, enabled_str);

        canvas_draw_str(canvas, 2, 50, "Detected:");

        if(paranoia->rf_anomaly) {
            canvas_draw_str(canvas, 55, 50, "~RF~");
        }

        if(paranoia->nfc_anomaly) {
            canvas_draw_str(canvas, 75, 50, "~NFC~");
        }

        if(paranoia->ir_anomaly) {
            canvas_draw_str(canvas, 105, 50, "~IR~");
        }

        bool is_scanning = (paranoia->state != ParanoiaStateIdle);
        elements_button_center(canvas, is_scanning ? "Stop" : "Scan");
        elements_button_left(canvas, "Menu");
        elements_button_right(canvas, "Info");
    }

    furi_mutex_release(paranoia->mutex);
}

static void paranoia_input_callback(InputEvent* input_event, void* ctx) {
    Paranoia* paranoia = ctx;
    ParanoiaEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(paranoia->event_queue, &event, FuriWaitForever);
}

static void paranoia_scan_rf(Paranoia* paranoia) {
    if(!paranoia->rf_scan_enabled) {
        paranoia->rf_anomaly = false;
        return;
    }

    if(furi_hal_subghz_is_frequency_valid(433920000)) {
        furi_hal_subghz_reset();
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(433920000);
        furi_hal_subghz_start_async_rx(NULL, NULL);

        furi_delay_ms(200);

        uint32_t threshold = 10 - (paranoia->sensitivity * 3);
        paranoia->rf_signal_count = 0;

        for(uint8_t i = 0; i < 5; i++) {
            float rssi = furi_hal_subghz_get_rssi();
            if(rssi > -80.0f + (paranoia->sensitivity * 10.0f)) {
                paranoia->rf_signal_count++;
            }
            furi_delay_ms(50);
        }

        paranoia->rf_anomaly = paranoia->rf_signal_count >= threshold;

        furi_hal_subghz_stop_async_rx();
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    } else {
        paranoia->rf_anomaly = false;
    }

    if(paranoia->rf_anomaly) {
        paranoia->threat_level += 1 + paranoia->sensitivity;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

static void paranoia_scan_nfc(Paranoia* paranoia) {
    if(!paranoia->nfc_scan_enabled) {
        paranoia->nfc_anomaly = false;
        return;
    }

    paranoia->nfc_signal_count = 0;
    bool field_present = false;

    furi_hal_nfc_init();

    for(uint8_t i = 0; i < 5; i++) {
        field_present = furi_hal_nfc_field_is_present();
        if(field_present) {
            paranoia->nfc_signal_count++;
        }
        furi_delay_ms(100);
    }

    furi_hal_nfc_event_stop();

    uint32_t threshold = 3 - paranoia->sensitivity + 1;
    paranoia->nfc_anomaly = (paranoia->nfc_signal_count >= threshold);

    if(paranoia->nfc_anomaly) {
        paranoia->threat_level += 1 + paranoia->sensitivity;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

static void paranoia_scan_ir(Paranoia* paranoia) {
    if(!paranoia->ir_scan_enabled) {
        paranoia->ir_anomaly = false;
        return;
    }

    paranoia->ir_signal_count = 0;

    furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL);

    for(uint8_t i = 0; i < 10; i++) {
        if(furi_hal_infrared_is_busy()) {
            paranoia->ir_signal_count++;
        }
        furi_delay_ms(100);
    }

    furi_hal_infrared_async_rx_set_capture_isr_callback(NULL, NULL);

    uint8_t threshold = 2 - (paranoia->sensitivity - 1);
    paranoia->ir_anomaly = (paranoia->ir_signal_count >= threshold);

    if(paranoia->ir_anomaly) {
        paranoia->threat_level += 2 + paranoia->sensitivity;
        notification_message(paranoia->notifications, &sequence_error);
    }
}

static void paranoia_state_machine(Paranoia* paranoia) {
    switch(paranoia->state) {
    case ParanoiaStateIdle:
        break;

    case ParanoiaStateRfScan:
        paranoia_scan_rf(paranoia);
        if(paranoia->nfc_scan_enabled) {
            paranoia->state = ParanoiaStateNfcScan;
        } else if(paranoia->ir_scan_enabled) {
            paranoia->state = ParanoiaStateIrScan;
        } else {
            paranoia->state = ParanoiaStateIdle;
        }
        break;

    case ParanoiaStateNfcScan:
        paranoia_scan_nfc(paranoia);
        if(paranoia->ir_scan_enabled) {
            paranoia->state = ParanoiaStateIrScan;
        } else {
            paranoia->state = ParanoiaStateIdle;
        }
        break;

    case ParanoiaStateIrScan:
        paranoia_scan_ir(paranoia);
        paranoia->state = ParanoiaStateIdle;
        break;

    case ParanoiaStateOptionsMenu:
        break;

    case ParanoiaStateInfoMenu:
        break;
    }

    if(paranoia->threat_level > 10) paranoia->threat_level = 10;
}

static void paranoia_handle_menu_input(Paranoia* paranoia, InputKey key) {
    switch(key) {
    case InputKeyUp:
        if(paranoia->selected_menu_item > 0) {
            paranoia->selected_menu_item--;
        } else {
            paranoia->selected_menu_item = MenuItemCount - 1;
        }
        break;

    case InputKeyDown:
        paranoia->selected_menu_item = (paranoia->selected_menu_item + 1) % MenuItemCount;
        break;

    case InputKeyRight:
    case InputKeyOk:
        switch(paranoia->selected_menu_item) {
        case MenuItemRfEnabled:
            paranoia->rf_scan_enabled = !paranoia->rf_scan_enabled;
            break;

        case MenuItemNfcEnabled:
            paranoia->nfc_scan_enabled = !paranoia->nfc_scan_enabled;
            break;

        case MenuItemIrEnabled:
            paranoia->ir_scan_enabled = !paranoia->ir_scan_enabled;
            break;

        case MenuItemSensitivity:
            paranoia->sensitivity = (paranoia->sensitivity % 3) + 1;
            break;

        case MenuItemExit:
            paranoia->state = ParanoiaStateIdle;
            break;

        default:
            break;
        }
        break;

    case InputKeyBack:
        paranoia->state = ParanoiaStateIdle;
        break;

    default:
        break;
    }
}

int32_t paranoia_app(void* p) {
    UNUSED(p);
    Paranoia* paranoia = malloc(sizeof(Paranoia));

    paranoia->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    paranoia->event_queue = furi_message_queue_alloc(8, sizeof(ParanoiaEvent));
    paranoia->view_port = view_port_alloc();
    paranoia->notifications = furi_record_open(RECORD_NOTIFICATION);
    paranoia->state = ParanoiaStateIdle;
    paranoia->running = true;
    paranoia->threat_level = 0;
    paranoia->rf_anomaly = false;
    paranoia->nfc_anomaly = false;
    paranoia->ir_anomaly = false;

    paranoia->rf_scan_enabled = false;
    paranoia->nfc_scan_enabled = false;
    paranoia->ir_scan_enabled = false;
    paranoia->sensitivity = 2;
    paranoia->selected_menu_item = 0;

    paranoia->rf_signal_count = 0;
    paranoia->nfc_signal_count = 0;
    paranoia->ir_signal_count = 0;

    view_port_draw_callback_set(paranoia->view_port, paranoia_draw_callback, paranoia);
    view_port_input_callback_set(paranoia->view_port, paranoia_input_callback, paranoia);

    paranoia->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(paranoia->gui, paranoia->view_port, GuiLayerFullscreen);

    dolphin_deed(DolphinDeedPluginStart);

    ParanoiaEvent event;

    while(paranoia->running) {
        if(furi_message_queue_get(paranoia->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                    furi_check(
                        furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);

                    if(paranoia->state == ParanoiaStateOptionsMenu) {
                        paranoia_handle_menu_input(paranoia, event.input.key);
                    } else if(paranoia->state == ParanoiaStateInfoMenu) {
                        if(event.input.key == InputKeyLeft || event.input.key == InputKeyBack) {
                            paranoia->state = ParanoiaStateIdle;
                        }
                    } else {
                        if(event.input.key == InputKeyOk) {
                            bool is_scanning = (paranoia->state != ParanoiaStateIdle);

                            if(is_scanning) {
                                paranoia->state = ParanoiaStateIdle;
                                notification_message(
                                    paranoia->notifications, &sequence_blink_stop);
                            } else if(
                                paranoia->rf_scan_enabled || paranoia->nfc_scan_enabled ||
                                paranoia->ir_scan_enabled) {
                                paranoia->threat_level = 0;
                                paranoia->rf_anomaly = false;
                                paranoia->nfc_anomaly = false;
                                paranoia->ir_anomaly = false;

                                if(paranoia->rf_scan_enabled) {
                                    paranoia->state = ParanoiaStateRfScan;
                                } else if(paranoia->nfc_scan_enabled) {
                                    paranoia->state = ParanoiaStateNfcScan;
                                } else if(paranoia->ir_scan_enabled) {
                                    paranoia->state = ParanoiaStateIrScan;
                                }

                                notification_message(
                                    paranoia->notifications, &sequence_blink_start_blue);
                            }
                        } else if(event.input.key == InputKeyLeft) {
                            paranoia->state = ParanoiaStateOptionsMenu;
                        } else if(event.input.key == InputKeyRight) {
                            paranoia->state = ParanoiaStateInfoMenu;
                        } else if(event.input.key == InputKeyUp) {
                            paranoia->state = ParanoiaStateIdle;
                        } else if(event.input.key == InputKeyBack) {
                            paranoia->running = false;
                        }
                    }

                    furi_mutex_release(paranoia->mutex);
                }
            }
        }

        furi_check(furi_mutex_acquire(paranoia->mutex, FuriWaitForever) == FuriStatusOk);
        paranoia_state_machine(paranoia);
        furi_mutex_release(paranoia->mutex);

        view_port_update(paranoia->view_port);
    }

    notification_message(paranoia->notifications, &sequence_blink_stop);
    gui_remove_view_port(paranoia->gui, paranoia->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(paranoia->view_port);
    furi_message_queue_free(paranoia->event_queue);
    furi_mutex_free(paranoia->mutex);
    free(paranoia);

    return 0;
}
