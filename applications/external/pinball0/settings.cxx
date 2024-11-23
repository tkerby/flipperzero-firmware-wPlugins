#include <flipper_format/flipper_format.h>

#include "settings.h"

#define PINBALL_SETTINGS_FILENAME     ".pinball0.conf"
#define PINBALL_SETTINGS_PATH         APP_DATA_PATH(PINBALL_SETTINGS_FILENAME)
#define PINBALL_SETTINGS_FILE_TYPE    "Pinball0 Settings File"
#define PINBALL_SETTINGS_FILE_VERSION 1

void pinball_load_settings(PinballApp* pb) {
    FlipperFormat* fff_settings = flipper_format_file_alloc(pb->storage);
    FuriString* tmp_str = furi_string_alloc();
    uint32_t tmp_data32 = 0;

    // init the settings to default values, then overwrite them if found in the settings file
    pb->settings.sound_enabled = true;
    pb->settings.led_enabled = true;
    pb->settings.vibrate_enabled = true;
    pb->settings.debug_mode = false;
    pb->selected_setting = 0;
    pb->max_settings = 4;

    do {
        if(!flipper_format_file_open_existing(fff_settings, PINBALL_SETTINGS_PATH)) {
            FURI_LOG_I(TAG, "SETTINGS: File not found, using defaults");
            break;
        }
        if(!flipper_format_read_header(fff_settings, tmp_str, &tmp_data32)) {
            FURI_LOG_E(TAG, "SETTINGS: Missing or incorrect header");
            break;
        }
        if(!strcmp(furi_string_get_cstr(tmp_str), PINBALL_SETTINGS_FILE_TYPE) &&
           (tmp_data32 == PINBALL_SETTINGS_FILE_VERSION)) {
        } else {
            FURI_LOG_E(TAG, "SETTINGS: Type or version mismatch");
            break;
        }
        if(flipper_format_read_uint32(fff_settings, "Sound", &tmp_data32, 1)) {
            pb->settings.sound_enabled = (tmp_data32 == 0) ? false : true;
        }
        if(flipper_format_read_uint32(fff_settings, "LED", &tmp_data32, 1)) {
            pb->settings.led_enabled = (tmp_data32 == 0) ? false : true;
        }
        if(flipper_format_read_uint32(fff_settings, "Vibrate", &tmp_data32, 1)) {
            pb->settings.vibrate_enabled = (tmp_data32 == 0) ? false : true;
        }
        if(flipper_format_read_uint32(fff_settings, "Debug", &tmp_data32, 1)) {
            pb->settings.debug_mode = (tmp_data32 == 0) ? false : true;
        }

    } while(false);

    furi_string_free(tmp_str);
    flipper_format_free(fff_settings);
}

void pinball_save_settings(PinballApp* pb) {
    FlipperFormat* fff_settings = flipper_format_file_alloc(pb->storage);
    uint32_t tmp_data32 = 0;
    FURI_LOG_I(TAG, "SETTINGS: Saving settings");
    do {
        if(!flipper_format_file_open_always(fff_settings, PINBALL_SETTINGS_PATH)) {
            FURI_LOG_E(TAG, "SETTINGS: Unable to open file for save!");
            break;
        }
        if(!flipper_format_write_header_cstr(
               fff_settings, PINBALL_SETTINGS_FILE_TYPE, PINBALL_SETTINGS_FILE_VERSION)) {
            FURI_LOG_E(TAG, "SETTINGS: Failed writing file type and version");
            break;
        }
        // now write out our settings data
        tmp_data32 = pb->settings.sound_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(fff_settings, "Sound", &tmp_data32, 1)) {
            FURI_LOG_E(TAG, "SETTINGS: Failed to write 'Sound'");
            break;
        }
        tmp_data32 = pb->settings.led_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(fff_settings, "LED", &tmp_data32, 1)) {
            FURI_LOG_E(TAG, "SETTINGS: Failed to write 'LED'");
            break;
        }
        tmp_data32 = pb->settings.vibrate_enabled ? 1 : 0;
        if(!flipper_format_write_uint32(fff_settings, "Vibrate", &tmp_data32, 1)) {
            FURI_LOG_E(TAG, "SETTINGS: Failed to write 'Vibrate'");
            break;
        }
        tmp_data32 = pb->settings.debug_mode ? 1 : 0;
        if(!flipper_format_write_uint32(fff_settings, "Debug", &tmp_data32, 1)) {
            FURI_LOG_E(TAG, "SETTINGS: Failed to write 'Debug'");
            break;
        }
    } while(false);

    flipper_format_file_close(fff_settings);
    flipper_format_free(fff_settings);
}
