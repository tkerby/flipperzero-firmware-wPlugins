#include "kia_v2.h"

#define TAG "KiaV2"

static const SubGhzBlockConst kia_protocol_v2_const = {
    .te_short = 500,
    .te_long = 1000,
    .te_delta = 150,
    .min_count_bit_for_found = 53,
};

struct SubGhzProtocolDecoderKiaV2
{
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
    
    ManchesterState manchester_state;
};

struct SubGhzProtocolEncoderKiaV2
{
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum
{
    KiaV2DecoderStepReset = 0,
    KiaV2DecoderStepCheckPreamble,
    KiaV2DecoderStepCollectRawBits,
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
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable,
    .decoder = &kia_protocol_v2_decoder,
    .encoder = &kia_protocol_v2_encoder,
};

void *kia_protocol_decoder_v2_alloc(SubGhzEnvironment *environment)
{
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV2 *instance = malloc(sizeof(SubGhzProtocolDecoderKiaV2));
    instance->base.protocol = &kia_protocol_v2;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void kia_protocol_decoder_v2_free(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;
    free(instance);
}

void kia_protocol_decoder_v2_reset(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;
    instance->decoder.parser_step = KiaV2DecoderStepReset;
    instance->header_count = 0;
    instance->manchester_state = ManchesterStateMid1;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
}

void kia_protocol_decoder_v2_feed(void *context, bool level, uint32_t duration)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;

    switch (instance->decoder.parser_step)
    {
    case KiaV2DecoderStepReset:
        if ((level) && (DURATION_DIFF(duration, kia_protocol_v2_const.te_long) <
                        kia_protocol_v2_const.te_delta))
        {
            instance->decoder.parser_step = KiaV2DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
            manchester_advance(instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
        }
        break;

    case KiaV2DecoderStepCheckPreamble:
        if (level)  // HIGH pulse
        {
            if (DURATION_DIFF(duration, kia_protocol_v2_const.te_long) <
                kia_protocol_v2_const.te_delta)
            {
                instance->decoder.te_last = duration;
                instance->header_count++;
            }
            else if (
                DURATION_DIFF(duration, kia_protocol_v2_const.te_short) <
                kia_protocol_v2_const.te_delta)
            {
                if (instance->header_count >= 100)
                {
                    instance->header_count = 0;
                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 1;
                    instance->decoder.parser_step = KiaV2DecoderStepCollectRawBits;
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                }
                else
                {
                    instance->decoder.te_last = duration;
                }
            }
            else
            {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        }
        else
        {
            if (DURATION_DIFF(duration, kia_protocol_v2_const.te_long) <
                kia_protocol_v2_const.te_delta)
            {
                instance->header_count++;
                instance->decoder.te_last = duration;
            }
            else if (
                DURATION_DIFF(duration, kia_protocol_v2_const.te_short) <
                kia_protocol_v2_const.te_delta)
            {
                instance->decoder.te_last = duration;
            }
            else
            {
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        }
        break;

    case KiaV2DecoderStepCollectRawBits:
    {
        ManchesterEvent event;
        
        if (DURATION_DIFF(duration, kia_protocol_v2_const.te_short) <
            kia_protocol_v2_const.te_delta)
        {
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        }
        else if (
            DURATION_DIFF(duration, kia_protocol_v2_const.te_long) <
            kia_protocol_v2_const.te_delta)
        {
            event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
        }
        else
        {
            instance->decoder.parser_step = KiaV2DecoderStepReset;
            break;
        }

        bool data_bit;
        if (manchester_advance(instance->manchester_state, event, &instance->manchester_state, &data_bit))
        {
            instance->decoder.decode_data = (instance->decoder.decode_data << 1) | data_bit;
            instance->decoder.decode_count_bit++;

            if (instance->decoder.decode_count_bit == 53)
            {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                instance->generic.serial = (uint32_t)((instance->generic.data >> 20) & 0xFFFFFFFF);
                instance->generic.btn = (uint8_t)((instance->generic.data >> 16) & 0x0F);

                uint16_t raw_count = (uint16_t)((instance->generic.data >> 4) & 0xFFF);
                instance->generic.cnt = ((raw_count >> 4) | (raw_count << 8)) & 0xFFF;

                if (instance->base.callback)
                    instance->base.callback(&instance->base, instance->base.context);

                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->header_count = 0;
                instance->decoder.parser_step = KiaV2DecoderStepReset;
            }
        }
        break;
    }
    }
}

uint8_t kia_protocol_decoder_v2_get_hash_data(void *context)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v2_serialize(
    void *context,
    FlipperFormat *flipper_format,
    SubGhzRadioPreset *preset)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    ret = subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if (ret == SubGhzProtocolStatusOk)
    {
        // Save CRC (last nibble)
        uint32_t crc = instance->generic.data & 0x0F;
        flipper_format_write_uint32(flipper_format, "CRC", &crc, 1);

        // Save decoded fields
        flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1);

        uint32_t temp = instance->generic.btn;
        flipper_format_write_uint32(flipper_format, "Btn", &temp, 1);

        flipper_format_write_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1);

        // Save raw count before transformation (for exact reproduction)
        uint32_t raw_count = (uint16_t)((instance->generic.data >> 4) & 0xFFF);
        flipper_format_write_uint32(flipper_format, "RawCnt", &raw_count, 1);
    }

    return ret;
}

SubGhzProtocolStatus
kia_protocol_decoder_v2_deserialize(void *context, FlipperFormat *flipper_format)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v2_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v2_get_string(void *context, FuriString *output)
{
    furi_assert(context);
    SubGhzProtocolDecoderKiaV2 *instance = context;

    uint8_t crc = instance->generic.data & 0x0F;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%013llX\r\n"
        "Sn:%08lX Btn:%X\r\n"
        "Cnt:%03lX CRC:%X\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->generic.data,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt,
        crc);
}
