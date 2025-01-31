#include "scheduler_run.h"

#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>

#include <devices/cc1101_int/cc1101_int_interconnect.h>
#include <devices/devices.h>

#include <subghz_protocol_registry.h>

#define TAG "SubGhzSchedulerRun"

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

static void
    transmit(SchedulerApp* app, const SubGhzDevice* device, SubGhzTransmitter* transmitter) {
    uint8_t repeats = scheduler_get_tx_repeats(app->scheduler);
    for(uint_fast8_t i = 0; i <= repeats; ++i) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Scheduled Tx %d of %d", i + 1, repeats + 1);
#endif
        if(subghz_devices_start_async_tx(device, subghz_transmitter_yield, transmitter)) {
            while(!subghz_devices_is_async_complete_tx(device)) {
                furi_delay_ms(10);
            }
            subghz_devices_stop_async_tx(device);
        }
    }
}

int32_t scheduler_tx(SchedulerApp* app) {
    furi_assert(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_head = flipper_format_file_alloc(storage);
    FlipperFormat* fff_file = flipper_format_file_alloc(storage);
    FlipperFormat* fff_data = flipper_format_string_alloc();
    SubGhzEnvironment* environment = subghz_environment_alloc();
    FuriString* protocol = furi_string_alloc();
    FuriString* preset = furi_string_alloc();
    FuriString* data = furi_string_alloc();
    bool filetype = scheduler_get_file_type(app->scheduler);
    uint32_t frequency;

    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    subghz_devices_begin(device);
    subghz_devices_reset(device);

    if(filetype == SchedulerFileTypePlaylist) {
        flipper_format_file_open_existing(fff_head, furi_string_get_cstr(app->file_path));
        flipper_format_read_string(fff_head, "sub", data);
    } else {
        furi_string_set_str(data, furi_string_get_cstr(app->file_path));
    }

    do {
        if(!flipper_format_file_open_existing(fff_file, furi_string_get_cstr(data))) {
            FURI_LOG_E(TAG, "Error loading file!");
            return -1;
        }

        flipper_format_read_uint32(fff_file, "Frequency", &frequency, 1);
        flipper_format_read_string(fff_file, "Preset", preset);
        flipper_format_read_string(fff_file, "Protocol", protocol);

        if(furi_string_equal(protocol, "RAW")) {
            subghz_protocol_raw_gen_fff_data(
                fff_data, furi_string_get_cstr(data), subghz_devices_get_name(device));
        } else {
            stream_copy_full(
                flipper_format_get_raw_stream(fff_file), flipper_format_get_raw_stream(fff_data));
        }
        flipper_format_file_close(fff_file);

        subghz_environment_set_protocol_registry(environment, (void*)&subghz_protocol_registry);
        SubGhzTransmitter* transmitter =
            subghz_transmitter_alloc_init(environment, furi_string_get_cstr(protocol));

        subghz_transmitter_deserialize(transmitter, fff_data);

        FuriHalSubGhzPreset preset_enum =
            scheduler_get_subghz_preset_name(furi_string_get_cstr(preset));
        subghz_devices_load_preset(device, preset_enum, NULL);

        frequency = subghz_devices_set_frequency(device, frequency);

        transmit(app, device, transmitter);
        subghz_transmitter_free(transmitter);
        if(filetype == SchedulerFileTypePlaylist) {
            furi_delay_ms(100);
        }
    } while(filetype == SchedulerFileTypePlaylist &&
            flipper_format_read_string(fff_head, "sub", data));

    subghz_devices_sleep(device);
    subghz_devices_end(device);

    flipper_format_free(fff_file);
    flipper_format_file_close(fff_head);
    flipper_format_free(fff_head);
    flipper_format_free(fff_data);
    furi_string_free(preset);
    furi_string_free(protocol);
    furi_string_free(data);
    subghz_environment_free(environment);
    furi_record_close(RECORD_STORAGE);

    return 0;
}
