// Created by FatherDivine (GH)

#include "lacrosse_tx141bv3.h"

#define TAG "WSProtocolLaCrosse_TX141BV3"

/*
 * Help / reference:
 *   - Flipper TX141THBv2 decoder:
 *       applications/external/weather_station/protocols/lacrosse_tx141thbv2.c
 *   - rtl_433 LaCrosse TX141-Bv2 / TX141TH-Bv2 / TX141-Bv3:
 *       https://github.com/merbanan/rtl_433/blob/master/src/devices/lacrosse_tx141x.c
 *
 * This file adds support for LaCrosse TX141-Bv3 (FCC ID: OMOTX141V3),
 * a 433.92 MHz temperature-only sensor.
 *
 * rtl_433 notes for TX141Bv3:
 *   - Temperature in °C, 12-bit, offset 500, scale 10:
 *       temp_c = (temp_raw - 500) * 0.1
 *   - ID: 8-bit
 *   - Flags byte:
 *       bit 7: battery (inverted vs TX141TH-Bv2)
 *       bit 6: test button
 *       bits 5..4: channel (1..3)
 *       bits 3..0: upper 4 bits of temp
 */

#define LACROSSE_TX141_BV3_BIT_COUNT 33

static const SubGhzBlockConst ws_protocol_lacrosse_tx141bv3_const = {
    .te_short = 208, // matches TX141THBv2 / rtl_433 timing
    .te_long = 417,
    .te_delta = 120,
    .min_count_bit_for_found = 32,
};

struct WSProtocolDecoderLaCrosse_TX141BV3 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    WSBlockGeneric generic;
    uint16_t header_count;
};

struct WSProtocolEncoderLaCrosse_TX141BV3 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    WSBlockGeneric generic;
};

typedef enum {
    LaCrosse_TX141BV3DecoderStepReset = 0,
    LaCrosse_TX141BV3DecoderStepCheckPreambule,
    LaCrosse_TX141BV3DecoderStepSaveDuration,
    LaCrosse_TX141BV3DecoderStepCheckDuration,
} LaCrosse_TX141BV3DecoderStep;

const SubGhzProtocolDecoder ws_protocol_lacrosse_tx141bv3_decoder = {
    .alloc = ws_protocol_decoder_lacrosse_tx141bv3_alloc,
    .free = ws_protocol_decoder_lacrosse_tx141bv3_free,
    .feed = ws_protocol_decoder_lacrosse_tx141bv3_feed,
    .reset = ws_protocol_decoder_lacrosse_tx141bv3_reset,
    .get_hash_data = NULL,
    .get_hash_data_long = ws_protocol_decoder_lacrosse_tx141bv3_get_hash_data,
    .serialize = ws_protocol_decoder_lacrosse_tx141bv3_serialize,
    .deserialize = ws_protocol_decoder_lacrosse_tx141bv3_deserialize,
    .get_string = ws_protocol_decoder_lacrosse_tx141bv3_get_string,
    .get_string_brief = NULL,
};

const SubGhzProtocolEncoder ws_protocol_lacrosse_tx141bv3_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ws_protocol_lacrosse_tx141bv3 = {
    .name = WS_PROTOCOL_LACROSSE_TX141BV3_NAME,
    .type = SubGhzProtocolWeatherStation,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable,
    .decoder = &ws_protocol_lacrosse_tx141bv3_decoder,
    .encoder = &ws_protocol_lacrosse_tx141bv3_encoder,
};

void* ws_protocol_decoder_lacrosse_tx141bv3_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    WSProtocolDecoderLaCrosse_TX141BV3* instance =
        malloc(sizeof(WSProtocolDecoderLaCrosse_TX141BV3));

    instance->base.protocol = &ws_protocol_lacrosse_tx141bv3;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void ws_protocol_decoder_lacrosse_tx141bv3_free(void* context) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;
    free(instance);
}

void ws_protocol_decoder_lacrosse_tx141bv3_reset(void* context) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;

    instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->header_count = 0;
}

