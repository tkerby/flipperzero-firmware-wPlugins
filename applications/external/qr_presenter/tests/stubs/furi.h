#pragma once
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    char text[128];
} FuriString;
FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* value);
const char* furi_string_get_cstr(FuriString* value);
size_t test_strlcpy(char* dest, const char* source, size_t size);
#define strlcpy test_strlcpy
