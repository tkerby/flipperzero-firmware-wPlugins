#include "kia_v1.h"
#include <lib/toolbox/manchester_decoder.h>
#include <lib/toolbox/manchester_encoder.h>

#define TAG "KiaV1"

// OOK PCM 800µs timing
static const SubGhzBlockConst kia_protocol_v1_const = {
    .te_short = 800,
    .te_long = 1600,
    .te_delta = 200,
    .min_count_bit_for_found = 57,
};

#define KIA_V1_TOTAL_BURSTS       3
#define KIA_V1_INTER_BURST_GAP_US 25000
#define KIA_V1_HEADER_PULSES      90

struct SubGhzProtocolDecoderKiaV1 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t header_count;
    ManchesterState manchester_saved_state;
    uint8_t crc;
    bool crc_check;
};

struct SubGhzProtocolEncoderKiaV1 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t serial;
    uint8_t button;
    uint16_t counter;

    bool is_running;
    size_t upload_idx;
    uint8_t current_burst;
    bool sending_gap;
};

typedef enum {
    KiaV1DecoderStepReset = 0,
    KiaV1DecoderStepCheckPreamble,
    KiaV1DecoderStepDecodeData,
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
    .alloc = kia_protocol_encoder_v1_alloc,
    .free = kia_protocol_encoder_v1_free,
    .deserialize = kia_protocol_encoder_v1_deserialize,
    .stop = kia_protocol_encoder_v1_stop,
    .yield = kia_protocol_encoder_v1_yield,
};

const SubGhzProtocol kia_protocol_v1 = {
    .name = KIA_PROTOCOL_V1_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &kia_protocol_v1_decoder,
    .encoder = &kia_protocol_v1_encoder,
};

static uint8_t kia_v1_crc4(const uint8_t* bytes) {
    uint8_t crc = 0;

    for(int i = 0; i < 6; i++) {
        uint8_t b = bytes[i];
        crc ^= ((b & 0x0F) ^ (b >> 4));
    }

    crc = (crc + 1) & 0x0F; // 4-bit CRC
    return crc;
}

static void kia_v1_check_remote_controller(SubGhzProtocolDecoderKiaV1* instance) {
    instance->generic.serial = instance->generic.data >> 24;
    instance->generic.btn = (instance->generic.data >> 16) & 0xFF;
    instance->generic.cnt = ((instance->generic.data >> 4) & 0xF) << 8 |
                            ((instance->generic.data >> 8) & 0xFF);

    uint8_t char_data[6];
    char_data[0] = (instance->generic.serial >> 24) & 0xFF;
    char_data[1] = (instance->generic.serial >> 16) & 0xFF;
    char_data[2] = (instance->generic.serial >> 8) & 0xFF;
    char_data[3] = instance->generic.serial & 0xFF;
    char_data[4] = instance->generic.btn;
    char_data[5] = instance->generic.cnt & 0xFF;

    instance->crc = ((instance->generic.cnt >> 8) & 0xF) << 4 | kia_v1_crc4(char_data);
    instance->crc_check = (kia_v1_crc4(char_data) == (instance->generic.data & 0xF));
}

static void
    kia_v1_extract_fields_from_data(uint64_t data, uint32_t* serial, uint8_t* btn, uint16_t* cnt) {
    *serial = data >> 24;
    *btn = (data >> 16) & 0xFF;
    *cnt = ((data >> 4) & 0xF) << 8 | ((data >> 8) & 0xFF);
}

static uint8_t kia_v1_calculate_crc(uint32_t serial, uint8_t btn, uint16_t cnt) {
    uint8_t char_data[6];
    char_data[0] = (serial >> 24) & 0xFF;
    char_data[1] = (serial >> 16) & 0xFF;
    char_data[2] = (serial >> 8) & 0xFF;
    char_data[3] = serial & 0xFF;
    char_data[4] = btn;
    char_data[5] = cnt & 0xFF;

    return kia_v1_crc4(char_data);
}

static const char* kia_v1_get_button_name(uint8_t btn) {
    const char* name;
    switch(btn) {
    case 0x1:
        name = "CLOSE";
        break;
    case 0x2:
        name = "OPEN";
        break;
    case 0x3:
        name = "TRUNK";
        break;
    default:
        name = "??";
        break;
    }
    return name;
}

