#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TjaCourseEasy,
    TjaCourseNormal,
    TjaCourseHard,
    TjaCourseOni,
    TjaCourseEdit,
    TjaCourseTower,
    TjaCourseDan,
    TjaCourseCount,
} TjaCourse;

typedef struct {
    int32_t time_ms; /* relative to the beginning of the audio */
    uint8_t kind;
} TjaNote;

typedef struct {
    int32_t time_ms;
    float bpm;
} TjaBpmChange;

typedef struct {
    char title[128];
    char wave[256];
    char course[24];
    TjaCourse course_id;
    TjaNote* notes;
    size_t note_count;
    float initial_bpm;
    TjaBpmChange* bpm_changes;
    size_t bpm_change_count;
    int32_t end_ms;
} TjaChart;

/* Return 1 for a line (including an empty one), 0 for EOF, -1 for an error.
 * The reader must reject lines that do not fit, and NUL-terminate each line. */
typedef int (*TjaReadLine)(void* context, char* line, size_t capacity);

/* Reads only the first #START ... #END chart. On failure, chart is still safe to free. */
bool tja_parse(TjaReadLine reader, void* context, TjaChart* chart, const char** error);
bool tja_parse_course(
    TjaReadLine reader,
    void* context,
    TjaCourse course,
    TjaChart* chart,
    const char** error);
/* Scan available COURSE sections without loading note arrays. Rewind/reopen the
 * reader before parsing. Numeric COURSE names and Ura (Edit) are accepted. */
bool tja_list_courses(TjaReadLine reader, void* context, uint8_t* courses, const char** error);
const char* tja_course_name(TjaCourse course);
TjaCourse tja_default_course(uint8_t courses);
/* Original chart BPM at an audio position, including changes before time zero. */
float tja_chart_bpm_at(const TjaChart* chart, uint32_t position_ms);
void tja_chart_free(TjaChart* chart);
