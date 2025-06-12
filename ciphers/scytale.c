#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

char* scytale_encrypt(const char* plaintext, int32_t key) {
    if (key <= 1) {
        return "key <= 1, encryption failed";
    }

    if (!plaintext) {
        return "text input does not exist, try again";
    }

    int len = strlen(plaintext);
    int cols = (int)ceil((double)len / key);

    char* ciphertext = malloc(len + 1);
    if (!ciphertext) return "memory allocation failed, try again";

    int index = 0;
    for (int col = 0; col < cols; col++) {
        for (int row = 0; row < key; row++) {
            int pos = row * cols + col;
            if (pos < len) {
                ciphertext[index++] = plaintext[pos];
            }
        }
    }

    ciphertext[index] = '\0';
    return ciphertext;
}

char* scytale_decrypt(const char* ciphertext, int32_t key) {
    if (key <= 1) {
      return "key <= 1, encryption failed";
    }

    if (!ciphertext) {
      return "text input does not exist, try again";
    }

    int len = strlen(ciphertext);
    int cols = (int)ceil((double)len / key);
    int full_cols = len % key;

    char* plaintext = malloc(len + 1);
    if (!plaintext) return "memory allocation failed, try again";

    int index = 0;
    for (int row = 0; row < key; row++) {
        for (int col = 0; col < cols; col++) {
            int pos = col * key + row;
            // Handle shorter final rows
            if (col == cols - 1 && row >= full_cols && full_cols != 0) continue;
            if (pos < len) {
                plaintext[index++] = ciphertext[pos];
            }
        }
    }

    plaintext[index] = '\0';
    return plaintext;
}