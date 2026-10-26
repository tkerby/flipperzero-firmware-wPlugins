// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "tja_parser.h"
#include "wav_reader.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(void* context, char* line, size_t capacity) {
    FILE* file = context;
    if(!fgets(line, (int)capacity, file)) return ferror(file) ? -1 : 0;
    if(!strchr(line, '\n') && !feof(file)) return -1;
    return 1;
}

static bool parse(const char* text, TjaChart* chart) {
    FILE* file = tmpfile();
    assert(file);
    fputs(text, file);
    rewind(file);
    const char* error;
    bool result = tja_parse(read_line, file, chart, &error);
    if(result) assert(error == NULL);
    fclose(file);
    return result;
}

static void test_timing(void) {
    TjaChart chart;
    assert(parse(
        "\xef\xbb\xbfTITLE:Test\r\nBPM:120\nWAVE:song.ogg\nOFFSET:-1\n"
        "COURSE:Oni\n#START\n12\n34, // multi-line measure\n"
        "#MEASURE 1/4\n#BPMCHANGE 60\n1,\n#DELAY 0.25\n2,\n#END\n"
        "COURSE:Hard\n#START\n9999,\n#END\n",
        &chart));
    assert(strcmp(chart.title, "Test") == 0);
    assert(strcmp(chart.wave, "song.ogg") == 0);
    assert(strcmp(chart.course, "Oni") == 0);
    assert(chart.note_count == 6);
    const int expected[] = {1000, 1500, 2000, 2500, 3000, 4250};
    for(size_t i = 0; i < chart.note_count; i++)
        assert(chart.notes[i].time_ms == expected[i]);
    assert(chart.end_ms == 5250);
    assert(chart.initial_bpm == 120);
    assert(chart.bpm_change_count == 1);
    assert(tja_chart_bpm_at(&chart, 2999) == 120);
    assert(tja_chart_bpm_at(&chart, 3000) == 60);
    assert(tja_chart_bpm_at(&chart, 5250) == 60);
    assert(tja_chart_bpm_at(&chart, 0) == 120); /* Seeking backward restores BPM. */
    tja_chart_free(&chart);

    assert(parse("BPM:120\nWAVE:a.wav\n#START\n10\n#BPMCHANGE 240\n20,\n,\n18,\n#END\n", &chart));
    assert(chart.note_count == 3);
    assert(chart.notes[0].time_ms == 0);
    assert(chart.notes[1].time_ms == 1000);
    assert(chart.notes[2].time_ms == 2500);
    assert(tja_chart_bpm_at(&chart, 999) == 120);
    assert(tja_chart_bpm_at(&chart, 1000) == 240);
    tja_chart_free(&chart);

    assert(parse("BPM:120\nWAVE:a.wav\nOFFSET:0.5\n#START\n1,\n#END\n", &chart));
    assert(chart.notes[0].time_ms == -500);
    tja_chart_free(&chart);

    /* Tempo changes also matter in silent measures and before the audio starts.
     * Multiple changes at one timestamp use the last command in file order. */
    assert(parse(
        "BPM:123.5\nWAVE:a.wav\nOFFSET:0.5\n#START\n"
        "#BPMCHANGE 90\n#DELAY 0.25\n#BPMCHANGE 95\n#BPMCHANGE 96\n00,\n"
        "#BPMCHANGE 150\n,\n#END\n",
        &chart));
    assert(chart.initial_bpm == 123.5f && chart.note_count == 0);
    assert(chart.bpm_change_count == 4);
    assert(tja_chart_bpm_at(&chart, 0) == 96);
    assert(tja_chart_bpm_at(&chart, 2249) == 96);
    assert(tja_chart_bpm_at(&chart, 2250) == 150);
    tja_chart_free(&chart);
}

