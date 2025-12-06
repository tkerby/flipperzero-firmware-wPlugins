#include "kia_v0.h"

#define TAG "KiaProtocolV0"

static const SubGhzBlockConst subghz_protocol_kia_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 61,
};

struct SubGhzProtocolDecoderKIA {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

struct SubGhzProtocolEncoderKIA {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    KIADecoderStepReset = 0,
    KIADecoderStepCheckPreambula,
    KIADecoderStepSaveDuration,
    KIADecoderStepCheckDuration,
} KIADecoderStep;

const SubGhzProtocolDecoder subghz_protocol_kia_decoder = {
    .alloc = subghz_protocol_decoder_kia_alloc,
    .free = subghz_protocol_decoder_kia_free,
    .feed = subghz_protocol_decoder_kia_feed,
    .reset = subghz_protocol_decoder_kia_reset,
    .get_hash_data = subghz_protocol_decoder_kia_get_hash_data,
    .serialize = subghz_protocol_decoder_kia_serialize,
    .deserialize = subghz_protocol_decoder_kia_deserialize,
    .get_string = subghz_protocol_decoder_kia_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_kia_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol kia_protocol_v0 = {
    .name = KIA_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable,
    .decoder = &subghz_protocol_kia_decoder,
    .encoder = &subghz_protocol_kia_encoder,
};

void* subghz_protocol_decoder_kia_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKIA* instance = malloc(sizeof(SubGhzProtocolDecoderKIA));
    instance->base.protocol = &kia_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_kia_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;
    free(instance);
}

void subghz_protocol_decoder_kia_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;
    instance->decoder.parser_step = KIADecoderStepReset;
}

void subghz_protocol_decoder_kia_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;

    switch(instance->decoder.parser_step) {
    case KIADecoderStepReset:
        if((level) &&
           (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta)) {
            instance->decoder.parser_step = KIADecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;
    case KIADecoderStepCheckPreambula:
        if(level) {
            if((DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta) ||
               (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) < subghz_protocol_kia_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta)) {
            instance->header_count++;
            break;
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) < subghz_protocol_kia_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_long) < subghz_protocol_kia_const.te_delta)) {
            if(instance->header_count > 15) {
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 1;
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
            } else {
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;
    case KIADecoderStepSaveDuration:
        if(level) {
            if(duration >= (subghz_protocol_kia_const.te_long + subghz_protocol_kia_const.te_delta * 2UL)) {
                instance->decoder.parser_step = KIADecoderStepReset;
                if(instance->decoder.decode_count_bit == subghz_protocol_kia_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = KIADecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;
    case KIADecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) < subghz_protocol_kia_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_long) < subghz_protocol_kia_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) < subghz_protocol_kia_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_kia_check_remote_controller(SubGhzBlockGeneric* instance) {
    instance->serial = (uint32_t)((instance->data >> 12) & 0x0FFFFFFF);
    instance->btn = (instance->data >> 8) & 0x0F;
    instance->cnt = (instance->data >> 40) & 0xFFFF;
}

uint8_t subghz_protocol_decoder_kia_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_kia_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_kia_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_kia_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_kia_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKIA* instance = context;

    subghz_protocol_kia_check_remote_controller(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%07lX Btn:%X Cnt:%04lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt);
}