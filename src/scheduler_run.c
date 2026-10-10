#include "scheduler_run.h"

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <devices/cc1101_int/cc1101_int_interconnect.h>
#include <devices/devices.h>
#include <furi_hal_power.h>
#include <protocols/raw.h>
#include <subghz_protocol_registry.h>
#include <toolbox/stream/stream.h>
#include <transmitter.h>

#define TAG "SubGhzSchedulerRun"

struct ScheduleTxRun {
    Storage* storage;
    FlipperFormat* fff_head;
    FlipperFormat* fff_file;
    FlipperFormat* fff_data;
    SubGhzEnvironment* environment;
    FuriString* protocol;
    FuriString* preset;
    FuriString* data;
    uint32_t frequency;
    uint16_t tx_delay_ms;
    bool filetype;
};

static ScheduleTxRun* tx_run_alloc() {
    ScheduleTxRun* tx_run = calloc(1, sizeof(ScheduleTxRun));
    tx_run->storage = furi_record_open(RECORD_STORAGE);
    tx_run->fff_head = flipper_format_file_alloc(tx_run->storage);
    tx_run->fff_file = flipper_format_file_alloc(tx_run->storage);
    tx_run->fff_data = flipper_format_string_alloc();
    tx_run->environment = subghz_environment_alloc();
    tx_run->protocol = furi_string_alloc();
    tx_run->preset = furi_string_alloc();
    tx_run->data = furi_string_alloc();
    return tx_run;
}

static void tx_run_free(ScheduleTxRun* tx_run) {
    flipper_format_free(tx_run->fff_file);
    flipper_format_file_close(tx_run->fff_head);
    flipper_format_free(tx_run->fff_head);
    flipper_format_free(tx_run->fff_data);
    furi_string_free(tx_run->preset);
    furi_string_free(tx_run->protocol);
    furi_string_free(tx_run->data);
    subghz_environment_free(tx_run->environment);
    furi_record_close(RECORD_STORAGE);
    free(tx_run);
}

static FuriHalSubGhzPreset scheduler_get_subghz_preset_name(const char* preset_name) {
    FuriHalSubGhzPreset preset = FuriHalSubGhzPresetIDLE;
    if(!strcmp(preset_name, "FuriHalSubGhzPresetOok270Async")) {
        preset = FuriHalSubGhzPresetOok270Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetOok650Async")) {
        preset = FuriHalSubGhzPresetOok650Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev238Async")) {
        preset = FuriHalSubGhzPreset2FSKDev238Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPreset2FSKDev476Async")) {
        preset = FuriHalSubGhzPreset2FSKDev476Async;
    } else if(!strcmp(preset_name, "FuriHalSubGhzPresetCustom")) {
        preset = FuriHalSubGhzPresetCustom;
    } else {
        FURI_LOG_E(TAG, "Unknown or incompatible preset");
    }
    return preset;
}

static const char* const radio_device_name[RADIO_DEVICE_COUNT] = {
    SUBGHZ_DEVICE_CC1101_INT_NAME,
    SUBGHZ_DEVICE_CC1101_EXT_NAME,
};

// Sleep in small steps so an abort request is picked up quickly
static void scheduler_tx_delay(SchedulerApp* app, uint32_t ms) {
    while(ms && !app->tx_abort) {
        const uint32_t step = MIN(ms, (uint32_t)10);
        furi_delay_ms(step);
        ms -= step;
    }
}

static void
    transmit(SchedulerApp* app, const SubGhzDevice* device, SubGhzTransmitter* transmitter) {
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    if(subghz_devices_start_async_tx(device, subghz_transmitter_yield, transmitter)) {
        while(!subghz_devices_is_async_complete_tx(device) && !app->tx_abort) {
            furi_delay_ms(5);
        }
        subghz_devices_stop_async_tx(device);
    }

    subghz_devices_idle(device);
    subghz_transmitter_stop(transmitter);
    subghz_transmitter_free(transmitter);
    notification_message(app->notifications, &sequence_blink_stop);
}

