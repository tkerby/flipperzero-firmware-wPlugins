#include "tja_parser.h"
#include "wav_reader.h"

#include <assert.h>
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
    tja_chart_free(&chart);

    assert(parse("BPM:120\nWAVE:a.wav\n#START\n10\n#BPMCHANGE 240\n20,\n,\n18,\n#END\n", &chart));
    assert(chart.note_count == 3);
    assert(chart.notes[0].time_ms == 0);
    assert(chart.notes[1].time_ms == 1000);
    assert(chart.notes[2].time_ms == 2500);
    tja_chart_free(&chart);

    assert(parse("BPM:120\nWAVE:a.wav\nOFFSET:0.5\n#START\n1,\n#END\n", &chart));
    assert(chart.notes[0].time_ms == -500);
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
    };
    for(size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        TjaChart chart;
        assert(!parse(invalid[i], &chart));
        tja_chart_free(&chart);
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
    fclose(file);
}

int main(int argc, char** argv) {
    test_timing();
    test_invalid_tja();
    test_wav();
    for(int i = 1; i < argc; i++)
        test_song(argv[i]);
    puts("Core tests passed");
    return 0;
}
