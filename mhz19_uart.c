#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_port.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

#include "mhz19.h"

#define CSV_FILE_PATH "/ext/apps_data/co2_logger/co2_log.csv"
#define LOG_INTERVAL_MS 30000 // 30 seconds

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    Mhz19* mhz19;
    Storage* storage;
    File* csv_file;

    FuriMutex* mutex;
    uint32_t co2_ppm;
    uint32_t counter;
    bool is_connected;
    uint32_t last_log_time;
} Mhz19Uart;

static bool mhz19_uart_create_csv_file(Mhz19Uart* state) {
    // Create directory if it doesn't exist
    storage_simply_mkdir(state->storage, "/ext/apps_data");
    storage_simply_mkdir(state->storage, "/ext/apps_data/co2_logger");
    
    // Open or create CSV file
    state->csv_file = storage_file_alloc(state->storage);
    bool file_exists = storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    
    if(file_exists) {
        storage_file_close(state->csv_file);
    }
    
    // Open file for append (create if doesn't exist)
    bool success = storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);
    
    if(success && !file_exists) {
        // Write CSV header if file is new
        const char* header = "Timestamp,CO2_PPM,Status\n";
        storage_file_write(state->csv_file, header, strlen(header));
    }
    
    return success;
}

static void mhz19_uart_log_reading(Mhz19Uart* state) {
    if(!state->csv_file) return;
    
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    
    char log_entry[128];
    snprintf(log_entry, sizeof(log_entry), 
             "%04d-%02d-%02d %02d:%02d:%02d,%lu,%s\n",
             datetime.year, datetime.month, datetime.day,
             datetime.hour, datetime.minute, datetime.second,
             state->co2_ppm,
             state->is_connected ? "Connected" : "Disconnected");
    
    storage_file_write(state->csv_file, log_entry, strlen(log_entry));
    storage_file_sync(state->csv_file);
}

static void mhz19_uart_render_callback(Canvas* canvas, void* ctx) {
    Mhz19Uart* state = ctx;
    char buffer[64];

    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontSecondary);
    if(state->is_connected) {
        snprintf(buffer, sizeof(buffer), "Status: Online, %lus", state->counter);
    } else {
        snprintf(buffer, sizeof(buffer), "Status: Offline");
    }
    canvas_draw_str(canvas, 10, 10, buffer);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 40, "CO2 PPM:");

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%lu", state->co2_ppm);
    canvas_draw_str(canvas, 70, 40, buffer);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 55, "Logging to CSV");
    canvas_draw_str(canvas, 10, 63, "[back] - to exit");

    furi_mutex_release(state->mutex);
}

static void mhz19_uart_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    }
}

int32_t co2_logger_app(void* p) {
    UNUSED(p);

    Mhz19Uart state = {0};
    state.gui = furi_record_open(RECORD_GUI);
    state.notification = furi_record_open(RECORD_NOTIFICATION);
    state.storage = furi_record_open(RECORD_STORAGE);
    state.view_port = view_port_alloc();
    state.event_queue = furi_message_queue_alloc(32, sizeof(InputEvent));
    state.mhz19 = mhz19_alloc();
    state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state.last_log_time = 0;

    // Create CSV file for logging
    if(!mhz19_uart_create_csv_file(&state)) {
        // Handle error - for now just continue without logging
    }

    view_port_draw_callback_set(state.view_port, mhz19_uart_render_callback, &state);
    view_port_input_callback_set(state.view_port, mhz19_uart_input_callback, state.event_queue);
    gui_add_view_port(state.gui, state.view_port, GuiLayerFullscreen);
    mhz19_open(state.mhz19);
    notification_message(state.notification, &sequence_display_backlight_enforce_on);

    InputEvent event;
    do {
        bool do_refresh = furi_message_queue_get(state.event_queue, &event, 1000) != FuriStatusOk;
        uint32_t current_time = furi_get_tick();

        furi_mutex_acquire(state.mutex, FuriWaitForever);

        if(do_refresh) {
            state.is_connected = mhz19_read_gas_concentration(state.mhz19, &state.co2_ppm);
            if(state.is_connected) {
                state.counter++;
            } else {
                state.counter = 0;
            }
            
            // Log to CSV every 30 seconds
            if(current_time - state.last_log_time >= LOG_INTERVAL_MS) {
                mhz19_uart_log_reading(&state);
                state.last_log_time = current_time;
            }
        } else if(event.key == InputKeyBack) {
            furi_mutex_release(state.mutex);
            break;
        }

        furi_mutex_release(state.mutex);
        view_port_update(state.view_port);
    } while(true);

    notification_message(state.notification, &sequence_display_backlight_enforce_auto);
    mhz19_close(state.mhz19);
    gui_remove_view_port(state.gui, state.view_port);

    // Clean up CSV file
    if(state.csv_file) {
        storage_file_close(state.csv_file);
        storage_file_free(state.csv_file);
    }

    furi_mutex_free(state.mutex);
    mhz19_free(state.mhz19);
    furi_message_queue_free(state.event_queue);
    view_port_free(state.view_port);
    state.notification = NULL;
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    state.gui = NULL;
    furi_record_close(RECORD_GUI);

    return 0;
}