/**
 * Basic sanity/validity check based on rtl_433 interpretation:
 *   id != 0, temperature within plausible range.
 * We do NOT have a documented checksum for TX141-Bv3, so we trust
 * timing, bit length, and value sanity.
 */
static bool ws_protocol_lacrosse_tx141bv3_check_frame(WSProtocolDecoderLaCrosse_TX141BV3* instance) {
    if(!instance->decoder.decode_data) return false;

    uint64_t data = instance->decoder.decode_data;

    if(instance->decoder.decode_count_bit == LACROSSE_TX141_BV3_BIT_COUNT) {
        // Drop the trailing "u" bit, similar to TX141THBv2 handling 41->40 bits
        data >>= 1;
    }

    // Align to 32 bits and extract bytes like rtl_433's b[0], b[1], b[2]
    uint8_t b0 = (data >> 24) & 0xFF;
    uint8_t b1 = (data >> 16) & 0xFF;
    uint8_t b2 = (data >> 8) & 0xFF;

    uint8_t id = b0;

    // Battery bit inverted vs TX141TH-Bv2 (see rtl_433 lacrosse_tx141x.c)
    bool battery_low = !(b1 >> 7);
    UNUSED(battery_low);

    uint8_t channel = (b1 & 0x30) >> 4;
    uint16_t temp_raw = ((uint16_t)(b1 & 0x0F) << 8) | b2;
    float temp_c = ((float)temp_raw - 500.0f) / 10.0f;

    if(id == 0) return false;
    if((channel < 1) || (channel > 3)) return false;
    if((temp_c < -40.0f) || (temp_c > 60.0f)) return false;

    return true;
}

/**
 * Analysis of received data
 * @param instance Pointer to a WSBlockGeneric* instance
 */
static void ws_protocol_lacrosse_tx141bv3_remote_controller(WSBlockGeneric* instance) {
    uint64_t data = instance->data;

    if(instance->data_count_bit == LACROSSE_TX141_BV3_BIT_COUNT) {
        data >>= 1;
    }

    uint8_t b0 = (data >> 24) & 0xFF;
    uint8_t b1 = (data >> 16) & 0xFF;
    uint8_t b2 = (data >> 8) & 0xFF;

    uint8_t id = b0;
    bool battery_low = !(b1 >> 7);
    uint8_t test = (b1 & 0x40) >> 6;
    uint8_t channel = ((b1 & 0x30) >> 4) + 1; // 1..3

    uint16_t temp_raw = ((uint16_t)(b1 & 0x0F) << 8) | b2;
    float temp_c = ((float)temp_raw - 500.0f) / 10.0f;

    instance->id = id;
	instance->battery_low = battery_low;
	instance->btn = test;
    instance->channel = channel;
    instance->temp = temp_c;
    instance->humidity = WS_NO_HUMIDITY;
}

static bool ws_protocol_decoder_lacrosse_tx141bv3_add_bit(
    WSProtocolDecoderLaCrosse_TX141BV3* instance,
    uint32_t te_last,
    uint32_t te_current) {
    furi_assert(instance);
    bool ret = false;

    if(DURATION_DIFF(
           te_last + te_current,
           ws_protocol_lacrosse_tx141bv3_const.te_short + ws_protocol_lacrosse_tx141bv3_const.te_long) <
       ws_protocol_lacrosse_tx141bv3_const.te_delta * 2) {
        if(te_last > te_current) {
            subghz_protocol_blocks_add_bit(&instance->decoder, 1);
        } else {
            subghz_protocol_blocks_add_bit(&instance->decoder, 0);
        }
        ret = true;
    }

    return ret;
}

