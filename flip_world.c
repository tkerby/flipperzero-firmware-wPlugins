#include <flip_world.h>
#include <flip_storage/storage.h>
char *fps_choices_str[] = {"30", "60", "120", "240"};
uint8_t fps_index = 0;
char *yes_or_no_choices[] = {"No", "Yes"};
uint8_t screen_always_on_index = 1;
uint8_t sound_on_index = 1;
uint8_t vibration_on_index = 1;
char *player_sprite_choices[] = {"naked", "sword", "axe", "bow"};
uint8_t player_sprite_index = 1;
char *vgm_levels[] = {"-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
uint8_t vgm_x_index = 2;
uint8_t vgm_y_index = 2;
uint8_t game_mode_index = 0;
float atof_(const char *nptr) { return (float)strtod(nptr, NULL); }
float atof_furi(const FuriString *nptr) { return atof_(furi_string_get_cstr(nptr)); }
bool is_str(const char *src, const char *dst) { return strcmp(src, dst) == 0; }
bool is_enough_heap(size_t heap_size, bool check_blocks)
{
    const size_t min_heap = heap_size + 1024; // 1KB buffer
    const size_t min_free = memmgr_get_free_heap();
    if (min_free < min_heap)
    {
        FURI_LOG_E(TAG, "Not enough heap memory: There are %zu bytes free.", min_free);
        return false;
    }
    if (check_blocks)
    {
        const size_t max_free_block = memmgr_heap_get_max_free_block();
        if (max_free_block < min_heap)
        {
            FURI_LOG_E(TAG, "Not enough free blocks: %zu bytes", max_free_block);
            return false;
        }
    }
    return true;
}

static DateTime *flip_world_get_rtc_time()
{
    DateTime *rtc_time = malloc(sizeof(DateTime));
    if (!rtc_time)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for DateTime");
        return NULL;
    }
    furi_hal_rtc_get_datetime(rtc_time);
    return rtc_time;
}

static DateTime *flip_world_json_to_datetime(const char *str)
{
    DateTime *rtc_time = malloc(sizeof(DateTime));
    if (!rtc_time)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for DateTime");
        return NULL;
    }
    char *hour = get_json_value("hour", str);
    char *minute = get_json_value("minute", str);
    char *second = get_json_value("second", str);
    char *day = get_json_value("day", str);
    char *month = get_json_value("month", str);
    char *year = get_json_value("year", str);
    char *weekday = get_json_value("weekday", str);

    if (!hour || !minute || !second || !day || !month || !year || !weekday)
    {
        FURI_LOG_E(TAG, "Failed to parse JSON");
        free(rtc_time);
        return NULL;
    }

    rtc_time->hour = atoi(hour);
    rtc_time->minute = atoi(minute);
    rtc_time->second = atoi(second);
    rtc_time->day = atoi(day);
    rtc_time->month = atoi(month);
    rtc_time->year = atoi(year);
    rtc_time->weekday = atoi(weekday);

    free(hour);
    free(minute);
    free(second);
    free(day);
    free(month);
    free(year);
    free(weekday);

    return rtc_time;
}

static FuriString *flip_world_datetime_to_json(DateTime *rtc_time)
{
    if (!rtc_time)
    {
        FURI_LOG_E(TAG, "rtc_time is NULL");
        return NULL;
    }
    FuriString *json = furi_string_alloc();
    if (!json)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for JSON");
        return NULL;
    }
    furi_string_printf(
        json,
        "{\"hour\":%d,\"minute\":%d,\"second\":%d,\"day\":%d,\"month\":%d,\"year\":%d,\"weekday\":%d}",
        rtc_time->hour,
        rtc_time->minute,
        rtc_time->second,
        rtc_time->day,
        rtc_time->month,
        rtc_time->year,
        rtc_time->weekday);
    return json;
}

static bool flip_world_save_rtc_time(DateTime *rtc_time)
{
    if (!rtc_time)
    {
        FURI_LOG_E(TAG, "rtc_time is NULL");
        return false;
    }
    FuriString *json = flip_world_datetime_to_json(rtc_time);
    if (!json)
    {
        FURI_LOG_E(TAG, "Failed to convert DateTime to JSON");
        return false;
    }
    save_char("last_checked", furi_string_get_cstr(json));
    furi_string_free(json);
    return true;
}

//
// Returns true if time_current is one hour (or more) later than the stored last_updated time
//
static bool flip_world_is_update_time(DateTime *time_current)
{
    if (!time_current)
    {
        FURI_LOG_E(TAG, "time_current is NULL");
        return false;
    }
    char last_updated_old[32];
    if (!load_char("last_updated", last_updated_old, sizeof(last_updated_old)))
    {
        FURI_LOG_E(TAG, "Failed to load last_updated");
        FuriString *json = flip_world_datetime_to_json(time_current);
        if (json)
        {
            save_char("last_updated", furi_string_get_cstr(json));
            furi_string_free(json);
        }
        return false;
    }
    DateTime *last_updated_time = flip_world_json_to_datetime(last_updated_old);
    if (!last_updated_time)
    {
        FURI_LOG_E(TAG, "Failed to convert JSON to DateTime");
        return false;
    }

    bool time_diff = false;
    // If the date is different assume more than one hour has passed.
    if (time_current->year != last_updated_time->year ||
        time_current->month != last_updated_time->month ||
        time_current->day != last_updated_time->day)
    {
        time_diff = true;
    }
    else
    {
        // For the same day, compute seconds from midnight.
        int seconds_current = time_current->hour * 3600 + time_current->minute * 60 + time_current->second;
        int seconds_last = last_updated_time->hour * 3600 + last_updated_time->minute * 60 + last_updated_time->second;
        if ((seconds_current - seconds_last) >= 3600)
        {
            time_diff = true;
        }
    }

    if (time_diff)
    {
        FuriString *json = flip_world_datetime_to_json(time_current);
        if (json)
        {
            save_char("last_updated", furi_string_get_cstr(json));
            furi_string_free(json);
        }
    }
    free(last_updated_time);
    return time_diff;
}

