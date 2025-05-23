#include "came.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

/*
 * Help
 * https://phreakerclub.com/447
 *
 */

#define TAG "SubGhzProtocolCame"

#define CAME_12_COUNT_BIT    12
#define CAME_24_COUNT_BIT    24
#define PRASTEL_25_COUNT_BIT 25
#define PRASTEL_42_COUNT_BIT 42
#define PRASTEL_NAME         "Prastel"
#define AIRFORCE_COUNT_BIT   18
#define AIRFORCE_NAME        "Airforce"

static const SubGhzBlockConst subghz_protocol_came_const = {
    .te_short = 320,
    .te_long = 640,
    .te_delta = 150,
    .min_count_bit_for_found = 12,
};

struct SubGhzProtocolDecoderCame {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
};

struct SubGhzProtocolEncoderCame {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    CameDecoderStepReset = 0,
    CameDecoderStepFoundStartBit,
    CameDecoderStepSaveDuration,
    CameDecoderStepCheckDuration,
} CameDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_came_decoder = {
    .alloc = subghz_protocol_decoder_came_alloc,
    .free = subghz_protocol_decoder_came_free,

    .feed = subghz_protocol_decoder_came_feed,
    .reset = subghz_protocol_decoder_came_reset,

    .get_hash_data = NULL,
    .get_hash_data_long = subghz_protocol_decoder_came_get_hash_data,
    .serialize = subghz_protocol_decoder_came_serialize,
    .deserialize = subghz_protocol_decoder_came_deserialize,
    .get_string = subghz_protocol_decoder_came_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder subghz_protocol_came_encoder = {
    .alloc = subghz_protocol_encoder_came_alloc,
    .free = subghz_protocol_encoder_came_free,

    .deserialize = subghz_protocol_encoder_came_deserialize,
    .stop = subghz_protocol_encoder_came_stop,
    .yield = subghz_protocol_encoder_came_yield,
};

const SubGhzProtocol subghz_protocol_came = {
    .name = SUBGHZ_PROTOCOL_CAME_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Send,

    .decoder = &subghz_protocol_came_decoder,
    .encoder = &subghz_protocol_came_encoder,
};

void* subghz_protocol_encoder_came_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderCame* instance = malloc(sizeof(SubGhzProtocolEncoderCame));

    instance->base.protocol = &subghz_protocol_came;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 128;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;
    return instance;
}

void subghz_protocol_encoder_came_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderCame* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

/**
 * Generating an upload from data.
 * @param instance Pointer to a SubGhzProtocolEncoderCame instance
 * @return true On success
 */
static bool subghz_protocol_encoder_came_get_upload(SubGhzProtocolEncoderCame* instance) {
    furi_assert(instance);
    uint32_t header_te = 0;
    size_t index = 0;
    size_t size_upload = (instance->generic.data_count_bit * 2) + 2;
    if(size_upload > instance->encoder.size_upload) {
        FURI_LOG_E(TAG, "Size upload exceeds allocated encoder buffer.");
        return false;
    } else {
        instance->encoder.size_upload = size_upload;
    }
    //Send header

    switch(instance->generic.data_count_bit) {
    case CAME_24_COUNT_BIT:
    case PRASTEL_42_COUNT_BIT:
        // CAME 24 Bit = 24320 us
        header_te = 76;
        break;
    case CAME_12_COUNT_BIT:
    case AIRFORCE_COUNT_BIT:
        // CAME 12 Bit Original only! and Airforce protocol = 15040 us
        header_te = 47;
        break;
    case PRASTEL_25_COUNT_BIT:
        // PRASTEL = 11520 us
        header_te = 36;
        break;
    default:
        // Some wrong detected protocols, 5120 us
        header_te = 16;
        break;
    }
    instance->encoder.upload[index++] =
        level_duration_make(false, (uint32_t)subghz_protocol_came_const.te_short * header_te);
    //Send start bit
    instance->encoder.upload[index++] =
        level_duration_make(true, (uint32_t)subghz_protocol_came_const.te_short);
    //Send key data
    for(uint8_t i = instance->generic.data_count_bit; i > 0; i--) {
        if(bit_read(instance->generic.data, i - 1)) {
            //send bit 1
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_came_const.te_long);
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_came_const.te_short);
        } else {
            //send bit 0
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_came_const.te_short);
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_came_const.te_long);
        }
    }
    return true;
}

