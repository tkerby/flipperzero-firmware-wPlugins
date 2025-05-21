#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xor.h"

char* xor_encrypt_and_decrypt(const char* text, const char* key) {
    int text_len = strlen(text);
    int key_len = strlen(key);

    char* result = malloc(text_len + 1);  // +1 for null terminator
    if (!result) {
        return NULL;  // Allocation failed
    }

    for (int i = 0; i < text_len; i++) {
        result[i] = text[i] ^ key[i % key_len];
    }

    result[text_len] = '\0';  // Null-terminate the result
    return result;
}