// Sends a request to fetch the last updated date of the app.
static bool flip_world_last_app_update(FlipperHTTP *fhttp, bool flipper_server)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    return flipper_http_request(
        fhttp,
        GET,
        !flipper_server ? "https://www.jblanked.com/flipper/api/app/last-updated/flip_world/"
                        : "//--TODO--//",
        "{\"Content-Type\":\"application/json\"}",
        NULL);
}

// Parses the server response and returns true if an update is available.
static bool flip_world_parse_last_app_update(FlipperHTTP *fhttp, DateTime *time_current)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    if (fhttp->state == ISSUE)
    {
        FURI_LOG_E(TAG, "Failed to fetch last app update");
        return false;
    }
    if (fhttp->last_response == NULL || strlen(fhttp->last_response) == 0)
    {
        FURI_LOG_E(TAG, "fhttp->last_response is NULL or empty");
        return false;
    }

    // Save the server app version.
    save_char("server_app_version", fhttp->last_response);

    // Only check for an update if an hour or more has passed.
    if (flip_world_is_update_time(time_current))
    {
        char app_version[32];
        if (!load_char("app_version", app_version, sizeof(app_version)))
        {
            FURI_LOG_E(TAG, "Failed to load app version");
            return false;
        }
        if (strcmp(fhttp->last_response, app_version) != 0)
        {
            FURI_LOG_I(TAG, "Update available");
            return true;
        }
        else
        {
            FURI_LOG_I(TAG, "No update available");
            return false;
        }
    }
    return false; // Not yet time to update.
}

static bool flip_world_get_fap_file(FlipperHTTP *fhttp, bool flipper_server)
{
    if (!fhttp)
    {
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
    if (flipper_server)
    {
        char build_id[32];
        snprintf(build_id, sizeof(build_id), "%s", "//--TODO--//");
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
    }
    else
    {
        snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/app/download/flip_world");
    }
    return flipper_http_request(fhttp, BYTES, url, "{\"Content-Type\": \"application/octet-stream\"}", NULL);
}

// Updates the app. Uses the supplied current time for validating if update check should proceed.
static void flip_world_update_app(FlipperHTTP *fhttp, DateTime *time_current)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return;
    }
    if (!flip_world_last_app_update(fhttp, false))
    {
        FURI_LOG_E(TAG, "Failed to fetch last app update");
        return;
    }
    while (fhttp->state == RECEIVING && furi_timer_is_running(fhttp->get_timeout_timer) > 0)
    {
        // Wait for the request to finish.
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp->get_timeout_timer);
    if (flip_world_parse_last_app_update(fhttp, time_current))
    {
        easy_flipper_dialog("App Update", "FlipWorld update is available.\nPress BACK to install.");
        if (!flip_world_get_fap_file(fhttp, false))
        {
            FURI_LOG_E(TAG, "Failed to fetch fap file");
            return;
        }
        while (fhttp->state == RECEIVING && furi_timer_is_running(fhttp->get_timeout_timer) > 0)
        {
            furi_delay_ms(100);
        }
        furi_timer_stop(fhttp->get_timeout_timer);
        if (fhttp->state == ISSUE)
        {
            FURI_LOG_E(TAG, "Failed to fetch fap file");
            return;
        }
        easy_flipper_dialog("Update Done", "FlipWorld update is done.\nPress BACK to restart.");
    }
    else
    {
        FURI_LOG_I(TAG, "No update available");
    }
}

// Handles the app update routine. This function obtains the current RTC time,
// checks the "last_checked" value, and if it is more than one hour old, calls for an update.
bool flip_world_handle_app_update(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "fhttp is NULL");
        return false;
    }
    DateTime *rtc_time = flip_world_get_rtc_time();
    if (!rtc_time)
    {
        FURI_LOG_E(TAG, "Failed to get RTC time");
        return false;
    }
    char last_checked[32];
    if (!load_char("last_checked", last_checked, sizeof(last_checked)))
    {
        // First time – save the current time and check for an update.
        flip_world_save_rtc_time(rtc_time);
        flip_world_update_app(fhttp, rtc_time);
        free(rtc_time);
        return true;
    }
    else
    {
        // Check if the current RTC time is at least one hour past the stored time.
        if (flip_world_is_update_time(rtc_time))
        {
            flip_world_update_app(fhttp, rtc_time);
            free(rtc_time);
            return true;
        }
        free(rtc_time);
        return false; // No update necessary.
    }
}
