/* Host tests for composer_music.c: note mapping and Flipper Music Format output. */
#include "../composer_music.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Pitch index 6 = tonic; key 0 = C -> C5 (MIDI 72); index 0 = rest. */
    assert(cm_note_midi(6, 0) == 72);
    assert(cm_note_midi(6, 2) == 74);
    assert(cm_note_midi(1, 0) == 67);
    assert(cm_note_midi(25, 11) == 102);
    assert(cm_note_midi(0, 0) == -1);
    assert(fabsf(cm_midi_frequency(69) - 440.0f) < 0.01f);
    char name[8];
    cm_midi_name(73, name, sizeof(name));
    assert(strcmp(name, "C#5") == 0);

    const float pure[CM_STYLES] = {0, 1, 0, 0, 0};
    const float mix[CM_STYLES] = {0.5f, 0.5f, 0, 0, 0};
    assert(cm_mix_bpm(pure) == 96);
    assert(cm_mix_bpm(mix) == 123);

    /* 8th C5, dotted 8th rest, 16th D#5, whole C6 */
    const CmNote notes[] = {{6, 1}, {0, 2}, {9, 0}, {18, 7}};
    char out[256];
    size_t n = cm_write_fmf(notes, 4, 0, 120, out, sizeof(out));
    printf("%s", out);
    assert(n > 0);
    assert(strstr(out, "Filetype: Flipper Music Format\nVersion: 0\nBPM: 120\n"));
    assert(strstr(out, "Notes: 8C5, 8P., 16D#5, 1C6\n"));
    assert(cm_write_fmf(notes, 4, 0, 120, out, 40) == 0); /* too small */
    puts("MUSIC OK");
    return 0;
}
