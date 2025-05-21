#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "xor.h"

uint8_t* xor_encrypt_and_decrypt(const char* input, const char* key, size_t* out_len) {
    size_t input_len = strlen(input);
    size_t key_len = strlen(key);
    uint8_t* output = malloc(input_len);
    if (!output) return NULL;

    for (size_t i = 0; i < input_len; ++i) {
        output[i] = input[i] ^ key[i % key_len];
    }

    *out_len = input_len;
    return output;
}

char* bytes_to_hex(const uint8_t* data, size_t len, char* hex_buf, size_t hex_buf_size) {
    const char hex_chars[] = "0123456789ABCDEF";
    size_t j = 0;

    for (size_t i = 0; i < len && j + 2 < hex_buf_size; ++i) {
        hex_buf[j++] = hex_chars[(data[i] >> 4) & 0x0F];
        hex_buf[j++] = hex_chars[data[i] & 0x0F];
    }

    hex_buf[j] = '\0'; // Null-terminate
    return hex_buf;
}