static void test_gogo(void) {
    TjaChart chart;
    assert(parse(
        "BPM:120\nWAVE:a.wav\nOFFSET:-1\n#START\n10\n#GOGOSTART\n"
        "#BPMCHANGE 240\n#DELAY 0.125\n20,\n#GOGOEND\n#END\n",
        &chart));
    assert(chart.gogo_change_count == 2 && chart.end_ms == 2625);
    assert(chart.gogo_changes[0].time_ms == 2000 && chart.gogo_changes[0].active);
    assert(chart.gogo_changes[1].time_ms == 2625 && !chart.gogo_changes[1].active);
    assert(chart.note_count == 2 && chart.notes[1].time_ms == 2125);
    assert(!tja_chart_gogo_at(&chart, 1999));
    assert(tja_chart_gogo_at(&chart, 2000));
    assert(tja_chart_gogo_at(&chart, 2124));
    assert(tja_chart_gogo_at(&chart, 2624));
    assert(!tja_chart_gogo_at(&chart, 2625));
    /* Seeking in either direction restores the state without replaying commands. */
    assert(tja_chart_gogo_at(&chart, 2200));
    assert(!tja_chart_gogo_at(&chart, 0));
    tja_chart_free(&chart);

    assert(parse(
        "BPM:120\nWAVE:a.wav\nOFFSET:0.25\n#START\n#GOGOSTART\n#GOGOSTART\n"
        "#DELAY 0.5\n#GOGOEND\n#GOGOSTART\n,\n#GOGOEND\n#DELAY 0.25\n#END\n",
        &chart));
    assert(chart.gogo_change_count == 4 && chart.end_ms == 2500);
    assert(chart.gogo_changes[0].time_ms == -250);
    assert(chart.gogo_changes[1].time_ms == 250 && chart.gogo_changes[2].time_ms == 250);
    assert(tja_chart_gogo_at(&chart, 0));
    assert(tja_chart_gogo_at(&chart, 250)); /* Last command at the same time wins. */
    assert(tja_chart_gogo_at(&chart, 2249));
    assert(!tja_chart_gogo_at(&chart, 2250));
    tja_chart_free(&chart);

    assert(parse("WAVE:a.wav\n#START\n#GOGOSTART\n1,\n#END\n", &chart));
    assert(tja_chart_gogo_at(&chart, 0) && tja_chart_gogo_at(&chart, 1999));
    assert(!tja_chart_gogo_at(&chart, 2000)); /* A missing GOGOEND stops at #END. */
    tja_chart_free(&chart);
    assert(parse("WAVE:a.wav\n#START\n#GOGOEND\n1,\n#END\n", &chart));
    assert(chart.gogo_change_count == 0 && !tja_chart_gogo_at(&chart, 0));
    tja_chart_free(&chart);
}

static void test_invalid_tja(void) {
    const char* invalid[] = {
        "BPM:0\nWAVE:a.wav\n#START\n1,\n#END\n",
        "BPM:nan\nWAVE:a.wav\n#START\n1,\n#END\n",
        "#START\n1,\n#END\n",
        "WAVE:a.wav\n#START\n1\n#END\n",
        "WAVE:a.wav\n#START\n1,\n",
        "WAVE:a.wav\n#START\n#MEASURE 4/0\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#BRANCHSTART p,50,80\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#DELAY -1\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#SCROLL 0\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#SCROLL nan\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#SCROLL inf\n1,\n#END\n",
        "WAVE:a.wav\n#START\n#SCROLL 1+2i\n1,\n#END\n",
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        TjaChart chart;
        assert(!parse(invalid[i], &chart));
        tja_chart_free(&chart);
    }
}

