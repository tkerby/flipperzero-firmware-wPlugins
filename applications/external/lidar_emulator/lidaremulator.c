#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "lidaremulator_app_i.h"

#include "scenes/lidaremulator_scene.h"

const GpioPin* const pin_led = &gpio_infrared_tx;
const GpioPin* const pin_back = &gpio_button_back;

static bool lidaremulator_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    return scene_manager_handle_custom_event(lidaremulator->scene_manager, event);
}

static bool lidaremulator_back_event_callback(void* context) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    return scene_manager_handle_back_event(lidaremulator->scene_manager);
}

static void lidaremulator_tick_event_callback(void* context) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    scene_manager_handle_tick_event(lidaremulator->scene_manager);
}

static LidarEmulatorApp* LidarEmulatorApp_alloc() {
    LidarEmulatorApp* lidaremulator = malloc(sizeof(LidarEmulatorApp));

    // Initialize app state with defaults
    lidaremulator->app_state.tx_pin = FuriHalInfraredTxPinInternal;
    lidaremulator->app_state.is_otg_enabled = false;

    lidaremulator->gui = furi_record_open(RECORD_GUI);
    lidaremulator->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(lidaremulator->view_dispatcher);
    lidaremulator->scene_manager =
        scene_manager_alloc(&lidaremulator_scene_handlers, lidaremulator);

    view_dispatcher_set_event_callback_context(lidaremulator->view_dispatcher, lidaremulator);
    view_dispatcher_set_custom_event_callback(
        lidaremulator->view_dispatcher, lidaremulator_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        lidaremulator->view_dispatcher, lidaremulator_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        lidaremulator->view_dispatcher, lidaremulator_tick_event_callback, 100);

    lidaremulator->submenu = submenu_alloc();
    view_dispatcher_add_view(
        lidaremulator->view_dispatcher,
        LidarEmulatorViewSubmenu,
        submenu_get_view(lidaremulator->submenu));

    lidaremulator->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        lidaremulator->view_dispatcher,
        LidarEmulatorViewVariableList,
        variable_item_list_get_view(lidaremulator->var_item_list));

    lidaremulator->view_hijacker = alloc_view_hijacker();

    // Load saved settings
    lidaremulator_load_settings(lidaremulator);

    return lidaremulator;
}

static void LidarEmulatorApp_free(LidarEmulatorApp* lidaremulator) {
    view_dispatcher_remove_view(lidaremulator->view_dispatcher, LidarEmulatorViewSubmenu);
    submenu_free(lidaremulator->submenu);

    view_dispatcher_remove_view(lidaremulator->view_dispatcher, LidarEmulatorViewVariableList);
    variable_item_list_free(lidaremulator->var_item_list);

    scene_manager_free(lidaremulator->scene_manager);
    view_dispatcher_free(lidaremulator->view_dispatcher);

    furi_record_close(RECORD_GUI);
    lidaremulator->gui = NULL;

    //    free_view_hijacker(lidaremulator->view_hijacker);

    free(lidaremulator);
}

int lidar_emulator_app(void* p) {
    UNUSED(p);
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    bool otg_was_enabled = furi_hal_power_is_otg_enabled();

    LidarEmulatorApp* lidaremulator = LidarEmulatorApp_alloc();

    view_dispatcher_attach_to_gui(
        lidaremulator->view_dispatcher, lidaremulator->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(lidaremulator->scene_manager, LidarEmulatorSceneStart);

    view_dispatcher_run(lidaremulator->view_dispatcher);

    LidarEmulatorApp_free(lidaremulator);

    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return 0;
}

// GPIO Settings helper functions
#define LIDAREMULATOR_SETTINGS_PATH    APP_ASSETS_PATH(".lidar_emulator.settings")
#define LIDAREMULATOR_SETTINGS_VERSION (1)
#define LIDAREMULATOR_SETTINGS_MAGIC   (0xA5)

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
} LidarEmulatorSettings;

void lidaremulator_set_tx_pin(LidarEmulatorApp* app, FuriHalInfraredTxPin tx_pin) {
    furi_check(app);

    if(tx_pin == FuriHalInfraredTxPinMax) {
        // Auto-detect external module
        FuriHalInfraredTxPin tx_pin_detected = furi_hal_infrared_detect_tx_output();
        furi_hal_infrared_set_tx_output(tx_pin_detected);
        if(tx_pin_detected != FuriHalInfraredTxPinInternal) {
            lidaremulator_enable_otg(app, true);
        }
    } else {
        furi_hal_infrared_set_tx_output(tx_pin);
    }

    app->app_state.tx_pin = tx_pin;
}

void lidaremulator_enable_otg(LidarEmulatorApp* app, bool enable) {
    furi_check(app);
    if(enable) {
        uint8_t attempts = 0;
        while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
            furi_hal_power_enable_otg();
            furi_delay_ms(10);
        }
        furi_delay_ms(200);
    }
    app->app_state.is_otg_enabled = enable;
}

void lidaremulator_load_settings(LidarEmulatorApp* app) {
    furi_check(app);

    LidarEmulatorSettings settings = {0};

    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));
    if(storage_file_open(file, LIDAREMULATOR_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t magic;
        uint8_t version;

        if(storage_file_read(file, &magic, 1) == 1 && magic == LIDAREMULATOR_SETTINGS_MAGIC &&
           storage_file_read(file, &version, 1) == 1 &&
           version == LIDAREMULATOR_SETTINGS_VERSION &&
           storage_file_read(file, &settings, sizeof(LidarEmulatorSettings)) ==
               sizeof(LidarEmulatorSettings)) {
            lidaremulator_set_tx_pin(app, settings.tx_pin);
            if(settings.tx_pin < FuriHalInfraredTxPinMax) {
                lidaremulator_enable_otg(app, settings.otg_enabled);
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void lidaremulator_save_settings(LidarEmulatorApp* app) {
    furi_check(app);

    LidarEmulatorSettings settings = {
        .tx_pin = app->app_state.tx_pin,
        .otg_enabled = app->app_state.is_otg_enabled,
    };

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, LIDAREMULATOR_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t magic = LIDAREMULATOR_SETTINGS_MAGIC;
        uint8_t version = LIDAREMULATOR_SETTINGS_VERSION;

        storage_file_write(file, &magic, 1);
        storage_file_write(file, &version, 1);
        storage_file_write(file, &settings, sizeof(LidarEmulatorSettings));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
