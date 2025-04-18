#include <flip_world.h>
#include <flip_storage/storage.h>
char* fps_choices_str[] = {"30", "60", "120", "240"};
uint8_t fps_index = 0;
char* yes_or_no_choices[] = {"No", "Yes"};
uint8_t screen_always_on_index = 1;
uint8_t sound_on_index = 1;
uint8_t vibration_on_index = 1;
char* player_sprite_choices[] = {"naked", "sword", "axe", "bow"};
uint8_t player_sprite_index = 1;
char* vgm_levels[] = {"-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
uint8_t vgm_x_index = 2;
uint8_t vgm_y_index = 2;
uint8_t game_mode_index = 0;
float atof_(const char* nptr) {
    return (float)strtod(nptr, NULL);
}
float atof_furi(const FuriString* nptr) {
    return atof_(furi_string_get_cstr(nptr));
}
bool is_str(const char* src, const char* dst) {
    return strcmp(src, dst) == 0;
}
bool is_enough_heap(size_t heap_size, bool check_blocks) {
    const size_t min_heap = heap_size + 1024; // 1KB buffer
    const size_t min_free = memmgr_get_free_heap();
    if(min_free < min_heap) {
        FURI_LOG_E(TAG, "Not enough heap memory: There are %zu bytes free.", min_free);
        return false;
    }
    if(check_blocks) {
        const size_t max_free_block = memmgr_heap_get_max_free_block();
        if(max_free_block < min_heap) {
            FURI_LOG_E(TAG, "Not enough free blocks: %zu bytes", max_free_block);
            return false;
        }
    }
    return true;
}

static bool flip_world_json_to_datetime(DateTime* rtc_time, FuriString* str) {
    if(!rtc_time || !str) {
        FURI_LOG_E(TAG, "rtc_time or str is NULL");
        return false;
    }
    FuriString* hour = get_json_value_furi("hour", str);
    if(hour) {
        rtc_time->hour = atoi(furi_string_get_cstr(hour));
        furi_string_free(hour);
    }
    FuriString* minute = get_json_value_furi("minute", str);
    if(minute) {
        rtc_time->minute = atoi(furi_string_get_cstr(minute));
        furi_string_free(minute);
    }
    FuriString* second = get_json_value_furi("second", str);
    if(second) {
        rtc_time->second = atoi(furi_string_get_cstr(second));
        furi_string_free(second);
    }
    FuriString* day = get_json_value_furi("day", str);
    if(day) {
        rtc_time->day = atoi(furi_string_get_cstr(day));
        furi_string_free(day);
    }
    FuriString* month = get_json_value_furi("month", str);
    if(month) {
        rtc_time->month = atoi(furi_string_get_cstr(month));
        furi_string_free(month);
    }
    FuriString* year = get_json_value_furi("year", str);
    if(year) {
        rtc_time->year = atoi(furi_string_get_cstr(year));
        furi_string_free(year);
    }
    FuriString* weekday = get_json_value_furi("weekday", str);
    if(weekday) {
        rtc_time->weekday = atoi(furi_string_get_cstr(weekday));
        furi_string_free(weekday);
    }
    return datetime_validate_datetime(rtc_time);
}

static FuriString* flip_world_datetime_to_json(DateTime* rtc_time) {
    if(!rtc_time) {
        FURI_LOG_E(TAG, "rtc_time is NULL");
        return NULL;
    }
    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{\"hour\":%d,\"minute\":%d,\"second\":%d,\"day\":%d,\"month\":%d,\"year\":%d,\"weekday\":%d}",
        rtc_time->hour,
        rtc_time->minute,
        rtc_time->second,
        rtc_time->day,
        rtc_time->month,
        rtc_time->year,
        rtc_time->weekday);
    return furi_string_alloc_set_str(json);
}

static bool flip_world_save_rtc_time(DateTime* rtc_time) {
    if(!rtc_time) {
        FURI_LOG_E(TAG, "rtc_time is NULL");
        return false;
    }
    FuriString* json = flip_world_datetime_to_json(rtc_time);
    if(!json) {
        FURI_LOG_E(TAG, "Failed to convert DateTime to JSON");
        return false;
    }
    save_char("last_checked", furi_string_get_cstr(json));
    furi_string_free(json);
    return true;
}

//
// Returns true if time_current is one hour (or more) later than the stored last_checked time
//
static bool flip_world_is_update_time(DateTime* time_current) {
    if(!time_current) {
        FURI_LOG_E(TAG, "time_current is NULL");
        return false;
    }
    char last_checked_old[128];
    if(!load_char("last_checked", last_checked_old, sizeof(last_checked_old))) {
        FURI_LOG_E(TAG, "Failed to load last_checked");
        FuriString* json = flip_world_datetime_to_json(time_current);
        if(json) {
            save_char("last_checked", furi_string_get_cstr(json));
            furi_string_free(json);
        }
        return false;
    }

    DateTime last_updated_time;

    FuriString* last_updated_furi = char_to_furi_string(last_checked_old);
    if(!last_updated_furi) {
        FURI_LOG_E(TAG, "Failed to convert char to FuriString");
        return false;
    }
    if(!flip_world_json_to_datetime(&last_updated_time, last_updated_furi)) {
        FURI_LOG_E(TAG, "Failed to convert JSON to DateTime");
        furi_string_free(last_updated_furi);
        return false;
    }
    furi_string_free(last_updated_furi); // Free after usage.

    bool time_diff = false;
    // If the date is different assume more than one hour has passed.
    if(time_current->year != last_updated_time.year ||
       time_current->month != last_updated_time.month ||
       time_current->day != last_updated_time.day) {
        time_diff = true;
    } else {
        // For the same day, compute seconds from midnight.
        int seconds_current =
            time_current->hour * 3600 + time_current->minute * 60 + time_current->second;
        int seconds_last = last_updated_time.hour * 3600 + last_updated_time.minute * 60 +
                           last_updated_time.second;
        if((seconds_current - seconds_last) >= 3600) {
            time_diff = true;
        }
    }

    return time_diff;
}

// Sends a request to fetch the last updated date of the app.
static bool flip_world_last_app_update(FlipperHTTP* fhttp, bool flipper_server) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    char url[256];
    if(flipper_server) {
        // make sure folder is created
        char directory_path[256];
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world");

        // Create the directory
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_mkdir(storage, directory_path);
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data");
        storage_common_mkdir(storage, directory_path);
        snprintf(
            directory_path,
            sizeof(directory_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/last_update_request.txt");
        storage_simply_remove_recursive(storage, directory_path); // ensure the file is empty
        furi_record_close(RECORD_STORAGE);

        fhttp->save_received_data = false;
        fhttp->is_bytes_request = true;

        snprintf(
            fhttp->file_path,
            sizeof(fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/last_update_request.txt");
        snprintf(
            url,
            sizeof(url),
            "https://raw.githubusercontent.com/flipperdevices/flipper-application-catalog/main/applications/GPIO/flip_world/manifest.yml");
        return flipper_http_request(
            fhttp, BYTES, url, "{\"Content-Type\":\"application/json\"}", NULL);
    } else {
        snprintf(
            url, sizeof(url), "https://www.jblanked.com/flipper/api/app/last-updated/flip_world/");
        return flipper_http_request(
            fhttp, GET, url, "{\"Content-Type\":\"application/json\"}", NULL);
    }
}

/*
 * Scans a NUL‐terminated YAML buffer for a top‐level “version:” key,
 * and writes its (unquoted) value into out_version.
 * Returns true on success.
 */
static bool parse_yaml_version(const char* yaml, char* out_version, size_t out_len) {
    const char* p = strstr(yaml, "\nversion:");
    if(!p) {
        // maybe it's the very first line
        p = yaml;
    } else {
        // skip the “\n”
        p++;
    }
    // skip the key name and colon
    p = strstr(p, "version");
    if(!p) return false;
    p += strlen("version");
    // skip whitespace and colon
    while(*p == ' ' || *p == ':')
        p++;
    // handle optional quote
    bool quoted = (*p == '"');
    if(quoted) p++;
    // copy up until end‐quote or newline/space
    size_t i = 0;
    while(*p && i + 1 < out_len) {
        if((quoted && *p == '"') || (!quoted && (*p == '\n' || *p == ' '))) {
            break;
        }
        out_version[i++] = *p++;
    }
    out_version[i] = '\0';
    return (i > 0);
}

// Parses the server response and returns true if an update is available.
static bool flip_world_parse_last_app_update(
    FlipperHTTP* fhttp,
    DateTime* time_current,
    bool flipper_server) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    if(fhttp->state == ISSUE) {
        FURI_LOG_E(TAG, "Failed to fetch last app update");
        return false;
    }
    char version_str[32];
    if(!flipper_server) {
        if(fhttp->last_response == NULL || strlen(fhttp->last_response) == 0) {
            FURI_LOG_E(TAG, "fhttp->last_response is NULL or empty");
            return false;
        }

        char* app_version = get_json_value("version", fhttp->last_response);
        if(app_version) {
            // Save the server app version: it should save something like: 0.8
            save_char("server_app_version", app_version);
            snprintf(version_str, sizeof(version_str), "%s", app_version);
            free(app_version);
        } else {
            FURI_LOG_E(TAG, "Failed to get app version");
            return false;
        }
    } else {
        FuriString* manifest_data = flipper_http_load_from_file(fhttp->file_path);
        if(!manifest_data) {
            FURI_LOG_E(TAG, "Failed to load app data");
            return false;
        }
        // parse version out of the YAML
        if(!parse_yaml_version(
               furi_string_get_cstr(manifest_data), version_str, sizeof(version_str))) {
            FURI_LOG_E(TAG, "Failed to parse version from YAML manifest");
            return false;
        }
        save_char("server_app_version", version_str);
        furi_string_free(manifest_data);
    }
    // Only check for an update if an hour or more has passed.
    if(flip_world_is_update_time(time_current)) {
        char app_version[32];
        if(!load_char("app_version", app_version, sizeof(app_version))) {
            FURI_LOG_E(TAG, "Failed to load app version");
            return false;
        }
        // Check if the app version is different from the server version.
        if(!is_str(app_version, version_str)) {
            easy_flipper_dialog(
                "Update available", "New update available!\nPress BACK to download.");
            return true; // Update available.
        }
        FURI_LOG_I(TAG, "No update available");
        return false; // No update available.
    }
    FURI_LOG_I(TAG, "Not enough time has passed since the last update check");
    return false; // Not yet time to update.
}

static bool flip_world_get_fap_file(FlipperHTTP* fhttp, bool flipper_server) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL.");
        return false;
    }
    char url[256];
    fhttp->save_received_data = false;
    fhttp->is_bytes_request = true;
#ifndef FW_ORIGIN_Momentum
    snprintf(
        fhttp->file_path,
        sizeof(fhttp->file_path),
        STORAGE_EXT_PATH_PREFIX "/apps/GPIO/flip_world.fap");
#else
    snprintf(
        fhttp->file_path,
        sizeof(fhttp->file_path),
        STORAGE_EXT_PATH_PREFIX "/apps/GPIO/FlipperHTTP/flip_world.fap");
#endif
    if(flipper_server) {
        char build_id[32];
        snprintf(build_id, sizeof(build_id), "%s", BUILD_ID);
        uint8_t target;
        target = furi_hal_version_get_hw_target();
        uint16_t api_major, api_minor;
        furi_hal_info_get_api_version(&api_major, &api_minor);
        snprintf(
            url,
            sizeof(url),
            "https://catalog.flipperzero.one/api/v0/application/version/%s/build/compatible?target=f%d&api=%d.%d",
            build_id,
            target,
            api_major,
            api_minor);
    } else {
        snprintf(
            url, sizeof(url), "https://www.jblanked.com/flipper/api/app/download/flip_world/");
    }
    return flipper_http_request(
        fhttp, BYTES, url, "{\"Content-Type\": \"application/octet-stream\"}", NULL);
}

// Updates the app. Uses the supplied current time for validating if update check should proceed.
static bool
    flip_world_update_app(FlipperHTTP* fhttp, DateTime* time_current, bool use_flipper_api) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    if(!flip_world_last_app_update(fhttp, use_flipper_api)) {
        FURI_LOG_E(TAG, "Failed to fetch last app update");
        return false;
    }
    fhttp->state = RECEIVING;
    furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);
    while(fhttp->state == RECEIVING && furi_timer_is_running(fhttp->get_timeout_timer) > 0) {
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp->get_timeout_timer);
    if(flip_world_parse_last_app_update(fhttp, time_current, use_flipper_api)) {
        if(!flip_world_get_fap_file(fhttp, false)) {
            FURI_LOG_E(TAG, "Failed to fetch fap file 1");
            return false;
        }
        fhttp->state = RECEIVING;

        while(fhttp->state == RECEIVING) {
            furi_delay_ms(100);
        }

        if(fhttp->state == ISSUE) {
            FURI_LOG_E(TAG, "Failed to fetch fap file 2");
            easy_flipper_dialog(
                "Update Error", "Failed to download the\nupdate file.\nPlease try again.");
            return false;
        }
        return true;
    }
    return false; // No update available.
}