void* kia_protocol_encoder_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderKiaV1* instance = malloc(sizeof(SubGhzProtocolEncoderKiaV1));
    instance->base.protocol = &kia_protocol_v1;
    instance->is_running = false;
    instance->serial = 0;
    instance->button = 0;
    instance->counter = 0;
    instance->current_burst = 0;
    instance->sending_gap = false;
    FURI_LOG_I(TAG, "Encoder allocated");
    return instance;
}

void kia_protocol_encoder_v1_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    free(instance);
}

static void kia_protocol_encoder_v1_update_data(SubGhzProtocolEncoderKiaV1* instance) {
    uint64_t data = 0;

    data |= ((uint64_t)instance->serial << 24);

    data |= ((uint64_t)(instance->button & 0xFF) << 16);

    data |= ((uint64_t)(instance->counter & 0xFF) << 8);

    data |= ((uint64_t)((instance->counter >> 8) & 0xF) << 4);

    uint8_t crc = kia_v1_calculate_crc(instance->serial, instance->button, instance->counter);
    data |= (crc & 0xF);

    instance->generic.data = data;
    instance->generic.data_count_bit = kia_protocol_v1_const.min_count_bit_for_found;

    FURI_LOG_I(
        TAG,
        "Data updated - Serial: 0x%08lX, Btn: 0x%02X, Cnt: 0x%03X, CRC: 0x%X",
        instance->serial,
        instance->button,
        instance->counter,
        crc);
    FURI_LOG_I(TAG, "Full data: 0x%014llX", instance->generic.data);
    instance->upload_idx = 0;
    instance->current_burst = 0;
    instance->sending_gap = false;
}

SubGhzProtocolStatus
    kia_protocol_encoder_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;

    instance->is_running = false;
    instance->upload_idx = 0;
    instance->current_burst = 0;
    instance->sending_gap = false;

    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    do {
        FuriString* temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            furi_string_free(temp_str);
            break;
        }

        FURI_LOG_I(TAG, "Protocol: %s", furi_string_get_cstr(temp_str));

        if(!furi_string_equal(temp_str, instance->base.protocol->name)) {
            FURI_LOG_E(
                TAG,
                "Wrong protocol %s != %s",
                furi_string_get_cstr(temp_str),
                instance->base.protocol->name);
            furi_string_free(temp_str);
            break;
        }
        furi_string_free(temp_str);

        uint32_t bit_count_temp;
        if(!flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1)) {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }

        FURI_LOG_I(TAG, "Bit count read: %lu", bit_count_temp);
        instance->generic.data_count_bit = kia_protocol_v1_const.min_count_bit_for_found;

        temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            FURI_LOG_E(TAG, "Missing Key");
            furi_string_free(temp_str);
            break;
        }

        const char* key_str = furi_string_get_cstr(temp_str);
        FURI_LOG_I(TAG, "Key string: %s", key_str);

        uint64_t key = 0;
        size_t str_len = strlen(key_str);

        size_t hex_pos = 0;
        for(size_t i = 0; i < str_len && hex_pos < 16; i++) {
            char c = key_str[i];
            if(c == ' ') continue;

            uint8_t nibble;
            if(c >= '0' && c <= '9') {
                nibble = c - '0';
            } else if(c >= 'A' && c <= 'F') {
                nibble = c - 'A' + 10;
            } else if(c >= 'a' && c <= 'f') {
                nibble = c - 'a' + 10;
            } else {
                FURI_LOG_E(TAG, "Invalid hex character: %c", c);
                furi_string_free(temp_str);
                break;
            }

            key = (key << 4) | nibble;
            hex_pos++;
        }

        furi_string_free(temp_str);

        instance->generic.data = key;
        FURI_LOG_I(TAG, "Parsed key: 0x%014llX", instance->generic.data);

        if(instance->generic.data == 0) {
            FURI_LOG_E(TAG, "Key is zero after parsing!");
            break;
        }

        kia_v1_extract_fields_from_data(
            key, &instance->serial, &instance->button, &instance->counter);

        FURI_LOG_I(
            TAG,
            "Extracted - Serial: 0x%08lX, Btn: 0x%02X, Cnt: 0x%03X",
            instance->serial,
            instance->button,
            instance->counter);

        uint32_t temp;
        if(flipper_format_read_uint32(flipper_format, "Serial", &temp, 1)) {
            instance->serial = temp;
            FURI_LOG_I(TAG, "Read serial from file: 0x%08lX", instance->serial);
        }

        if(flipper_format_read_uint32(flipper_format, "Btn", &temp, 1)) {
            instance->button = (uint8_t)temp;
            FURI_LOG_I(TAG, "Read button from file: 0x%02X", instance->button);
        }

        if(flipper_format_read_uint32(flipper_format, "Cnt", &temp, 1)) {
            instance->counter = (uint16_t)temp;
            FURI_LOG_I(TAG, "Read counter from file: 0x%03X", instance->counter);
        }

        kia_protocol_encoder_v1_update_data(instance);

        instance->is_running = true;
        instance->upload_idx = 0;
        instance->current_burst = 0;
        instance->sending_gap = false;

        FURI_LOG_I(TAG, "Encoder initialized - will send %d bursts", KIA_V1_TOTAL_BURSTS);
        FURI_LOG_I(TAG, "Final data to transmit: 0x%014llX", instance->generic.data);
        res = SubGhzProtocolStatusOk;
    } while(false);

    return res;
}

