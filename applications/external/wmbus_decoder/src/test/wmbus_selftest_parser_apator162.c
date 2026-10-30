#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"
#include "../protocol/packet/wmbus_packet_security.h"
#include "../protocol/parser/wmbus_parser_apator162.h"

#include <string.h>

static bool wmbus_selftest_check_parser_apator162_public_vectors(char* detail, size_t detail_len) {
    for(size_t i = 0; i < WMBUS_SELFTEST_APATOR_PUBLIC_VECTOR_COUNT; i++) {
        const WmBusSelftestApatorPublicVector* vector = &wmbus_selftest_apator_public_vectors[i];
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        uint8_t roundtrip[WMBUS_SELFTEST_BUF_MAX] = {0};
        WmBusFrameNormalizeResult result = {0};
        WmBusPacketRecord record = {0};
        char id[WMBUS_ID_STR_LEN] = {0};
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_bytes(
               vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }
        if(normalized_len != vector->parsed_len) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s unexpected len=%u",
                vector->id,
                (unsigned int)normalized_len);
            return false;
        }
        if(wmbus_selftest_fnv1a32(normalized, normalized_len) != vector->parsed_fnv1a) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s fingerprint mismatch", vector->id);
            return false;
        }
        if(normalized[0] != (uint8_t)(normalized_len - 1U) ||
           !wmbus_decode_is_plausible_frame(normalized, normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s plausibility failed", vector->id);
            return false;
        }
        if(!wmbus_selftest_prepare_plaintext_short_tpl(normalized, normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s is not plaintext TPL", vector->id);
            return false;
        }
        if(!wmbus_selftest_frame_build_format_b(
               normalized, normalized_len, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s format-B build failed", vector->id);
            return false;
        }
        if(!wmbus_frame_normalize(
               WmBusFrameFormatB, frame, frame_len, roundtrip, sizeof(roundtrip), &result)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s normalize failed", vector->id);
            return false;
        }
        if(!result.crc_ok || result.normalized_len != normalized_len ||
           memcmp(normalized, roundtrip, normalized_len) != 0) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s roundtrip failed", vector->id);
            return false;
        }
        wmbus_frame_format_id(&roundtrip[4], id);
        if(strncmp(vector->id, id, WMBUS_ID_STR_LEN) != 0) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector expected id=%s got=%s", vector->id, id);
            return false;
        }
        if(!wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vector->total_m3_x1000 ||
           strcmp(record.identity.meter_id, vector->id) != 0 || record.tpl.decrypted ||
           record.tpl.key_index != 0U) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu expected=%lu id=%s enc=%s dec=%s idx=%u %s",
                vector->id,
                wmbus_selftest_parser_name(&record),
                (unsigned long)total_m3_x1000,
                (unsigned long)vector->total_m3_x1000,
                record.identity.meter_id,
                (record.tpl.header_type != WmBusTplHeaderNone &&
                 wmbus_packet_tpl_payload_is_protected(record.tpl.cfg)) ?
                    "YES" :
                    "NO",
                record.tpl.decrypted ? "YES" : "NO",
                (unsigned int)record.tpl.key_index,
                rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "vectors=%u", (unsigned int)WMBUS_SELFTEST_APATOR_PUBLIC_VECTOR_COUNT);
    return true;
}