// Handles the app update routine. This function obtains the current RTC time,
// checks the "last_checked" value, and if it is more than one hour old, calls for an update.
bool flip_world_handle_app_update(FlipperHTTP* fhttp, bool use_flipper_api) {
    if(!fhttp) {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    DateTime rtc_time;
    furi_hal_rtc_get_datetime(&rtc_time);
    char last_checked[32];
    if(!load_char("last_checked", last_checked, sizeof(last_checked))) {
        // First time – save the current time and check for an update.
        if(!flip_world_save_rtc_time(&rtc_time)) {
            FURI_LOG_E(TAG, "Failed to save RTC time");
            return false;
        }
        return flip_world_update_app(fhttp, &rtc_time, use_flipper_api);
    } else {
        // Check if the current RTC time is at least one hour past the stored time.
        if(flip_world_is_update_time(&rtc_time)) {
            if(!flip_world_update_app(fhttp, &rtc_time, use_flipper_api)) {
                // save the last_checked for the next check.
                if(!flip_world_save_rtc_time(&rtc_time)) {
                    FURI_LOG_E(TAG, "Failed to save RTC time");
                    return false;
                }
                return false;
            }
            // Save the current time for the next check.
            if(!flip_world_save_rtc_time(&rtc_time)) {
                FURI_LOG_E(TAG, "Failed to save RTC time");
                return false;
            }
            return true;
        }
        return false; // No update necessary.
    }
}
