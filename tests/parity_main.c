/* Host harness: prints every generated note and its logits so that
 * tests/parity.py can compare composer_model.c against training/reference.py. */
#include "../composer_model.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if(argc < 4 + CM_STYLES) {
        fprintf(stderr, "usage: parity seed temperature w0..w%d melodies\n", CM_STYLES - 1);
        return 2;
    }
    uint32_t seed = (uint32_t)strtoul(argv[1], NULL, 10);
    float temperature = strtof(argv[2], NULL);
    float weights[CM_STYLES];
    for(int i = 0; i < CM_STYLES; ++i)
        weights[i] = strtof(argv[3 + i], NULL);
    int melodies = atoi(argv[3 + CM_STYLES]);
    for(int m = 0; m < melodies; ++m) {
        ComposerState state;
        composer_reset(&state, weights);
        uint8_t p, d;
        while(composer_next(&state, temperature, composer_xorshift01, &seed, &p, &d)) {
            printf("%d %d", p, d);
            for(int i = 0; i < CM_PITCHES; ++i)
                printf(" %.6g", composer_last_pitch_logits[i]);
            for(int i = 0; i < CM_DURATIONS; ++i)
                printf(" %.6g", composer_last_dur_logits[i]);
            printf("\n");
        }
        printf("END\n");
    }
    return 0;
}
