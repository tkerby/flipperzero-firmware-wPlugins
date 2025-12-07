#include "kia_v4.h"

#define TAG "KiaV4"

static const uint64_t kia_v4_mf_key = 0xA8F5DFFC8DAA5CDB;

static const SubGhzBlockConst kia_protocol_v4_const = {
    .te_short = 400,
    .te_long = 800,
    .te_delta = 150,
    .min_count_bit_for_found = 64,
};

struct SubGhzProtocolDecoderKiaV4 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
    uint8_t short_pulse_count;
    uint32_t encrypted;
    uint32_t decrypted;
};

struct SubGhzProtocolEncoderKiaV4 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    KiaV4DecoderStepReset = 0,
    KiaV4DecoderStepCheckPreamble,
    KiaV4DecoderStepSaveDuration,
    KiaV4DecoderStepCheckDuration,
} KiaV4DecoderStep;

const SubGhzProtocolDecoder kia_protocol_v4_decoder = {
    .alloc = kia_protocol_decoder_v4_alloc,
    .free = kia_protocol_decoder_v4_free,
    .feed = kia_protocol_decoder_v4_feed,
    .reset = kia_protocol_decoder_v4_reset,
    .get_hash_data = kia_protocol_decoder_v4_get_hash_data,
    .serialize = kia_protocol_decoder_v4_serialize,
    .deserialize = kia_protocol_decoder_v4_deserialize,
    .get_string = kia_protocol_decoder_v4_get_string,
};

const SubGhzProtocolEncoder kia_protocol_v4_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol kia_protocol_v4 = {
    .name = KIA_PROTOCOL_V4_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable,
    .decoder = &kia_protocol_v4_decoder,
    .encoder = &kia_protocol_v4_encoder,
};

static const uint32_t keeloq_nlf = 0x3A5C742E;

static uint32_t keeloq_decrypt(uint32_t data, uint64_t key) {
    for(int i = 0; i < 528; i++) {
        uint8_t nlf_idx = ((data >> 1) & 1) | ((data >> 8) & 2) | ((data >> 18) & 4) | 
                          ((data >> 21) & 8) | ((data >> 27) & 16);
        uint8_t nlf_bit = (keeloq_nlf >> nlf_idx) & 1;
        
        uint8_t key_bit = (key >> (15 - (i % 64))) & 1;
        uint8_t msb = (data >> 31) & 1;
        uint8_t bit0 = (data >> 0) & 1;
        uint8_t bit16 = (data >> 16) & 1;
        
        uint8_t new_bit = nlf_bit ^ msb ^ bit0 ^ bit16 ^ key_bit;
        
        data = (data >> 1) | (new_bit << 31);
    }
    return data;
}

static uint8_t kia_v4_reverse8(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
}

static void kia_v4_extract_fields(SubGhzProtocolDecoderKiaV4* instance) {
    uint64_t inverted = ~instance->decoder.decode_data;
    
    instance->generic.data = inverted;
    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
    
    uint8_t b[8];
    for(int i = 0; i < 8; i++) {
        b[i] = (instance->generic.data >> (56 - i * 8)) & 0xFF;
    }
    
    instance->generic.serial = 
        ((uint32_t)kia_v4_reverse8(b[7] & 0xF0) << 24) |
        ((uint32_t)kia_v4_reverse8(b[6]) << 16) |
        ((uint32_t)kia_v4_reverse8(b[5]) << 8) |
        (uint32_t)kia_v4_reverse8(b[4]);
    
    instance->generic.btn = (kia_v4_reverse8(b[7]) & 0xF0) >> 4;
    
    instance->encrypted = 
        ((uint32_t)kia_v4_reverse8(b[3]) << 24) |
        ((uint32_t)kia_v4_reverse8(b[2]) << 16) |
        ((uint32_t)kia_v4_reverse8(b[1]) << 8) |
        (uint32_t)kia_v4_reverse8(b[0]);
    
    instance->decrypted = keeloq_decrypt(instance->encrypted, kia_v4_mf_key);
    instance->generic.cnt = instance->decrypted & 0xFFFF;
    
    FURI_LOG_D(TAG, "Raw: %016llX Bits: %u Inv: %016llX", 
        instance->decoder.decode_data, instance->decoder.decode_count_bit, inverted);
    FURI_LOG_D(TAG, "Sn: %07lX Btn: %X Enc: %08lX Dec: %08lX Cnt: %04lX", 
        instance->generic.serial, instance->generic.btn, instance->encrypted, 
        instance->decrypted, instance->generic.cnt);
}

