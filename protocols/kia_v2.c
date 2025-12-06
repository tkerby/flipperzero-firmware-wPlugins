#include "kia_v2.h"

#define TAG "KiaProtocolV2"

static const SubGhzBlockConst kia_protocol_v2_const = {
    .te_short = 500,
    .te_long = 1000,
    .te_delta = 150,
    .min_count_bit_for_found = 52,
};

struct SubGhzProtocolDecoderKiaV2 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

struct SubGhzProtocolEncoderKiaV2 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    KiaV2DecoderStepReset = 0,
    KiaV2DecoderStepCheckPreamble,
    KiaV2DecoderStepSaveDuration,
    KiaV2DecoderStepCheckDuration,
} KiaV2DecoderStep;

const SubGhzProtocolDecoder kia_protocol_v2_decoder = {
    .alloc = kia_protocol_decoder_v2_alloc,
    .free = kia_protocol_decoder_v2_free,
    .feed = kia_protocol_decoder_v2_feed,
    .reset = kia_protocol_decoder_v2_reset,
    .get_hash_data = kia_protocol_decoder_v2_get_hash_data,
    .serialize = kia_protocol_decoder_v2_serialize,
    .deserialize = kia_protocol_decoder_v2_deserialize,
    .get_string = kia_protocol_decoder_v2_get_string,
};

const SubGhzProtocolEncoder kia_protocol_v2_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol kia_protocol_v2 = {
    .name = KIA_PROTOCOL_V2_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable,
    .decoder = &kia_protocol_v2_decoder,
    .encoder = &kia_protocol_v2_encoder,
};

void* kia_protocol_decoder_v2_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV2* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV2));
    instance->base.protocol = &kia_protocol_v2;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void kia_protocol_decoder_v2_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    free(instance);
}

void kia_protocol_decoder_v2_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    instance->decoder.parser_step = KiaV2DecoderStepReset;
    instance->header_count = 0;
}

void kia_protocol_decoder_v2_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;

    switch(instance->decoder.parser_step) {
    case KiaV2DecoderStepReset:
        if((level) && (DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta)) {
            instance->decoder.parser_step = KiaV2DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;

    case KiaV2DecoderStepCheckPreamble:
        if(level) {
            if((DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta) ||
               (DURATION_DIFF(duration, kia_protocol_v2_const.te_long) < kia_protocol_v2_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        } else if((DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta) &&
                  (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta)) {
            instance->header_count++;
        } else if((DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta) &&
                  (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v2_const.te_long) < kia_protocol_v2_const.te_delta)) {
            if(instance->header_count > 12) {
                instance->decoder.parser_step = KiaV2DecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            } else {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV2DecoderStepReset;
        }
        break;

    case KiaV2DecoderStepSaveDuration:
        if(level) {
            if(duration >= (kia_protocol_v2_const.te_long * 2)) {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
                if(instance->decoder.decode_count_bit >= kia_protocol_v2_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    if(instance->base.callback) {
                        instance->base.callback(&instance->base, instance->base.context);
                    }
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = KiaV2DecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = KiaV2DecoderStepReset;
        }
        break;

    case KiaV2DecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta) &&
               (DURATION_DIFF(duration, kia_protocol_v2_const.te_long) < kia_protocol_v2_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = KiaV2DecoderStepSaveDuration;
            } else if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v2_const.te_long) < kia_protocol_v2_const.te_delta) &&
                      (DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = KiaV2DecoderStepSaveDuration;
            } else if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta) &&
                      (DURATION_DIFF(duration, kia_protocol_v2_const.te_short) < kia_protocol_v2_const.te_delta)) {
                instance->decoder.parser_step = KiaV2DecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV2DecoderStepReset;
        }
        break;
    }
}

static void kia_v2_check_remote(SubGhzBlockGeneric* instance) {
    instance->serial = (uint32_t)((instance->data >> 20) & 0xFFFFFFFF);
    instance->btn = (instance->data >> 16) & 0x0F;
    uint16_t count_raw = (uint16_t)((instance->data >> 4) & 0xFFF);
    instance->cnt = (uint16_t)(((count_raw >> 4) | (count_raw << 8)) & 0xFFF);
}

uint8_t kia_protocol_decoder_v2_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    return subghz_protocol_blocks_get_hash_data(&instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v2_serialize(void* context, FlipperFormat* flipper_format, SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus kia_protocol_decoder_v2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, kia_protocol_v2_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v2_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2* instance = context;
    kia_v2_check_remote(&instance->generic);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%08lX Btn:%X Cnt:%03lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)(instance->generic.data & 0xFFFFFFFF),
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}