SubGhzProtocolStatus
    subghz_protocol_encoder_came_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderCame* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize(&instance->generic, flipper_format);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(instance->generic.data_count_bit > PRASTEL_42_COUNT_BIT) {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            ret = SubGhzProtocolStatusErrorValueBitCount;
            break;
        }
        //optional parameter parameter
        flipper_format_read_uint32(
            flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1);

        if(!subghz_protocol_encoder_came_get_upload(instance)) {
            ret = SubGhzProtocolStatusErrorEncoderGetUpload;
            break;
        }
        instance->encoder.is_running = true;
    } while(false);

    return ret;
}

void subghz_protocol_encoder_came_stop(void* context) {
    SubGhzProtocolEncoderCame* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_came_yield(void* context) {
    SubGhzProtocolEncoderCame* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

void* subghz_protocol_decoder_came_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderCame* instance = malloc(sizeof(SubGhzProtocolDecoderCame));
    instance->base.protocol = &subghz_protocol_came;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_came_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    free(instance);
}

void subghz_protocol_decoder_came_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    instance->decoder.parser_step = CameDecoderStepReset;
}

void subghz_protocol_decoder_came_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    switch(instance->decoder.parser_step) {
    case CameDecoderStepReset:
        if((!level) && (DURATION_DIFF(duration, subghz_protocol_came_const.te_short * 56) <
                        subghz_protocol_came_const.te_delta * 47)) {
            //Found header CAME
            instance->decoder.parser_step = CameDecoderStepFoundStartBit;
        }
        break;
    case CameDecoderStepFoundStartBit:
        if(!level) {
            break;
        } else if(
            DURATION_DIFF(duration, subghz_protocol_came_const.te_short) <
            subghz_protocol_came_const.te_delta) {
            //Found start bit CAME
            instance->decoder.parser_step = CameDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        } else {
            instance->decoder.parser_step = CameDecoderStepReset;
        }
        break;
    case CameDecoderStepSaveDuration:
        if(!level) { //save interval
            if(duration >= (subghz_protocol_came_const.te_short * 4)) {
                instance->decoder.parser_step = CameDecoderStepFoundStartBit;
                if((instance->decoder.decode_count_bit ==
                    subghz_protocol_came_const.min_count_bit_for_found) ||
                   (instance->decoder.decode_count_bit == AIRFORCE_COUNT_BIT) ||
                   (instance->decoder.decode_count_bit == PRASTEL_25_COUNT_BIT) ||
                   (instance->decoder.decode_count_bit == PRASTEL_42_COUNT_BIT) ||
                   (instance->decoder.decode_count_bit == CAME_24_COUNT_BIT)) {
                    instance->generic.serial = 0x0;
                    instance->generic.btn = 0x0;

                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                break;
            }
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = CameDecoderStepCheckDuration;
        } else {
            instance->decoder.parser_step = CameDecoderStepReset;
        }
        break;
    case CameDecoderStepCheckDuration:
        if(level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_came_const.te_short) <
                subghz_protocol_came_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_came_const.te_long) <
                subghz_protocol_came_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = CameDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_came_const.te_long) <
                 subghz_protocol_came_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_came_const.te_short) <
                 subghz_protocol_came_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = CameDecoderStepSaveDuration;
            } else
                instance->decoder.parser_step = CameDecoderStepReset;
        } else {
            instance->decoder.parser_step = CameDecoderStepReset;
        }
        break;
    }
}

uint32_t subghz_protocol_decoder_came_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_came_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    subghz_protocol_decoder_came_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize(&instance->generic, flipper_format);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(instance->generic.data_count_bit > PRASTEL_42_COUNT_BIT) {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            ret = SubGhzProtocolStatusErrorValueBitCount;
            break;
        }
    } while(false);
    return ret;
}

void subghz_protocol_decoder_came_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderCame* instance = context;

    uint32_t code_found_lo = instance->generic.data & 0x000003ffffffffff;

    uint64_t code_found_reverse = subghz_protocol_blocks_reverse_key(
        instance->generic.data, instance->generic.data_count_bit);

    uint32_t code_found_reverse_lo = code_found_reverse & 0x000003ffffffffff;

    const char* name = instance->generic.protocol_name;
    switch(instance->generic.data_count_bit) {
    case PRASTEL_25_COUNT_BIT:
    case PRASTEL_42_COUNT_BIT:
        name = PRASTEL_NAME;
        break;
    case AIRFORCE_COUNT_BIT:
        name = AIRFORCE_NAME;
        break;
    }

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%08lX\r\n"
        "Yek:0x%08lX\r\n",
        name,
        instance->generic.data_count_bit,
        code_found_lo,
        code_found_reverse_lo);
}
