#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "porta.h"

static const char* porta_rows[13] = {
    "NOPQRSTUVWXYZABCDEFGHIJKLM", // A/B
    "OPQRSTUVWXYZABCDEFGHIJKLMN", // C/D
    "PQRSTUVWXYZABCDEFGHIJKLMNO", // E/F
    "QRSTUVWXYZABCDEFGHIJKLMNOP", // G/H
    "RSTUVWXYZABCDEFGHIJKLMNOPQ", // I/J
    "STUVWXYZABCDEFGHIJKLMNOPQR", // K/L
    "TUVWXYZABCDEFGHIJKLMNOPQRS", // M/N
    "UVWXYZABCDEFGHIJKLMNOPQRST", // O/P
    "VWXYZABCDEFGHIJKLMNOPQRSTU", // Q/R
    "WXYZABCDEFGHIJKLMNOPQRSTUV", // S/T
    "XYZABCDEFGHIJKLMNOPQRSTUVW", // U/V
    "YZABCDEFGHIJKLMNOPQRSTUVWX", // W/X
    "ZABCDEFGHIJKLMNOPQRSTUVWXY"  // Y/Z
};

static int number_to_index(char letter) {
    if (letter >= 'a' && letter <= 'z') {
        return letter - 'a';
    } else if (letter >= 'A' && letter <= 'Z') {
        return letter - 'A';
    } else {
        return -1; // not a letter
    }
}

char* porta_encrypt(const char* plaintext, const char* keyword) {
    size_t plaintext_length = strlen(plaintext);
    size_t keyword_length = strlen(keyword);

    char* output = malloc(plaintext_length + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < plaintext_length; i++) {
        char p = plaintext[i];
        int p_index = number_to_index(p);

        if (p_index >= 0) {
            char k = keyword[i % keyword_length];
            int k_index = number_to_index(k);

            // Determine row number
            int table = k_index / 2;
            // Porta cipher: letters A–M get substituted from row, N–Z get substituted from first half
            if (p_index < 13) {
                output[i] = porta_rows[table][p_index];
            } else {
                output[i] = porta_rows[table][p_index - 13];
            }

            // Preserve case
            if (islower(p)) {
                output[i] = tolower(output[i]);
            }
        } else {
            // Non-letter, copy directly
            output[i] = p;
        }
    }

    output[plaintext_length] = '\0';
    return output;
}