void* kia_protocol_decoder_v4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV4* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV4));
    instance->base.protocol = &kia_protocol_v4;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encrypted = 0;
    instance->decrypted = 0;
    instance->short_pulse_count = 0;
    return instance;
}

void kia_protocol_decoder_v4_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    free(instance);
}

void kia_protocol_decoder_v4_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    instance->decoder.parser_step = KiaV4DecoderStepReset;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->header_count = 0;
    instance->short_pulse_count = 0;
    instance->encrypted = 0;
    instance->decrypted = 0;
}

void kia_protocol_decoder_v4_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;

    switch(instance->decoder.parser_step) {
    case KiaV4DecoderStepReset:
        if(level) {
            if(DURATION_DIFF(duration, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta) {
                instance->decoder.parser_step = KiaV4DecoderStepCheckPreamble;
                instance->header_count = 1;
                instance->short_pulse_count = 0;
            }
        }
        break;

    case KiaV4DecoderStepCheckPreamble:
        if(level) {
            if(DURATION_DIFF(duration, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta) {
                instance->header_count++;
                instance->short_pulse_count = 0;
            } else if(DURATION_DIFF(duration, kia_protocol_v4_const.te_short) < kia_protocol_v4_const.te_delta) {
                instance->short_pulse_count++;
            } else if(duration > 1000 && duration < 1500) {
                if(instance->header_count >= 6 && instance->short_pulse_count >= 2) {
                    FURI_LOG_D(TAG, "Sync: %lu (hdr=%u, short=%u)", 
                        duration, instance->header_count, instance->short_pulse_count);
                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 0;
                    instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
                } else {
                    instance->decoder.parser_step = KiaV4DecoderStepReset;
                }
            } else {
                instance->decoder.parser_step = KiaV4DecoderStepReset;
            }
        }
        break;

    case KiaV4DecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = KiaV4DecoderStepCheckDuration;
        } else {
            if(duration > 1500) {
                if(instance->decoder.decode_count_bit >= kia_protocol_v4_const.min_count_bit_for_found) {
                    kia_v4_extract_fields(instance);
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = KiaV4DecoderStepReset;
            }
        }
        break;

    case KiaV4DecoderStepCheckDuration:
        if(!level) {
            if(DURATION_DIFF(instance->decoder.te_last, kia_protocol_v4_const.te_short) < kia_protocol_v4_const.te_delta) {
                instance->decoder.decode_data <<= 1;
                instance->decoder.decode_count_bit++;
                instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
            } else if(DURATION_DIFF(instance->decoder.te_last, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta) {
                instance->decoder.decode_data <<= 1;
                instance->decoder.decode_data |= 1;
                instance->decoder.decode_count_bit++;
                instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
            } else {
                if(instance->decoder.decode_count_bit >= kia_protocol_v4_const.min_count_bit_for_found) {
                    kia_v4_extract_fields(instance);
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = KiaV4DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV4DecoderStepReset;
        }
        break;
    }
}

uint8_t kia_protocol_decoder_v4_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus kia_protocol_decoder_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v4_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v4_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;

    uint8_t b[8];
    for(int i = 0; i < 8; i++) {
        b[i] = (instance->generic.data >> (56 - i * 8)) & 0xFF;
    }
    uint64_t yek = 0;
    for(int i = 0; i < 8; i++) {
        yek |= ((uint64_t)kia_v4_reverse8(b[7-i]) << (56 - i * 8));
    }

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Yek:%016llX\r\n"
        "Sn:%07lX Btn:%X Cnt:%04lX\r\n"
        "Enc:%08lX Dec:%08lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->generic.data,
        yek,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt,
        instance->encrypted,
        instance->decrypted);
}