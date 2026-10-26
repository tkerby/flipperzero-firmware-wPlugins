#include "../decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void text(DecodeMode mode, const char* input, const char* expected, unsigned layers) {
    DecodeResult r;
    mm_decode(mode, input, &r);
    assert(r.success && r.length == strlen(expected));
    assert(memcmp(r.bytes, expected, r.length) == 0 && r.layers == layers);
}
static void rejects(DecodeMode mode, const char* input) {
    DecodeResult r;
    mm_decode(mode, input, &r);
    assert(!r.success && r.error[0]);
}
int main(void) {
    text(DecodeHex, "48656c6c6f", "Hello", 1);
    text(DecodeBase64, "SGVsbG8=", "Hello", 1);
    text(DecodeROT13, "Uryyb", "Hello", 1);
    text(DecodeBrainfuck, "++++++++[>++++++++<-]>+.", "A", 1);
    text(DecodeAuto, "534756736247383d", "Hello", 2);
    text(
        DecodeAuto,
        "KysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysu",
        "A",
        2);
    text(DecodeAuto, "++++++++[>++++++++<-]>+.", "A", 1);
    rejects(DecodeHex, "1");
    rejects(DecodeHex, "GG");
    rejects(DecodeBase64, "AB==");
    rejects(DecodeBase64, "SGVsbG8=AAAA");
    rejects(DecodeBase64, "A===");
    rejects(DecodeAuto, "Hello, world.");
    rejects(DecodeBrainfuck, "[");
    rejects(DecodeBrainfuck, "]");
    rejects(DecodeBrainfuck, "<");
    rejects(DecodeBrainfuck, "+[]");
    rejects(DecodeAuto, "");
    DecodeResult r;
    mm_decode(DecodeBase64, "AAEC/w==", &r);
    assert(r.success && r.length == 4 && r.bytes[0] == 0 && r.bytes[3] == 255);
    char large[MM_INPUT_CAP + 10];
    memset(large, '>', sizeof(large));
    large[256] = 0;
    rejects(DecodeBrainfuck, large);
    memset(large, '.', sizeof(large));
    large[513] = 0;
    rejects(DecodeBrainfuck, large);
    memset(large, 'A', sizeof(large));
    large[1024] = 0;
    rejects(DecodeBase64, large);
    large[1024] = 'A';
    large[1025] = 0;
    rejects(DecodeHex, large);
    /* Bounded random stress: each input is NUL-terminated within the cap. */
    srand(17);
    for(unsigned n = 0; n < 15000; n++) {
        size_t len = rand() % 200;
        for(size_t i = 0; i < len; i++)
            large[i] = "ABCDEF0123456789+/=><-.,[] \n"[rand() % 27];
        large[len] = 0;
        mm_decode(n % DecodeCount, large, &r);
        assert(r.length < MM_OUTPUT_CAP);
    }
    puts("Decoder vectors and 15000 bounded stress cases passed.");
    return 0;
}
