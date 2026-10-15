#pragma once

#include <furi.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KeyCategoryNone = 0,
    KeyCategoryLFRFID,
    KeyCategoryNFC,
    KeyCategoryIButton,
} KeyCategory;

typedef struct {
    KeyCategory category;
    char protocol_name[32];
    uint8_t data[64];
    size_t data_len;
    char data_hex[128];
    char atqa[8];
    char sak[8];
    bool valid;
    char saved_path[128];
} DetectedKey;

const char* key_category_to_string(KeyCategory cat);
const char* key_category_to_asset_dir(KeyCategory cat);
const char* key_category_to_extension(KeyCategory cat);
