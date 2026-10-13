#include "decoder.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static void fail(DecodeResult* r, const char* error) {
    r->success = false;
    strncpy(r->error, error, sizeof(r->error) - 1);
    r->error[sizeof(r->error) - 1] = 0;
}
static int hex_digit(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool hex_decode(const char* s, DecodeResult* r) {
    int high = -1;
    for(size_t i = 0; s[i]; ++i) {
        if(isspace((unsigned char)s[i])) continue;
        int value = hex_digit(s[i]);
        if(value < 0) {
            fail(r, "Invalid hex character");
            return false;
        }
        if(high < 0)
            high = value;
        else {
            if(r->length >= MM_OUTPUT_CAP - 1) {
                fail(r, "Output exceeds 512 bytes");
                return false;
            }
            r->bytes[r->length++] = (uint8_t)(high * 16 + value);
            high = -1;
        }
    }
    if(high >= 0 || !r->length) {
        fail(r, "Hex needs complete byte pairs");
        return false;
    }
    return true;
}
static int b64_digit(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}
static bool base64_decode(const char* s, DecodeResult* r) {
    size_t len = strlen(s);
    if(!len || len % 4) {
        fail(r, "Base64 needs groups of four");
        return false;
    }
    for(size_t i = 0; i < len; i += 4) {
        int a = b64_digit(s[i]), b = b64_digit(s[i + 1]);
        bool pc = s[i + 2] == '=', pd = s[i + 3] == '=';
        int c = pc ? 0 : b64_digit(s[i + 2]), d = pd ? 0 : b64_digit(s[i + 3]);
        if(a < 0 || b < 0 || c < 0 || d < 0 || (pc && !pd) || ((pc || pd) && i + 4 != len) ||
           (pc && (b & 15)) || (!pc && pd && (c & 3))) {
            fail(r, "Invalid Base64 / padding");
            return false;
        }
        uint32_t bits = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c << 6) | d;
        size_t count = pc ? 1 : (pd ? 2 : 3);
        if(r->length + count >= MM_OUTPUT_CAP) {
            fail(r, "Output exceeds 512 bytes");
            return false;
        }
        for(size_t j = 0; j < count; ++j)
            r->bytes[r->length++] = (uint8_t)(bits >> (16 - 8 * j));
    }
    return true;
}
static bool bf_decode(const char* code, DecodeResult* r) {
    size_t len = strlen(code);
    int16_t* jumps = malloc(len * sizeof(int16_t));
    uint16_t* stack = malloc(len * sizeof(uint16_t));
    uint8_t tape[MM_TAPE_SIZE] = {0};
    if(!jumps || !stack) {
        free(jumps);
        free(stack);
        fail(r, "Not enough memory");
        return false;
    }
    size_t depth = 0, ptr = 0, pc = 0;
    uint32_t steps = 0;
    bool ok = true, instruction = false;
    for(size_t i = 0; i < len; ++i) {
        if(strchr("><+-.,[]", code[i])) instruction = true;
        if(code[i] == '[')
            stack[depth++] = i;
        else if(code[i] == ']') {
            if(!depth) {
                fail(r, "Unmatched closing bracket");
                ok = false;
                break;
            }
            size_t open = stack[--depth];
            jumps[open] = i;
            jumps[i] = open;
        }
    }
    if(ok && depth) {
        fail(r, "Unmatched opening bracket");
        ok = false;
    }
    if(ok && !instruction) {
        fail(r, "No Brainfuck instructions");
        ok = false;
    }
    while(ok && pc < len) {
        if(++steps > MM_STEP_LIMIT) {
            fail(r, "Stopped: 100000 step limit");
            ok = false;
            break;
        }
        char cmd = code[pc];
        if(cmd == '>') {
            if(ptr + 1 >= MM_TAPE_SIZE) {
                fail(r, "Stopped: tape boundary");
                ok = false;
            } else
                ptr++;
        } else if(cmd == '<') {
            if(!ptr) {
                fail(r, "Stopped: tape boundary");
                ok = false;
            } else
                ptr--;
        } else if(cmd == '+')
            tape[ptr]++;
        else if(cmd == '-')
            tape[ptr]--;
        else if(cmd == '.') {
            if(r->length >= MM_OUTPUT_CAP - 1) {
                fail(r, "Stopped: output limit");
                ok = false;
            } else
                r->bytes[r->length++] = tape[ptr];
        } else if(cmd == ',') { /* Preserve baseline: input instruction is a no-op. */
        } else if(cmd == '[' && tape[ptr] == 0)
            pc = jumps[pc];
        else if(cmd == ']' && tape[ptr] != 0)
            pc = jumps[pc];
        pc++;
    }
    free(jumps);
    free(stack);
    return ok;
}
static bool looks_bf(const char* s) {
    size_t n = 0;
    bool work = false;
    for(size_t i = 0; s[i]; ++i) {
        if(isspace((unsigned char)s[i])) continue;
        if(!strchr("><+-.,[]", s[i])) return false;
        n++;
        if(strchr("><+-[]", s[i])) work = true;
    }
    return n && work;
}
static bool as_text(const DecodeResult* r) {
    for(size_t i = 0; i < r->length; ++i)
        if(!r->bytes[i] || (r->bytes[i] < 32 && !isspace(r->bytes[i])) || r->bytes[i] > 126)
            return false;
    return true;
}
void mm_decode(DecodeMode mode, const char* input, DecodeResult* r) {
    memset(r, 0, sizeof(*r));
    size_t len = strlen(input);
    if(!len || len >= MM_INPUT_CAP) {
        fail(r, "Input must be 1-1024 bytes");
        return;
    }
    if(mode == DecodeHex)
        r->success = hex_decode(input, r);
    else if(mode == DecodeBase64)
        r->success = base64_decode(input, r);
    else if(mode == DecodeBrainfuck)
        r->success = bf_decode(input, r);
    else if(mode == DecodeROT13) {
        if(len >= MM_OUTPUT_CAP) {
            fail(r, "Output exceeds 512 bytes");
            return;
        }
        for(size_t i = 0; i < len; ++i) {
            char c = input[i];
            if(c >= 'a' && c <= 'z')
                c = 'a' + (c - 'a' + 13) % 26;
            else if(c >= 'A' && c <= 'Z')
                c = 'A' + (c - 'A' + 13) % 26;
            r->bytes[r->length++] = c;
        }
        r->success = true;
    } else {
        /* Preserve the baseline order: Hex -> Base64 -> Brainfuck.
         * Each stage is attempted once. Conservative BF auto detection avoids
         * treating ordinary prose punctuation as executable instructions.
         */
        char* current = malloc(MM_INPUT_CAP);
        DecodeResult* stage = malloc(sizeof(DecodeResult));
        if(!current || !stage) {
            free(current);
            free(stage);
            fail(r, "Not enough memory");
            return;
        }
        strcpy(current, input);
        uint8_t layers = 0;
        memset(stage, 0, sizeof(*stage));
        if(hex_decode(current, stage)) {
            *r = *stage;
            layers++;
            if(!as_text(stage)) goto auto_done;
            memcpy(current, stage->bytes, stage->length);
            current[stage->length] = 0;
        }
        memset(stage, 0, sizeof(*stage));
        if(base64_decode(current, stage)) {
            *r = *stage;
            layers++;
            if(!as_text(stage)) goto auto_done;
            memcpy(current, stage->bytes, stage->length);
            current[stage->length] = 0;
        }
        if(looks_bf(current)) {
            memset(stage, 0, sizeof(*stage));
            bool ok = bf_decode(current, stage);
            *r = *stage;
            if(!ok) {
                free(current);
                free(stage);
                return;
            }
            layers++;
        }
    auto_done:
        r->success = layers > 0;
        r->layers = layers;
        if(!layers)
            fail(r, "No supported encoding found");
        else
            r->error[0] = 0;
        free(current);
        free(stage);
    }
    if(r->success && !r->layers) r->layers = 1;
    r->bytes[r->length] = 0;
}
uint32_t mm_meal_hash(DecodeMode mode, const char* s) {
    uint32_t hash = 2166136261U ^ (uint32_t)mode;
    while(*s) {
        hash ^= (uint8_t)*s++;
        hash *= 16777619U;
    }
    return hash ? hash : 1;
}
