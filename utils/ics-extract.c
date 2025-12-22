#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Data Structures

typedef struct {
    FuriString* summary;        // Event title/summary
    FuriString* location;       // Event location
    FuriString* description;    // Event description
    DateTime start_time;        // Event start date/time
    DateTime end_time;          // Event end date/time
    bool all_day;               // True if event is all-day
} CalendarEvent;

typedef struct {
    CalendarEvent* events;  // Array of events
    size_t count;           // Current number of events
    size_t capacity;        // Allocated capacity
} CalendarEventList;

// Memory Management

CalendarEventList* calendar_event_list_alloc(size_t initial_capacity) {
    CalendarEventList* list = malloc(sizeof(CalendarEventList));
    list->events = malloc(sizeof(CalendarEvent) * initial_capacity);
    list->count = 0;
    list->capacity = initial_capacity;
    return list;
}

void calendar_event_list_free(CalendarEventList* list) {
    for(size_t i = 0; i < list->count; i++) {
        furi_string_free(list->events[i].summary);
        furi_string_free(list->events[i].location);
        furi_string_free(list->events[i].description);
    }
    free(list->events);
    free(list);
}

// Date / Time Parsing

static bool parse_ics_datetime(const char* ics_date, DateTime* dt) {
    if(!ics_date || strlen(ics_date) < 8) return false;

    char year[5] = {0}, month[3] = {0}, day[3] = {0};
    strncpy(year, ics_date, 4);
    strncpy(month, ics_date + 4, 2);
    strncpy(day, ics_date + 6, 2);

    dt->year = atoi(year);
    dt->month = atoi(month);
    dt->day = atoi(day);

    if(strlen(ics_date) >= 15 && ics_date[8] == 'T') {
        char hour[3] = {0}, minute[3] = {0}, second[3] = {0};
        strncpy(hour, ics_date + 9, 2);
        strncpy(minute, ics_date + 11, 2);
        strncpy(second, ics_date + 13, 2);

        dt->hour = atoi(hour);
        dt->minute = atoi(minute);
        dt->second = atoi(second);
        return false;
    } else {
        dt->hour = 0;
        dt->minute = 0;
        dt->second = 0;
        return true;
    }
}

static int datetime_compare(const DateTime* dt1, const DateTime* dt2) {
    if(dt1->year != dt2->year) return dt1->year - dt2->year;
    if(dt1->month != dt2->month) return dt1->month - dt2->month;
    if(dt1->day != dt2->day) return dt1->day - dt2->day;
    if(dt1->hour != dt2->hour) return dt1->hour - dt2->hour;
    if(dt1->minute != dt2->minute) return dt1->minute - dt2->minute;
    return dt1->second - dt2->second;
}

// Main Extraction Function

CalendarEventList* extract_calendar_events(
    const char* ics_file_path,
    const DateTime* date_after,
    const DateTime* date_before) {

    CalendarEventList* event_list = calendar_event_list_alloc(10);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, ics_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return event_list;
    }

    FuriString* line = furi_string_alloc();
    bool in_event = false;
    CalendarEvent current_event = {0};

    while(storage_file_read(file, (uint8_t*)furi_string_get_cstr(line), 1) == 1) {
        char c = furi_string_get_char(line, 0);
        if(c == '\n' || c == '\r') {
            const char* line_str = furi_string_get_cstr(line);

            if(strcmp(line_str, "BEGIN:VEVENT") == 0) {
                in_event = true;
                current_event.summary = furi_string_alloc();
                current_event.location = furi_string_alloc();
                current_event.description = furi_string_alloc();
                current_event.all_day = false;
            }
            else if(strcmp(line_str, "END:VEVENT") == 0 && in_event) {
                if(datetime_compare(&current_event.start_time, date_after) >= 0 &&
                   datetime_compare(&current_event.start_time, date_before) <= 0) {

                    if(event_list->count >= event_list->capacity) {
                        event_list->capacity *= 2;
                        event_list->events = realloc(
                            event_list->events,
                            sizeof(CalendarEvent) * event_list->capacity);
                    }
                    event_list->events[event_list->count++] = current_event;
                } else {
                    furi_string_free(current_event.summary);
                    furi_string_free(current_event.location);
                    furi_string_free(current_event.description);
                }
                in_event = false;
            }
            else if(in_event) {
                if(strncmp(line_str, "SUMMARY:", 8) == 0) {
                    furi_string_set_str(current_event.summary, line_str + 8);
                }
                else if(strncmp(line_str, "LOCATION:", 9) == 0) {
                    furi_string_set_str(current_event.location, line_str + 9);
                }
                else if(strncmp(line_str, "DESCRIPTION:", 12) == 0) {
                    furi_string_set_str(current_event.description, line_str + 12);
                }
                else if(strncmp(line_str, "DTSTART", 7) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        current_event.all_day =
                            parse_ics_datetime(value + 1, &current_event.start_time);
                    }
                }
                else if(strncmp(line_str, "DTEND", 5) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        parse_ics_datetime(value + 1, &current_event.end_time);
                    }
                }
            }

            furi_string_reset(line);
        } else {
            furi_string_push_back(line, c);
        }
    }

    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return event_list;
}
