#include "kia_v4.h"

#define TAG "KiaProtocolV4"

static const SubGhzBlockConst kia_protocol_v4_const = {
    .te_short = 370,
    .te_long = 772,
    .te_delta = 150,
    .min_count_bit_for_found = 64,
};

#define KIA_V4_TE_SYNC 4000

static const uint64_t kia_v4_mf_keys[] = {
    0xA8F5DFFC8DAA5CDB,
    0xCB72B95709057920
};

struct SubGhzProtocolDecoderKiaV4 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint8_t model_type;
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
    KiaV4DecoderStepCheckSync,
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
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,
    .decoder = &kia_protocol_v4_decoder,
    .encoder = &kia_protocol_v4_encoder,
};

static uint8_t kia_v4_reverse8(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
}

static uint32_t kia_v4_keeloq_decrypt(uint32_t data, uint64_t key) {
    uint32_t result = data;
    for(int i = 0; i < 528; i++) {
        uint8_t nlf_input = ((result >> 1) & 1) | ((result >> 8) & 2) | ((result >> 18) & 4) | ((result >> 23) & 8) | ((result >> 27) & 16);
        uint32_t nlf = 0x3A5C742E;
        uint8_t nlf_bit = (nlf >> nlf_input) & 1;
        uint8_t key_bit = (key >> (i % 64)) & 1;
        uint8_t feedback = nlf_bit ^ ((result >> 16) & 1) ^ (result & 1) ^ key_bit;
        result = (result >> 1) | (feedback << 31);
    }
    return result;
}

static bool kia_v4_check_decrypt(SubGhzProtocolDecoderKiaV4* instance) {
    uint64_t data = instance->decoder.decode_data;
    uint8_t* b = (uint8_t*)&data;

    uint32_t encrypted = ((uint32_t)kia_v4_reverse8(b[3]) << 24) |
                         ((uint32_t)kia_v4_reverse8(b[2]) << 16) |
                         ((uint32_t)kia_v4_reverse8(b[1]) << 8) |
                         (uint32_t)kia_v4_reverse8(b[0]);

    uint32_t serial = ((uint32_t)kia_v4_reverse8(b[7] & 0xF0) << 24) |
                      ((uint32_t)kia_v4_reverse8(b[6]) << 16) |
                      ((uint32_t)kia_v4_reverse8(b[5]) << 8) |
                      (uint32_t)kia_v4_reverse8(b[4]);

    uint8_t btn = (kia_v4_reverse8(b[7]) & 0xF0) >> 4;

    instance->encrypted = encrypted;
    instance->generic.serial = serial;
    instance->generic.btn = btn;

    for(size_t j = 0; j < 2; j++) {
        uint32_t block = kia_v4_keeloq_decrypt(encrypted, kia_v4_mf_keys[j]);
        uint8_t dec_btn = (uint8_t)(block >> 28);
        uint8_t dec_serial_low = (uint8_t)((block >> 16) & 0xFF);
        uint8_t serial_low = (uint8_t)(serial & 0xFF);

        if((btn == dec_btn) && (serial_low == dec_serial_low)) {
            instance->decrypted = block;
            instance->generic.cnt = (uint16_t)(block & 0xFFFF);
            instance->model_type = (kia_v4_mf_keys[j] == 0xA8F5DFFC8DAA5CDB) ? 4 : 3;
            return true;
        }
    }

    instance->decrypted = 0;
    instance->model_type = 0;
    return true;
}

void* kia_protocol_decoder_v4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV4* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV4));
    instance->base.protocol = &kia_protocol_v4;
    instance->generic.protocol_name = instance->base.protocol->name;
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
    instance->model_type = 0;
    instance->encrypted = 0;
    instance->decrypted = 0;
}

void kia_protocol_decoder_v4_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;

    switch(instance->decoder.parser_step) {
    case KiaV4DecoderStepReset:
        if(!level && (duration > KIA_V4_TE_SYNC)) {
            instance->decoder.parser_step = KiaV4DecoderStepCheckSync;
        }
        break;

    case KiaV4DecoderStepCheckSync:
        if(level) {
            if((DURATION_DIFF(duration, kia_protocol_v4_const.te_short) < kia_protocol_v4_const.te_delta) ||
               (DURATION_DIFF(duration, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta)) {
                instance->decoder.te_last = duration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = KiaV4DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV4DecoderStepReset;
        }
        break;

    case KiaV4DecoderStepSaveDuration:
        if(!level) {
            if(duration > KIA_V4_TE_SYNC) {
                if(instance->decoder.decode_count_bit >= kia_protocol_v4_const.min_count_bit_for_found) {
                    instance->decoder.decode_data = ~instance->decoder.decode_data;
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    if(kia_v4_check_decrypt(instance)) {
                        if(instance->base.callback) {
                            instance->base.callback(&instance->base, instance->base.context);
                        }
                    }
                }
                instance->decoder.parser_step = KiaV4DecoderStepReset;
            } else {
                instance->decoder.parser_step = KiaV4DecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = KiaV4DecoderStepReset;
        }
        break;

    case KiaV4DecoderStepCheckDuration:
        if(level) {
            bool prev_short = (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v4_const.te_short) < kia_protocol_v4_const.te_delta);
            bool prev_long = (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta);
            bool curr_short = (DURATION_DIFF(duration, kia_protocol_v4_const.te_short) < kia_protocol_v4_const.te_delta);
            bool curr_long = (DURATION_DIFF(duration, kia_protocol_v4_const.te_long) < kia_protocol_v4_const.te_delta);

            if(prev_short && curr_long) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
            } else if(prev_long && curr_short) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = KiaV4DecoderStepSaveDuration;
            } else {
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
    return subghz_protocol_blocks_get_hash_data(&instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v4_serialize(void* context, FlipperFormat* flipper_format, SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus kia_protocol_decoder_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, kia_protocol_v4_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v4_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV4* instance = context;

    const char* model_name;
    if(instance->model_type == 4) {
        model_name = "Kia V4";
    } else if(instance->model_type == 3) {
        model_name = "Kia V3";
    } else {
        model_name = "Kia V3/4";
    }

    if(instance->decrypted != 0) {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:%08lX%08lX\r\n"
            "Sn:%07lX Btn:%X Cnt:%04lX\r\n"
            "Enc:%08lX",
            model_name,
            instance->generic.data_count_bit,
            (uint32_t)(instance->generic.data >> 32),
            (uint32_t)(instance->generic.data & 0xFFFFFFFF),
            instance->generic.serial,
            instance->generic.btn,
            instance->generic.cnt,
            instance->encrypted);
    } else {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:%08lX%08lX\r\n"
            "Sn:%07lX Btn:%X\r\n"
            "Enc:%08lX (no key)",
            model_name,
            instance->generic.data_count_bit,
            (uint32_t)(instance->generic.data >> 32),
            (uint32_t)(instance->generic.data & 0xFFFFFFFF),
            instance->generic.serial,
            instance->generic.btn,
            instance->encrypted);
    }
}