static void test_scroll(void) {
    TjaChart chart;
    assert(parse(
        "BPM:120\nWAVE:a.wav\nOFFSET:-1\n#START\n1\n#SCROLL 2\n2\n"
        "#BPMCHANGE 60\n3\n#SCROLL -0.5\n4,\n1,\n#END\n",
        &chart));
    assert(chart.note_count == 5);
    const int times[] = {1000, 1500, 2000, 3000, 4000};
    const float speeds[] = {0.5f, 1.0f, 0.5f, -0.125f, -0.125f};
    for(size_t i = 0; i < chart.note_count; i++) {
        assert(chart.notes[i].time_ms == times[i]);
        assert(fabsf(chart.notes[i].scroll_speed - speeds[i]) < 0.00001f);
    }
    assert(chart.end_ms == 8000);
    assert(fabsf(chart.min_note_speed - 0.125f) < 0.00001f);
    tja_chart_free(&chart);

    /* A slow intro must not amplify the main section's visual speed.
     * The same effective BPM has the same pixel speed across song headers. */
    assert(parse("BPM:90\nWAVE:a.wav\n#START\n,\n#BPMCHANGE 220\n1,\n#END\n", &chart));
    float slow_intro_speed = chart.notes[0].scroll_speed;
    assert(fabsf(slow_intro_speed - 220.0f / 240) < 0.00001f);
    tja_chart_free(&chart);
    assert(parse("BPM:180\nWAVE:a.wav\n#START\n,\n#BPMCHANGE 220\n1,\n#END\n", &chart));
    assert(chart.notes[0].scroll_speed == slow_intro_speed);
    tja_chart_free(&chart);

    assert(parse("BPM:90\nWAVE:a.wav\n#START\n#SCROLL 2\n1,\n#END\n", &chart));
    assert(chart.notes[0].scroll_speed == 0.75f);
    tja_chart_free(&chart);
}

static void test_course_selection(void) {
    const char* text =
        "TITLE:Multi\nBPM:120\nWAVE:song.wav\nOFFSET:-1\n"
        "COURSE:easy\n#START\n#GOGOSTART\n#BPMCHANGE 240\n#SCROLL 4\n#BRANCHSTART p,10,20\n1,\n#END\n"
        "COURSE:2\n#START\n2,\n#END\n"
        "COURSE:ONI\n#START\n13,\n#END\n"
        "COURSE:Ura\n#START\n4,\n#END\n";
    FILE* file = tmpfile();
    assert(file);
    fputs(text, file);
    rewind(file);
    uint8_t courses;
    const char* error;
    assert(tja_list_courses(read_line, file, &courses, &error));
    assert(!error);
    assert(
        courses == ((1U << TjaCourseEasy) | (1U << TjaCourseHard) | (1U << TjaCourseOni) |
                    (1U << TjaCourseEdit)));
    assert(tja_default_course(courses) == TjaCourseOni);
    assert(tja_default_course(courses & ~(1U << TjaCourseOni)) == TjaCourseEdit);
    assert(tja_default_course(1U << TjaCourseNormal) == TjaCourseNormal);
    TjaChart chart;
    rewind(file);
    assert(tja_parse_course(read_line, file, TjaCourseOni, &chart, &error));
    assert(chart.course_id == TjaCourseOni && strcmp(chart.course, "Oni") == 0);
    assert(chart.note_count == 2 && chart.notes[0].kind == 1 && chart.notes[1].kind == 3);
    /* Timing commands in the skipped Easy body must not leak into Oni. */
    assert(chart.notes[0].time_ms == 1000 && chart.notes[1].time_ms == 2000);
    assert(chart.end_ms == 3000);
    assert(chart.initial_bpm == 120 && chart.bpm_change_count == 0);
    assert(chart.gogo_change_count == 0 && !tja_chart_gogo_at(&chart, 1000));
    assert(fabsf(chart.notes[0].scroll_speed - 0.5f) < 0.00001f);
    assert(fabsf(chart.min_note_speed - 0.5f) < 0.00001f);
    tja_chart_free(&chart);
    rewind(file);
    assert(tja_parse_course(read_line, file, TjaCourseHard, &chart, &error));
    assert(chart.course_id == TjaCourseHard && chart.notes[0].kind == 2);
    tja_chart_free(&chart);
    rewind(file);
    assert(tja_parse_course(read_line, file, TjaCourseEdit, &chart, &error));
    assert(chart.notes[0].kind == 4);
    tja_chart_free(&chart);
    rewind(file);
    assert(!tja_parse_course(read_line, file, TjaCourseNormal, &chart, &error));
    tja_chart_free(&chart);
    rewind(file);
    assert(!tja_parse_course(read_line, file, TjaCourseEasy, &chart, &error));
    tja_chart_free(&chart);
    fclose(file);

    const char* invalid[] = {
        "COURSE:Hard\n#START\n1,\n", "COURSE:unknown\n#START\n1,\n#END\n", "TITLE:Empty\n"};
    for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        file = tmpfile();
        assert(file);
        fputs(invalid[i], file);
        rewind(file);
        assert(!tja_list_courses(read_line, file, &courses, &error));
        assert(error);
        fclose(file);
    }
}