void kia_protocol_encoder_v1_stop(void* context) {
    if(!context) return;
    SubGhzProtocolEncoderKiaV1* instance = context;

    instance->is_running = false;
    instance->upload_idx = 0;
    instance->current_burst = 0;
    instance->sending_gap = false;

    FURI_LOG_D(TAG, "Encoder stopped and reset");
}

LevelDuration kia_protocol_encoder_v1_yield(void* context) {
    SubGhzProtocolEncoderKiaV1* instance = context;

    if(!instance || !instance->is_running) {
        return level_duration_reset();
    }

    if(instance->sending_gap) {
        instance->sending_gap = false;
        instance->upload_idx = 0;
        FURI_LOG_D(TAG, "Starting burst %d", instance->current_burst + 1);
        return level_duration_make(false, KIA_V1_INTER_BURST_GAP_US);
    }

    LevelDuration result;

    if(instance->upload_idx < KIA_V1_HEADER_PULSES * 2) {
        bool is_high = (instance->upload_idx % 2) == 1;
        result = level_duration_make(is_high, kia_protocol_v1_const.te_long);
    } else if(instance->upload_idx == KIA_V1_HEADER_PULSES * 2) {
        result = level_duration_make(false, kia_protocol_v1_const.te_short);
    } else if(instance->upload_idx < KIA_V1_HEADER_PULSES * 2 + 1 + 56 * 2) {
        uint32_t data_idx = instance->upload_idx - (KIA_V1_HEADER_PULSES * 2 + 1);
        uint32_t bit_num = data_idx / 2;
        bool is_first_half = (data_idx % 2) == 0;

        uint64_t bit_mask = 1ULL << (55 - bit_num);
        bool current_bit = (instance->generic.data & bit_mask) ? true : false;

        if(current_bit) {
            result = level_duration_make(is_first_half, kia_protocol_v1_const.te_short);
        } else {
            result = level_duration_make(!is_first_half, kia_protocol_v1_const.te_short);
        }
    } else {
        instance->current_burst++;

        if(instance->current_burst >= KIA_V1_TOTAL_BURSTS) {
            instance->is_running = false;
            instance->upload_idx = 0;
            instance->current_burst = 0;
            FURI_LOG_I(TAG, "All %d bursts transmitted", KIA_V1_TOTAL_BURSTS);
            return level_duration_reset();
        }

        instance->sending_gap = true;
        instance->upload_idx = 0;
        return level_duration_make(false, KIA_V1_INTER_BURST_GAP_US);
    }

    instance->upload_idx++;
    return result;
}

void kia_protocol_encoder_v1_set_button(void* context, uint8_t button) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->button = button;
    kia_protocol_encoder_v1_update_data(instance);
    FURI_LOG_I(TAG, "Button set to 0x%02X, CRC recalculated", instance->button);
}

