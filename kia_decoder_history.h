// kia_decoder_history.h
#pragma once

#include <lib/subghz/receiver.h>
#include <lib/subghz/protocols/base.h>

typedef struct KiaHistory KiaHistory;

KiaHistory* kia_history_alloc(void);
void kia_history_free(KiaHistory* instance);
void kia_history_reset(KiaHistory* instance);
uint16_t kia_history_get_item(KiaHistory* instance);
uint16_t kia_history_get_last_index(KiaHistory* instance);
bool kia_history_add_to_history(
    KiaHistory* instance,
    void* context,
    SubGhzRadioPreset* preset);
void kia_history_get_text_item_menu(KiaHistory* instance, FuriString* output, uint16_t idx);
void kia_history_get_text_item(KiaHistory* instance, FuriString* output, uint16_t idx);
SubGhzProtocolDecoderBase* kia_history_get_decoder_base(KiaHistory* instance, uint16_t idx);
FlipperFormat* kia_history_get_raw_data(KiaHistory* instance, uint16_t idx);