// kia_decoder_history.c
#include "kia_decoder_history.h"
#include <lib/subghz/receiver.h>
#include <flipper_format/flipper_format_i.h>

#define TAG "ProtoPirateHistory"
#define KIA_HISTORY_MAX 50

typedef struct {
    FuriString* item_str;
    FlipperFormat* flipper_format;
    uint8_t type;
    SubGhzRadioPreset* preset;
} KiaHistoryItem;

ARRAY_DEF(KiaHistoryItemArray, KiaHistoryItem, M_POD_OPLIST)

struct KiaHistory {
    KiaHistoryItemArray_t data;
    uint16_t last_index;
};

KiaHistory* kia_history_alloc(void) {
    KiaHistory* instance = malloc(sizeof(KiaHistory));
    KiaHistoryItemArray_init(instance->data);
    instance->last_index = 0;
    return instance;
}

void kia_history_free(KiaHistory* instance) {
    furi_assert(instance);
    for(size_t i = 0; i < KiaHistoryItemArray_size(instance->data); i++) {
        KiaHistoryItem* item = KiaHistoryItemArray_get(instance->data, i);
        furi_string_free(item->item_str);
        flipper_format_free(item->flipper_format);
        free(item->preset);
    }
    KiaHistoryItemArray_clear(instance->data);
    free(instance);
}

void kia_history_reset(KiaHistory* instance) {
    furi_assert(instance);
    for(size_t i = 0; i < KiaHistoryItemArray_size(instance->data); i++) {
        KiaHistoryItem* item = KiaHistoryItemArray_get(instance->data, i);
        furi_string_free(item->item_str);
        flipper_format_free(item->flipper_format);
        free(item->preset);
    }
    KiaHistoryItemArray_reset(instance->data);
    instance->last_index = 0;
}

uint16_t kia_history_get_item(KiaHistory* instance) {
    furi_assert(instance);
    return KiaHistoryItemArray_size(instance->data);
}

uint16_t kia_history_get_last_index(KiaHistory* instance) {
    furi_assert(instance);
    return instance->last_index;
}

bool kia_history_add_to_history(
    KiaHistory* instance,
    void* context,
    SubGhzRadioPreset* preset) {
    furi_assert(instance);
    furi_assert(context);
    
    if(KiaHistoryItemArray_size(instance->data) >= KIA_HISTORY_MAX) {
        return false;
    }

    SubGhzProtocolDecoderBase* decoder_base = context;
    
    // Create a new history item
    KiaHistoryItem* item = KiaHistoryItemArray_push_raw(instance->data);
    item->item_str = furi_string_alloc();
    item->flipper_format = flipper_format_string_alloc();
    item->type = 0;
    
    // Copy preset
    item->preset = malloc(sizeof(SubGhzRadioPreset));
    item->preset->frequency = preset->frequency;
    item->preset->name = furi_string_alloc();
    furi_string_set(item->preset->name, preset->name);
    item->preset->data = preset->data;
    item->preset->data_size = preset->data_size;

    // Get string representation
    FuriString* text = furi_string_alloc();
    subghz_protocol_decoder_base_get_string(decoder_base, text);
    furi_string_set(item->item_str, text);
    
    // Serialize to flipper format
    subghz_protocol_decoder_base_serialize(decoder_base, item->flipper_format, preset);
    
    furi_string_free(text);
    
    instance->last_index++;
    
    FURI_LOG_I(TAG, "Added item %u to history", instance->last_index);
    
    return true;
}

void kia_history_get_text_item_menu(KiaHistory* instance, FuriString* output, uint16_t idx) {
    furi_assert(instance);
    furi_assert(output);
    
    if(idx >= KiaHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }
    
    KiaHistoryItem* item = KiaHistoryItemArray_get(instance->data, idx);
    
    // Get just the first line for the menu
    const char* str = furi_string_get_cstr(item->item_str);
    const char* newline = strchr(str, '\r');
    if(newline) {
        size_t len = newline - str;
        furi_string_set_strn(output, str, len);
    } else {
        newline = strchr(str, '\n');
        if(newline) {
            size_t len = newline - str;
            furi_string_set_strn(output, str, len);
        } else {
            furi_string_set(output, item->item_str);
        }
    }
}

void kia_history_get_text_item(KiaHistory* instance, FuriString* output, uint16_t idx) {
    furi_assert(instance);
    furi_assert(output);
    
    if(idx >= KiaHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }
    
    KiaHistoryItem* item = KiaHistoryItemArray_get(instance->data, idx);
    furi_string_set(output, item->item_str);
}

SubGhzProtocolDecoderBase* kia_history_get_decoder_base(KiaHistory* instance, uint16_t idx) {
    UNUSED(instance);
    UNUSED(idx);
    // This would need the environment to recreate the decoder
    // For now, return NULL - use get_text_item instead
    return NULL;
}

FlipperFormat* kia_history_get_raw_data(KiaHistory* instance, uint16_t idx) {
    furi_assert(instance);
    
    if(idx >= KiaHistoryItemArray_size(instance->data)) {
        return NULL;
    }
    
    KiaHistoryItem* item = KiaHistoryItemArray_get(instance->data, idx);
    return item->flipper_format;
}