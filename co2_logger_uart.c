#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_port.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

#include "co2_logger.h"

#define CSV_FILE_PATH "/ext/apps_data/co2_logger/co2_log.csv"
#define LOG_INTERVAL_MS 30000 // 30 seconds

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    co2_logger* co2_logger;
    Storage* storage;
    File* csv_file;

    FuriMutex* mutex;
    uint32_t co2_ppm;
    uint32_t counter;
    bool is_connected;
    uint32_t last_log_time;
} co2_loggerUart;

static bool co2_logger_uart_create_csv_file(co2_loggerUart* state) {
    FURI_LOG_I("CSV", "Creating CSV file...");
    
    // Create directory if it doesn't exist
    FURI_LOG_D("CSV", "Creating directories...");
    storage_simply_mkdir(state->storage, "/ext/apps_data");
    storage_simply_mkdir(state->storage, "/ext/apps_data/co2_logger");
    
    // Open or create CSV file
    FURI_LOG_D("CSV", "Allocating file handle...");
    state->csv_file = storage_file_alloc(state->storage);
    if(!state->csv_file) {
        FURI_LOG_E("CSV", "Failed to allocate file");
        return false;
    }
    
    FURI_LOG_D("CSV", "Checking if file exists...");
    bool file_exists = storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    
    if(file_exists) {
        storage_file_close(state->csv_file);
        FURI_LOG_I("CSV", "File exists, reopening for append");
    } else {
        FURI_LOG_I("CSV", "File does not exist, will create new file");
    }
    
    // Open file for append (create if doesn't exist)
    FURI_LOG_D("CSV", "Opening file for writing...");
    bool success = storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);
    
    if(!success) {
        FURI_LOG_E("CSV", "Failed to open CSV file for writing");
        storage_file_free(state->csv_file);
        state->csv_file = NULL;
        return false;
    }
    
    if(success && !file_exists) {
        // Write CSV header if file is new
        const char* header = "Timestamp,CO2_PPM,Status\n";
        size_t bytes_written = storage_file_write(state->csv_file, header, strlen(header));
        FURI_LOG_I("CSV", "Created new CSV file with header (%zu bytes written)", bytes_written);
    }
    
    FURI_LOG_I("CSV", "CSV file ready");
    return success;
}

static void co2_logger_uart_log_reading(co2_loggerUart* state) {
    if(!state->csv_file) {
        FURI_LOG_W("CSV", "CSV file not available for logging");
        return;
    }
    
    FURI_LOG_D("CSV", "Getting current timestamp...");
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    
    char log_entry[128];
    snprintf(log_entry, sizeof(log_entry), 
             "%04d-%02d-%02d %02d:%02d:%02d,%lu,%s\n",
             datetime.year, datetime.month, datetime.day,
             datetime.hour, datetime.minute, datetime.second,
             state->co2_ppm,
             state->is_connected ? "Connected" : "Disconnected");
    
    FURI_LOG_D("CSV", "Writing log entry: %s", log_entry);
    size_t bytes_written = storage_file_write(state->csv_file, log_entry, strlen(log_entry));
    FURI_LOG_D("CSV", "Wrote %zu bytes to CSV", bytes_written);
    
    storage_file_sync(state->csv_file);
    FURI_LOG_D("CSV", "File sync completed");
}

static void co2_logger_uart_render_callback(Canvas* canvas, void* ctx) {
    co2_loggerUart* state = ctx;
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

static void co2_logger_uart_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    if(input_event->type == InputTypeShort) {
        furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    }
}

int32_t co2_logger_app(void* p) {
    UNUSED(p);
    
    FURI_LOG_I("App", "Starting CO2 Logger app - Debug Mode");

    co2_loggerUart state = {0};
    FURI_LOG_I("App", "Opening records...");
    state.gui = furi_record_open(RECORD_GUI);
    state.notification = furi_record_open(RECORD_NOTIFICATION);
    state.storage = furi_record_open(RECORD_STORAGE);
    
    FURI_LOG_I("App", "Allocating resources...");
    state.view_port = view_port_alloc();
    state.event_queue = furi_message_queue_alloc(32, sizeof(InputEvent));
    state.co2_logger = co2_logger_alloc();
    state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state.last_log_time = 0;

    FURI_LOG_I("App", "Setting up CSV logging...");
    // Create CSV file for logging
    if(!co2_logger_uart_create_csv_file(&state)) {
        FURI_LOG_W("App", "CSV logging disabled - continuing without file logging");
    }

    FURI_LOG_I("App", "Setting up GUI...");
    view_port_draw_callback_set(state.view_port, co2_logger_uart_render_callback, &state);
    view_port_input_callback_set(state.view_port, co2_logger_uart_input_callback, state.event_queue);
    gui_add_view_port(state.gui, state.view_port, GuiLayerFullscreen);
    
    FURI_LOG_I("App", "Opening CO2 logger...");
    co2_logger_open(state.co2_logger);
    notification_message(state.notification, &sequence_display_backlight_enforce_on);
    
    FURI_LOG_I("App", "Entering main loop...");

    InputEvent event;
    do {
        bool do_refresh = furi_message_queue_get(state.event_queue, &event, 1000) != FuriStatusOk;
        uint32_t current_time = furi_get_tick();

        furi_mutex_acquire(state.mutex, FuriWaitForever);

        if(do_refresh) {
            FURI_LOG_D("App", "Attempting to read CO2 concentration...");
            state.is_connected = co2_logger_read_gas_concentration(state.co2_logger, &state.co2_ppm);
            if(state.is_connected) {
                state.counter++;
                FURI_LOG_D("App", "CO2 reading successful: %lu ppm", state.co2_ppm);
            } else {
                state.counter = 0;
                FURI_LOG_W("App", "CO2 reading failed - sensor disconnected?");
            }
            
            // Log to CSV every 30 seconds
            if(current_time - state.last_log_time >= LOG_INTERVAL_MS) {
                FURI_LOG_D("App", "Logging to CSV file...");
                co2_logger_uart_log_reading(&state);
                state.last_log_time = current_time;
                FURI_LOG_D("App", "CSV log entry completed");
            }
        } else if(event.key == InputKeyBack) {
            FURI_LOG_I("App", "Back button pressed - exiting app");
            furi_mutex_release(state.mutex);
            break;
        }

        furi_mutex_release(state.mutex);
        view_port_update(state.view_port);
    } while(true);

    FURI_LOG_I("App", "Cleaning up and exiting...");
    notification_message(state.notification, &sequence_display_backlight_enforce_auto);
    co2_logger_close(state.co2_logger);
    gui_remove_view_port(state.gui, state.view_port);

    // Clean up CSV file
    if(state.csv_file) {
        storage_file_close(state.csv_file);
        storage_file_free(state.csv_file);
    }

    furi_mutex_free(state.mutex);
    co2_logger_free(state.co2_logger);
    furi_message_queue_free(state.event_queue);
    view_port_free(state.view_port);
    state.notification = NULL;
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    state.gui = NULL;
    furi_record_close(RECORD_GUI);

    return 0;
}