static bool wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected(
    char* detail,
    size_t detail_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0;
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(
           wmbus_selftest_apator_old_style_b6, normalized, sizeof(normalized), &normalized_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }
    if(normalized_len != wmbus_selftest_apator_old_style_b6_len) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected len=%u", (unsigned int)normalized_len);
        return false;
    }
    if(wmbus_selftest_fnv1a32(normalized, normalized_len) !=
       wmbus_selftest_apator_old_style_b6_fnv1a) {
        wmbus_selftest_set_detail(detail, detail_len, "fingerprint mismatch");
        return false;
    }
    if(normalized_len <= 10U || normalized[10] != 0xB6U) {
        wmbus_selftest_set_detail(detail, detail_len, "expected CI=B6");
        return false;
    }
    if(!wmbus_selftest_frame_build_format_b(
           normalized, normalized_len, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "old-style CI=B6 accepted parser=%s total=%lu",
            wmbus_selftest_parser_name(&record),
            (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "ci=B6 rejected=YES");
    return true;
}

static bool wmbus_selftest_check_parser_apator162_mode5_configured_zero_key_vectors(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5, 4848U, "88888888"},
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    key_store.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};
        uint16_t cfg = 0;
        char rec_desc[96] = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(
               vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(frame_len < 17U || frame[10] != 0x7AU) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s unexpected frame", vectors[i].id);
            return false;
        }
        cfg = (uint16_t)frame[13] | ((uint16_t)frame[14] << 8);
        if(!wmbus_packet_tpl_payload_is_protected(cfg) ||
           (frame[15] == 0x2FU && frame[16] == 0x2FU)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s unexpected cipher state", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        uint32_t total_m3_x1000 = 0U;
        wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
        if(record.application.parser_id != WmBusParserIdApator162 ||
           !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
           total_m3_x1000 != vectors[i].total_m3_x1000 || !record.tpl.decrypted ||
           record.tpl.key_index != 0U || strcmp(record.identity.meter_id, vectors[i].id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu dec=%s idx=%u id=%s %s",
                vectors[i].id,
                wmbus_selftest_parser_name(&record),
                (unsigned long)total_m3_x1000,
                record.tpl.decrypted ? "YES" : "NO",
                (unsigned int)record.tpl.key_index,
                record.identity.meter_id,
                rec_desc);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "mode5 configured-zero vectors=%u key_index=0",
        (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5_corrupt, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000) || record.tpl.decrypted) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "corrupt ciphertext accepted parser=%s total=%lu dec=%s",
            wmbus_selftest_parser_name(&record),
            (unsigned long)total_m3_x1000,
            record.tpl.decrypted ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode5 corrupt rejected=YES");
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_payload_without_total(char* detail, size_t detail_len) {
    uint8_t normalized[WMBUS_APATOR_B_LEN] = {0};
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;

    memcpy(normalized, wmbus_apator_b, sizeof(normalized));
    normalized[28] = 0xFFU;
    if(!wmbus_selftest_prepare_plaintext_short_tpl(normalized, sizeof(normalized))) {
        wmbus_selftest_set_detail(detail, detail_len, "plaintext fixture setup failed");
        return false;
    }

    if(!wmbus_selftest_frame_build_format_b(
           normalized, sizeof(normalized), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "payload unexpectedly parsed total=%lu",
            (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "payload total=NO");
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_payload_total_a1(char* detail, size_t detail_len) {
    uint8_t normalized[WMBUS_APATOR_B_LEN] = {0};
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;

    memcpy(normalized, wmbus_apator_b, sizeof(normalized));
    normalized[28] = 0xA1U;
    normalized[29] = 0x78U;
    normalized[30] = 0x56U;
    normalized[31] = 0x34U;
    normalized[32] = 0x12U;
    if(!wmbus_selftest_prepare_plaintext_short_tpl(normalized, sizeof(normalized))) {
        wmbus_selftest_set_detail(detail, detail_len, "plaintext fixture setup failed");
        return false;
    }

    if(!wmbus_selftest_frame_build_format_b(
           normalized, sizeof(normalized), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id != WmBusParserIdApator162 ||
       !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) ||
       total_m3_x1000 != 0x12345678U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "a1 total parse failed total=%lu parser=%s",
            (unsigned long)total_m3_x1000,
            wmbus_selftest_parser_name(&record));
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "payload a1 total=%lu", (unsigned long)total_m3_x1000);
    return true;
}

static bool wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed(
    char* detail,
    size_t detail_len) {
    uint8_t normalized[WMBUS_APATOR_B_LEN] = {0};
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    memcpy(normalized, wmbus_apator_b, sizeof(normalized));
    normalized[25] = 0xFEU;
    if(!wmbus_selftest_prepare_plaintext_short_tpl(normalized, sizeof(normalized))) {
        wmbus_selftest_set_detail(detail, detail_len, "plaintext fixture setup failed");
        return false;
    }

    if(!wmbus_selftest_frame_build_format_b(
           normalized, sizeof(normalized), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    if(record.application.parser_id == WmBusParserIdApator162 ||
       wmbus_selftest_find_total_volume(&record, &total_m3_x1000)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "invalid payload accepted parser=%s total=%lu",
            wmbus_selftest_parser_name(&record),
            (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "invalid payload rejected=YES");
    return true;
}

static bool wmbus_selftest_check_apator162_register_sizes(char* detail, size_t detail_len) {
    if(wmbus_parser_apator162_register_size(0x10U) != 4 ||
       wmbus_parser_apator162_register_size(0x71U) != 9 ||
       wmbus_parser_apator162_register_size(0x7BU) != 49 ||
       wmbus_parser_apator162_register_size(0xA1U) != 4 ||
       wmbus_parser_apator162_register_size(0xB2U) != 16 ||
       wmbus_parser_apator162_register_size(0xB6U) != 3 ||
       wmbus_parser_apator162_register_size(0xC7U) != 3 ||
       wmbus_parser_apator162_register_size(0xC8U) != -1 ||
       wmbus_parser_apator162_register_size(0xFEU) != -1) {
        wmbus_selftest_set_detail(detail, detail_len, "register size table mismatch");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "fixed and ranged register sizes valid");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_parser_apator162[] = {
    {"check_parser_apator162_public_vectors",
     wmbus_selftest_check_parser_apator162_public_vectors},
    {"check_parser_apator162_old_style_ci_b6_rejected",
     wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected},
    {"check_parser_apator162_mode5_configured_zero_key_vectors",
     wmbus_selftest_check_parser_apator162_mode5_configured_zero_key_vectors},
    {"check_parser_apator162_payload_without_total",
     wmbus_selftest_check_parser_apator162_payload_without_total},
    {"check_parser_apator162_payload_total_a1",
     wmbus_selftest_check_parser_apator162_payload_total_a1},
    {"check_parser_apator162_invalid_payload_not_claimed",
     wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed},
    {"check_parser_apator162_mode5_corrupt_rejected",
     wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected},
    {"check_apator162_register_sizes", wmbus_selftest_check_apator162_register_sizes},
};

const WmBusSelftestCheck* wmbus_selftest_parser_apator162_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_parser_apator162);
    return wmbus_selftest_checks_parser_apator162;
}
