/* 
 * This file is part of the 8-bit ATARI FDD Emulator for Flipper Zero 
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <storage/storage.h>

#include "app_common.h"
#include "app_config.h"
#include "slice.h"

#define CONFIG_FILE_NAME APP_DATA_PATH("config.ini")

#define SECTION_APP "app"

#define KEY_LED_BLINKING "ledBlinking"

#define SECTION_FDD "fdd."

#define KEY_FDD_TYPE   "fddType"
#define KEY_DISK_IMAGE "diskImage"

void app_config_init(AppConfig* config) {
    furi_check(config != NULL);
    memset(config, 0, sizeof(AppConfig));

    config->led_blinking = true;
    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        config->fdd[i].type = FddType_ATARI_1050;
        config->fdd[i].image = furi_string_alloc();
    }
}

void app_config_free(AppConfig* config) {
    if(config != NULL) {
        for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
            if(config->fdd[i].image != NULL) {
                furi_string_free(config->fdd[i].image);
            }
        }
    }
}

const char* fdd_type_name(FddType fdd_type) {
    switch(fdd_type) {
    case FddType_ATARI_1050:
        return "A1050";
    case FddType_ATARI_XF551:
        return "XF551";
    default:
        return "Unknown";
    }
}

#define ini_add_comment(s, comment)      furi_string_cat_printf(s, "# %s\n", comment)
#define ini_add_section(s, section)      furi_string_cat_printf(s, "[%s]\n", section)
#define ini_add_keyval(s, key, fmt, val) furi_string_cat_printf(s, "%s = " fmt "\n", key, val)
#define ini_add_empty_line(s)            furi_string_cat_printf(s, "\n")

#define ini_add_fdd_section(s, i) furi_string_cat_printf(s, "[%s%d]\n", SECTION_FDD, i)

static FuriString* app_config_build(const AppConfig* config) {
    FuriString* s = furi_string_alloc();

    ini_add_section(s, SECTION_APP);
    ini_add_keyval(s, KEY_LED_BLINKING, "%s", config->led_blinking ? "1" : "0");
    ini_add_empty_line(s);

    for(size_t i = 0; i < FDD_EMULATOR_COUNT; i++) {
        ini_add_fdd_section(s, i);
        ini_add_keyval(s, KEY_FDD_TYPE, "%s", fdd_type_name(config->fdd[i].type));
        ini_add_keyval(s, KEY_DISK_IMAGE, "%s", furi_string_get_cstr(config->fdd[i].image));
        ini_add_empty_line(s);
    }

    return s;
}

#define section(str) slice_equals_cstr(section, str)
#define key(str)     slice_equals_cstr(key, str)

#define section_starts_with(str) slice_starts_with_cstr(section, str)

static void app_config_set(AppConfig* config, Slice section, Slice key, Slice value) {
    if(section(SECTION_APP)) {
        if(key(KEY_LED_BLINKING)) {
            uint32_t temp;
            if(slice_parse_uint32(value, 0, &temp)) {
                config->led_blinking = temp != 0;
                FURI_LOG_I(TAG, "led_blinking=%d", config->led_blinking);
            } else {
                FURI_LOG_E(TAG, "Failed to parse 'led_blinking' value");
            }
        } else {
            FURI_LOG_E(TAG, "Unknown key");
        }
    } else if(section_starts_with(SECTION_FDD)) {
        section.start += strlen(SECTION_FDD);
        uint32_t fdd_idx;
        if(!slice_parse_uint32(section, 0, &fdd_idx) || fdd_idx >= FDD_EMULATOR_COUNT) {
            FURI_LOG_E(TAG, "Invalid 'fdd' index");
            return;
        }

        if(key(KEY_FDD_TYPE)) {
            bool found = false;
            for(FddType i = 0; i < FddType_count; i++) {
                if(slice_equals_cstr(value, fdd_type_name(i))) {
                    config->fdd[fdd_idx].type = i;
                    found = true;
                    break;
                }
            }
            if(found) {
                FURI_LOG_I(
                    TAG, "fdd[%lu].type=%s", fdd_idx, fdd_type_name(config->fdd[fdd_idx].type));
            } else {
                FURI_LOG_E(TAG, "Unknown fdd type");
            }
        } else if(key(KEY_DISK_IMAGE)) {
            furi_string_set_strn(config->fdd[fdd_idx].image, value.start, slice_len(value));
            FURI_LOG_I(
                TAG,
                "fdd[%lu].image=%s",
                fdd_idx,
                furi_string_get_cstr(config->fdd[fdd_idx].image));
        } else {
            FURI_LOG_E(TAG, "Unknown key");
        }
    }
}

static void app_config_parse(AppConfig* config, Slice text) {
    Slice section = slice_empty();
    while(!slice_is_empty(text)) {
        Slice line = slice_trim_left(tok_until_eol(&text));
        if(slice_is_empty(line) || tok_skip_char(&line, '#')) {
            // Skip line
        } else if(tok_skip_char(&line, '[')) {
            // Change section
            section = slice_trim(tok_until_char(&line, ']'));
        } else {
            // Parse key-value pair
            Slice key = slice_trim(tok_until_char(&line, '='));
            Slice value = slice_empty();
            if(tok_skip_char(&line, '=')) {
                value = slice_trim(line);
            }
            app_config_set(config, section, key, value);
        }
        tok_skip_eol(&text);
    }
}

void app_config_save(const AppConfig* config, Storage* storage) {
    furi_check(config != NULL);
    furi_check(storage != NULL);

    FURI_LOG_I(TAG, "Storing configuration...");

    File* file = storage_file_alloc(storage);
    furi_check(file != NULL, "Failed to allocate file");

    if(storage_file_open(file, CONFIG_FILE_NAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* config_string = app_config_build(config);
        const char* cstr = furi_string_get_cstr(config_string);
        size_t cstr_len = furi_string_size(config_string);
        if(storage_file_write(file, cstr, cstr_len) != cstr_len) {
            FURI_LOG_E(TAG, "Failed to write file %s", CONFIG_FILE_NAME);
        }
        furi_string_free(config_string);
        FURI_LOG_I(TAG, "Configuration stored to %s", CONFIG_FILE_NAME);
    } else {
        FURI_LOG_E(TAG, "Failed to open file %s", CONFIG_FILE_NAME);
    }

    storage_file_close(file);
    storage_file_free(file);
}

void app_config_load(AppConfig* config, Storage* storage) {
    furi_check(config != NULL);
    furi_check(storage != NULL);

    FURI_LOG_I(TAG, "Loading configuration...");

    File* file = storage_file_alloc(storage);
    furi_check(file != NULL, "Failed to allocate file");

    if(storage_file_open(file, CONFIG_FILE_NAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t buff_size = storage_file_size(file);
        char* buff = malloc(buff_size);

        if(storage_file_read(file, buff, buff_size) == buff_size) {
            Slice text = {buff, buff + buff_size};
            FURI_LOG_I(TAG, "Parsing configuration...");
            app_config_parse(config, text);
        } else {
            FURI_LOG_E(TAG, "Failed to read config file %s", CONFIG_FILE_NAME);
        }

        free(buff);
    } else {
        FURI_LOG_E(TAG, "Failed to open config file %s", CONFIG_FILE_NAME);
    }

    storage_file_close(file);
    storage_file_free(file);

    FURI_LOG_I(TAG, "Configuration loaded");
}