// Send the .sub file in tx_run->data tx_count + 1 times
static bool
    scheduler_tx_send_file(SchedulerApp* app, ScheduleTxRun* tx_run, const SubGhzDevice* device) {
    if(!flipper_format_file_open_existing(tx_run->fff_file, furi_string_get_cstr(tx_run->data))) {
        FURI_LOG_E(TAG, "Error loading file: %s", furi_string_get_cstr(tx_run->data));
        flipper_format_file_close(tx_run->fff_file);
        return false;
    }

    flipper_format_read_uint32(tx_run->fff_file, "Frequency", &tx_run->frequency, 1);
    flipper_format_read_string(tx_run->fff_file, "Preset", tx_run->preset);
    flipper_format_read_string(tx_run->fff_file, "Protocol", tx_run->protocol);

    // Start from an empty buffer, otherwise playlist entries pile up
    stream_clean(flipper_format_get_raw_stream(tx_run->fff_data));
    if(furi_string_equal(tx_run->protocol, "RAW")) {
        subghz_protocol_raw_gen_fff_data(
            tx_run->fff_data, furi_string_get_cstr(tx_run->data), subghz_devices_get_name(device));
    } else {
        stream_copy_full(
            flipper_format_get_raw_stream(tx_run->fff_file),
            flipper_format_get_raw_stream(tx_run->fff_data));
    }
    flipper_format_file_close(tx_run->fff_file);

    const uint8_t tx_count = scheduler_get_tx_count(app->scheduler);
    for(uint_fast8_t i = 0; i <= tx_count && !app->tx_abort; ++i) {
        subghz_environment_set_protocol_registry(
            tx_run->environment, (void*)&subghz_protocol_registry);
        SubGhzTransmitter* transmitter = subghz_transmitter_alloc_init(
            tx_run->environment, furi_string_get_cstr(tx_run->protocol));

        subghz_transmitter_deserialize(transmitter, tx_run->fff_data);

        FuriHalSubGhzPreset preset_enum =
            scheduler_get_subghz_preset_name(furi_string_get_cstr(tx_run->preset));
        subghz_devices_load_preset(device, preset_enum, NULL);

        tx_run->frequency = subghz_devices_set_frequency(device, tx_run->frequency);

        transmit(app, device, transmitter);

        scheduler_tx_delay(app, tx_run->tx_delay_ms);
    }

    return true;
}

static int32_t scheduler_tx(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    ScheduleTxRun* tx_run = tx_run_alloc();
    int32_t result = 0;

    // Only block charging while actually transmitting, so long schedules can charge in between
    furi_hal_power_suppress_charge_enter();

    const bool radio = scheduler_get_radio(app->scheduler);
    const SubGhzDevice* device = subghz_devices_get_by_name(radio_device_name[radio]);
    subghz_devices_begin(device);
    subghz_devices_reset(device);

    const int16_t line = app->tx_line;
    bool ok = true;

    tx_run->filetype = scheduler_get_file_type(app->scheduler);
    if(tx_run->filetype == SchedulerFileTypePlaylist) {
        ok = flipper_format_file_open_existing(
            tx_run->fff_head, furi_string_get_cstr(app->tx_file_path));
        // Read up to the requested line, or the first one when sending everything
        for(int16_t i = 0; ok && i <= MAX(line, 0); ++i) {
            ok = flipper_format_read_string(tx_run->fff_head, "sub", tx_run->data);
        }
        if(!ok) {
            FURI_LOG_E(TAG, "Playlist line %d not found", line);
        }
    } else {
        furi_string_set_str(tx_run->data, furi_string_get_cstr(app->tx_file_path));
    }

    tx_run->tx_delay_ms = scheduler_get_tx_delay_ms(app->scheduler);
    if(ok) {
        do {
            if(!scheduler_tx_send_file(app, tx_run, device)) {
                result = -1;
                break;
            }
        } while(line < 0 && tx_run->filetype == SchedulerFileTypePlaylist && !app->tx_abort &&
                flipper_format_read_string(tx_run->fff_head, "sub", tx_run->data));
    } else {
        result = -1;
    }

    subghz_devices_sleep(device);
    subghz_devices_end(device);
    furi_hal_power_suppress_charge_exit();
    tx_run_free(tx_run);

    return result;
}

void scheduler_start_tx(SchedulerApp* app, int16_t line) {
    if(app->thread || app->is_transmitting) {
        return;
    }
    app->tx_abort = false;
    app->tx_line = line;
    app->is_transmitting = true;
    app->thread = furi_thread_alloc_ex("SchedulerTxThread", 2048, scheduler_tx, app);
    furi_thread_start(app->thread);
}

bool scheduler_tx_finished(SchedulerApp* app) {
    return app->thread && furi_thread_get_state(app->thread) == FuriThreadStateStopped;
}

void scheduler_cleanup_tx(SchedulerApp* app) {
    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }
    app->is_transmitting = false;
}

void scheduler_stop_tx(SchedulerApp* app) {
    app->tx_abort = true;
    scheduler_cleanup_tx(app);
    app->tx_abort = false;
}