void kia_protocol_encoder_v1_set_counter(void* context, uint16_t counter) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->counter = counter & 0xFFF;
    kia_protocol_encoder_v1_update_data(instance);
    FURI_LOG_I(TAG, "Counter set to 0x%03X, CRC recalculated", instance->counter);
}

void kia_protocol_encoder_v1_increment_counter(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->counter = (instance->counter + 1) & 0xFFF;
    kia_protocol_encoder_v1_update_data(instance);
    FURI_LOG_I(TAG, "Counter incremented to 0x%03X, CRC recalculated", instance->counter);
}

uint16_t kia_protocol_encoder_v1_get_counter(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    return instance->counter;
}

uint8_t kia_protocol_encoder_v1_get_button(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    return instance->button;
}

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
}

void kia_protocol_decoder_v1_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    ManchesterEvent event = ManchesterEventReset;

    switch(instance->decoder.parser_step) {
    case KiaV1DecoderStepReset:
        if((level) && (DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                       kia_protocol_v1_const.te_delta)) {
            instance->decoder.parser_step = KiaV1DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            manchester_advance(
                instance->manchester_saved_state,
                ManchesterEventReset,
                &instance->manchester_saved_state,
                NULL);
        }
        break;

    case KiaV1DecoderStepCheckPreamble:
        if(!level) {
            if((DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta) &&
               (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta)) {
                instance->header_count++;
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
            }
        }
        if(instance->header_count > 70) {
            if((!level) &&
               (DURATION_DIFF(duration, kia_protocol_v1_const.te_short) <
                kia_protocol_v1_const.te_delta) &&
               (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta)) {
                instance->decoder.decode_count_bit = 1;
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->header_count = 0;
                instance->decoder.parser_step = KiaV1DecoderStepDecodeData;
            }
        }
        break;

    case KiaV1DecoderStepDecodeData:
        if((DURATION_DIFF(duration, kia_protocol_v1_const.te_short) <
            kia_protocol_v1_const.te_delta)) {
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        } else if((DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                   kia_protocol_v1_const.te_delta)) {
            event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
        }

        if(event != ManchesterEventReset) {
            bool data;
            bool data_ok = manchester_advance(
                instance->manchester_saved_state, event, &instance->manchester_saved_state, &data);
            if(data_ok) {
                instance->decoder.decode_data = (instance->decoder.decode_data << 1) | data;
                instance->decoder.decode_count_bit++;
            }
        }

        if(instance->decoder.decode_count_bit == kia_protocol_v1_const.min_count_bit_for_found) {
            instance->generic.data = instance->decoder.decode_data;
            instance->generic.data_count_bit = instance->decoder.decode_count_bit;
            if(instance->base.callback)
                instance->base.callback(&instance->base, instance->base.context);

            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            instance->decoder.parser_step = KiaV1DecoderStepReset;
        }
        break;
    }
}

uint8_t kia_protocol_decoder_v1_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v1_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    kia_v1_check_remote_controller(instance);

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Preset", furi_string_get_cstr(preset->name)))
            break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Protocol", instance->generic.protocol_name))
            break;

        uint32_t bits = instance->generic.data_count_bit;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &bits, 1)) break;

        char key_str[20];
        snprintf(key_str, sizeof(key_str), "%016llX", instance->generic.data);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key_str)) break;

        if(!flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1))
            break;

        uint32_t temp = instance->generic.btn;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp, 1)) break;

        temp = instance->generic.cnt;
        if(!flipper_format_write_uint32(flipper_format, "Cnt", &temp, 1)) break;

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    kia_protocol_decoder_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    flipper_format_rewind(flipper_format);
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v1_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v1_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    kia_v1_check_remote_controller(instance);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0xFFFFFFFF;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%06lX%08lX\r\n"
        "Serial:%08lX\r\n"
        "Cnt:%03lX CRC:%01X %s\r\n"
        "Btn:%02X:%s\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt,
        instance->crc,
        instance->crc_check ? "OK" : "WRONG",
        instance->generic.btn,
        kia_v1_get_button_name(instance->generic.btn));
}
