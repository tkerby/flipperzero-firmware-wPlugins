#pragma once

#include "kia_generic.h"

#define KIA_PROTOCOL_V4_NAME "Kia V4"

typedef struct SubGhzProtocolDecoderKiaV4 SubGhzProtocolDecoderKiaV4;
typedef struct SubGhzProtocolEncoderKiaV4 SubGhzProtocolEncoderKiaV4;

extern const SubGhzProtocolDecoder kia_protocol_v4_decoder;
extern const SubGhzProtocolEncoder kia_protocol_v4_encoder;
extern const SubGhzProtocol kia_protocol_v4;

void* kia_protocol_decoder_v4_alloc(SubGhzEnvironment* environment);
void kia_protocol_decoder_v4_free(void* context);
void kia_protocol_decoder_v4_reset(void* context);
void kia_protocol_decoder_v4_feed(void* context, bool level, uint32_t duration);
uint8_t kia_protocol_decoder_v4_get_hash_data(void* context);
SubGhzProtocolStatus kia_protocol_decoder_v4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus kia_protocol_decoder_v4_deserialize(void* context, FlipperFormat* flipper_format);
void kia_protocol_decoder_v4_get_string(void* context, FuriString* output);