typedef struct {
    const unsigned char* bytes;
    size_t size;
} MemoryFile;

static bool read_at(void* context, uint32_t offset, void* data, size_t size) {
    MemoryFile* file = context;
    if(offset > file->size || size > file->size - offset) return false;
    memcpy(data, file->bytes + offset, size);
    return true;
}

static void test_wav(void) {
    /* Odd-sized JUNK chunk, PCM format, and 4 samples. */
    unsigned char bytes[] = {
        'R', 'I', 'F', 'F', 50,   0,    0, 0,   'W',  'A',  'V', 'E', 'J', 'U', 'N',
        'K', 1,   0,   0,   0,    42,   0, 'f', 'm',  't',  ' ', 16,  0,   0,   0,
        1,   0,   1,   0,   0x22, 0x56, 0, 0,   0x22, 0x56, 0,   0,   1,   0,   8,
        0,   'd', 'a', 't', 'a',  4,    0, 0,   0,    128,  100, 150, 128,
    };
    MemoryFile file = {bytes, sizeof(bytes)};
    WavInfo info;
    assert(wav_read_header(read_at, &file, (uint32_t)file.size, &info));
    assert(info.sample_rate == 22050 && info.sample_count == 4 && info.data_offset == 54);
    assert(!wav_read_header(read_at, &file, (uint32_t)file.size - 1, &info));
    bytes[44] = 16;
    assert(!wav_read_header(read_at, &file, (uint32_t)file.size, &info));
    bytes[44] = 8;
    bytes[32] = 2;
    assert(!wav_read_header(read_at, &file, (uint32_t)file.size, &info));
}

static void test_song(const char* path) {
    FILE* file = fopen(path, "rb");
    assert(file);
    TjaChart chart;
    const char* error;
    if(!tja_parse(read_line, file, &chart, &error)) {
        fprintf(stderr, "%s: %s\n", path, error);
        abort();
    }
    assert(error == NULL);
    assert(chart.note_count > 100);
    for(size_t i = 1; i < chart.note_count; i++)
        assert(chart.notes[i].time_ms >= chart.notes[i - 1].time_ms);
    printf(
        "%s: %zu notes, first %dms, end %dms\n",
        path,
        chart.note_count,
        chart.notes[0].time_ms,
        chart.end_ms);
    tja_chart_free(&chart);
    rewind(file);
    uint8_t courses;
    assert(tja_list_courses(read_line, file, &courses, &error));
    for(int i = 0; i < TjaCourseCount; i++) {
        if(!(courses & (1U << i))) continue;
        rewind(file);
        if(!tja_parse_course(read_line, file, i, &chart, &error)) {
            fprintf(stderr, "%s (%s): %s\n", path, tja_course_name(i), error);
            abort();
        }
        assert(chart.course_id == (TjaCourse)i && chart.note_count);
        for(size_t n = 1; n < chart.note_count; n++)
            assert(chart.notes[n].time_ms >= chart.notes[n - 1].time_ms);
        tja_chart_free(&chart);
    }
    fclose(file);
}

int main(int argc, char** argv) {
    test_timing();
    test_gogo();
    test_invalid_tja();
    test_scroll();
    test_course_selection();
    test_wav();
    for(int i = 1; i < argc; i++)
        test_song(argv[i]);
    puts("Core tests passed");
    return 0;
}
