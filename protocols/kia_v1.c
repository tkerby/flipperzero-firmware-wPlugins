#include "kia_v1.h"

#define TAG "KiaProtocolV1"

static const SubGhzBlockConst kia_protocol_v1_const = {
    .te_short = 800,
    .te_long = 1600,
    .te_delta = 200,
    .min_count_bit_for_found = 56,
};

struct SubGhzProtocolDecoderKiaV1 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

struct SubGhzProtocolEncoderKiaV1 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    KiaV1DecoderStepReset = 0,
    KiaV1DecoderStepCheckPreamble,
    KiaV1DecoderStepSaveDuration,
    KiaV1DecoderStepCheckDuration,
} KiaV1DecoderStep;

const SubGhzProtocolDecoder kia_protocol_v1_decoder = {
    .alloc = kia_protocol_decoder_v1_alloc,
    .free = kia_protocol_decoder_v1_free,
    .feed = kia_protocol_decoder_v1_feed,
    .reset = kia_protocol_decoder_v1_reset,
    .get_hash_data = kia_protocol_decoder_v1_get_hash_data,
    .serialize = kia_protocol_decoder_v1_serialize,
    .deserialize = kia_protocol_decoder_v1_deserialize,
    .get_string = kia_protocol_decoder_v1_get_string,
};

const SubGhzProtocolEncoder kia_protocol_v1_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol kia_protocol_v1 = {
    .name = KIA_PROTOCOL_V1_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,
    .decoder = &kia_protocol_v1_decoder,
    .encoder = &kia_protocol_v1_encoder,
};

void* kia_protocol_decoder_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV1* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV1));
    instance->base.protocol = &kia_protocol_v1;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void kia_protocol_decoder_v1_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    free(instance);
}

void kia_protocol_decoder_v1_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    instance->decoder.parser_step = KiaV1DecoderStepReset;
    instance->header_count = 0;
}

void kia_protocol_decoder_v1_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    switch(instance->decoder.parser_step) {
    case KiaV1DecoderStepReset:
        if((level) && (DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta)) {
            instance->decoder.parser_step = KiaV1DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;

    case KiaV1DecoderStepCheckPreamble:
        if(level) {
            if((DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta) ||
               (DURATION_DIFF(duration, kia_protocol_v1_const.te_long) < kia_protocol_v1_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
            }
        } else if((DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta) &&
                  (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta)) {
            instance->header_count++;
        } else if((DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta) &&
                  (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) < kia_protocol_v1_const.te_delta)) {
            if(instance->header_count > 20) {
                instance->decoder.parser_step = KiaV1DecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            } else {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV1DecoderStepReset;
        }
        break;

    case KiaV1DecoderStepSaveDuration:
        if(level) {
            if(duration >= (kia_protocol_v1_const.te_long * 2)) {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
                if(instance->decoder.decode_count_bit >= kia_protocol_v1_const.min_count_bit_for_found) {
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
                instance->decoder.parser_step = KiaV1DecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = KiaV1DecoderStepReset;
        }
        break;

    case KiaV1DecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta) &&
               (DURATION_DIFF(duration, kia_protocol_v1_const.te_long) < kia_protocol_v1_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = KiaV1DecoderStepSaveDuration;
            } else if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) < kia_protocol_v1_const.te_delta) &&
                      (DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = KiaV1DecoderStepSaveDuration;
            } else if((DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta) &&
                      (DURATION_DIFF(duration, kia_protocol_v1_const.te_short) < kia_protocol_v1_const.te_delta)) {
                instance->decoder.parser_step = KiaV1DecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KiaV1DecoderStepReset;
        }
        break;
    }
}

static void kia_v1_check_remote(SubGhzBlockGeneric* instance) {
    instance->serial = (uint32_t)((instance->data >> 24) & 0xFFFFFFFF);
    instance->btn = (instance->data >> 16) & 0xFF;
    instance->cnt = (instance->data >> 8) & 0xFF;
}

uint8_t kia_protocol_decoder_v1_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    return subghz_protocol_blocks_get_hash_data(&instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v1_serialize(void* context, FlipperFormat* flipper_format, SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus kia_protocol_decoder_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, kia_protocol_v1_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v1_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    kia_v1_check_remote(&instance->generic);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%08lX Btn:%02X Cnt:%02lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)(instance->generic.data & 0xFFFFFFFF),
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}