void ws_protocol_decoder_lacrosse_tx141bv3_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;

    switch(instance->decoder.parser_step) {
    case LaCrosse_TX141BV3DecoderStepReset:
        // Preamble: similar to TX141THBv2, look for repeated long pulses
        if(level && (DURATION_DIFF(
                         duration,
                         ws_protocol_lacrosse_tx141bv3_const.te_short * 4) <
                     ws_protocol_lacrosse_tx141bv3_const.te_delta * 2)) {
            instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepCheckPreambule;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;

    case LaCrosse_TX141BV3DecoderStepCheckPreambule:
        if(level) {
            instance->decoder.te_last = duration;
        } else {
            if((DURATION_DIFF(
                    instance->decoder.te_last,
                    ws_protocol_lacrosse_tx141bv3_const.te_short * 4) <
                ws_protocol_lacrosse_tx141bv3_const.te_delta * 2) &&
               (DURATION_DIFF(
                    duration,
                    ws_protocol_lacrosse_tx141bv3_const.te_short * 4) <
                ws_protocol_lacrosse_tx141bv3_const.te_delta * 2)) {
                // Found preamble chunk
                instance->header_count++;
            } else if(instance->header_count >= 4) {
                if(ws_protocol_decoder_lacrosse_tx141bv3_add_bit(
                       instance, instance->decoder.te_last, duration)) {
                    instance->decoder.decode_data = instance->decoder.decode_data & 1;
                    instance->decoder.decode_count_bit = 1;
                    instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepSaveDuration;
                } else {
                    instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
                }
            } else {
                instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
            }
        }
        break;

    case LaCrosse_TX141BV3DecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepCheckDuration;
        } else {
            instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
        }
        break;

    case LaCrosse_TX141BV3DecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(
                    instance->decoder.te_last,
                    ws_protocol_lacrosse_tx141bv3_const.te_short * 3) <
                ws_protocol_lacrosse_tx141bv3_const.te_delta * 2) &&
               (DURATION_DIFF(
                    duration,
                    ws_protocol_lacrosse_tx141bv3_const.te_short * 4) <
                ws_protocol_lacrosse_tx141bv3_const.te_delta * 2)) {
                // End-of-frame gap
                if((instance->decoder.decode_count_bit ==
                    ws_protocol_lacrosse_tx141bv3_const.min_count_bit_for_found) ||
                   (instance->decoder.decode_count_bit == LACROSSE_TX141_BV3_BIT_COUNT)) {
                    if(ws_protocol_lacrosse_tx141bv3_check_frame(instance)) {
                        instance->generic.data = instance->decoder.decode_data;
                        instance->generic.data_count_bit =
                            instance->decoder.decode_count_bit;

                        ws_protocol_lacrosse_tx141bv3_remote_controller(&instance->generic);

                        if(instance->base.callback)
                            instance->base.callback(&instance->base, instance->base.context);
                    }

                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 0;
                    instance->header_count = 1;
                    instance->decoder.parser_step =
                        LaCrosse_TX141BV3DecoderStepCheckPreambule;
                    break;
                }
            } else if(ws_protocol_decoder_lacrosse_tx141bv3_add_bit(
                          instance, instance->decoder.te_last, duration)) {
                instance->decoder.parser_step =
                    LaCrosse_TX141BV3DecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = LaCrosse_TX141BV3DecoderStepReset;
        }
        break;
    }
}

uint32_t ws_protocol_decoder_lacrosse_tx141bv3_get_hash_data(void* context) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;
    return subghz_protocol_blocks_get_hash_data_long(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus ws_protocol_decoder_lacrosse_tx141bv3_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;
    return ws_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus ws_protocol_decoder_lacrosse_tx141bv3_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;
    return ws_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        ws_protocol_lacrosse_tx141bv3_const.min_count_bit_for_found);
}

void ws_protocol_decoder_lacrosse_tx141bv3_get_string(void* context, FuriString* output) {
    furi_assert(context);
    WSProtocolDecoderLaCrosse_TX141BV3* instance = context;

    furi_string_printf(
        output,
        "%s %dbit\r\n"
        "Key:0x%lX%08lX\r\n"
        "Sn:0x%lX Ch:%d Bat:%d\r\n"
        "Temp:%3.1f C Hum:%d%%",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (uint32_t)(instance->generic.data >> 32),
        (uint32_t)(instance->generic.data),
        instance->generic.id,
        instance->generic.channel,
        instance->generic.battery_low,
        (double)instance->generic.temp,
        instance->generic.humidity);
}
