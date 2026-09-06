#include "wmbus_selftest_i.h"

#include <string.h>

static bool wmbus_selftest_check_packet_process_mode5_no_key_does_not_decrypt(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(record.application.parser_id != WmBusParserIdShortTpl || record.tpl.decrypted ||
       record.tpl.key_index != 0U || record.application.record_count != 0U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected parser=%s dec=%s idx=%u records=%u",
            wmbus_selftest_parser_name(&record),
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.tpl.key_index,
            (unsigned int)record.application.record_count);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "no_key leaves encrypted payload closed");
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_multiple_keys_uses_matching_slot(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};
    uint32_t total_m3_x1000 = 0U;
    char rec_desc[96] = {0};

    memset(key_store.keys[0], 0xA5, sizeof(key_store.keys[0]));
    key_store.count = 2U;

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5_gold, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    wmbus_selftest_describe_first_record(&record, rec_desc, sizeof(rec_desc));
    if(record.application.parser_id != WmBusParserIdApator162 ||
       !wmbus_selftest_find_total_volume(&record, &total_m3_x1000) || total_m3_x1000 != 345654U ||
       !record.tpl.decrypted || record.tpl.key_index != 1U ||
       strcmp(record.identity.meter_id, "02991056") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "parser=%s total=%lu dec=%s idx=%u id=%s %s",
            wmbus_selftest_parser_name(&record),
            (unsigned long)total_m3_x1000,
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.tpl.key_index,
            record.identity.meter_id,
            rec_desc);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "multiple_keys matched key_index=1");
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_wrong_key_does_not_decrypt(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusCryptoKeyStore key_store = {0};

    memset(key_store.keys[0], 0xA5, sizeof(key_store.keys[0]));
    key_store.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        WmBusPacketRecord record = {0};

        if(!wmbus_selftest_hex_to_format_b_frame(
               vectors[i].telegram, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s format-B build failed", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }

        if(record.application.parser_id != WmBusParserIdShortTpl || record.tpl.decrypted ||
           record.tpl.key_index != 0U || record.application.record_count != 0U) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s dec=%s idx=%u records=%u",
                vectors[i].id,
                wmbus_selftest_parser_name(&record),
                record.tpl.decrypted ? "YES" : "NO",
                (unsigned int)record.tpl.key_index,
                (unsigned int)record.application.record_count);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "wrong_key=MISS no_fallback vectors=%u",
        (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_sections_clear_payload(char* detail, size_t detail_len) {
    uint8_t normalized[WMBUS_APATOR_B_LEN] = {0};
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    WmBusPacketRecord record = {0};

    memcpy(normalized, wmbus_apator_b, sizeof(normalized));
    if(!wmbus_selftest_prepare_plaintext_short_tpl(normalized, sizeof(normalized)) ||
       !wmbus_selftest_frame_build_format_b(
           normalized, sizeof(normalized), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(record.quality != WmBusPacketQualityParsed ||
       record.application.parser_id != WmBusParserIdApator162 ||
       record.application.record_count == 0U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "clear transient parse failed q=%u parser=%u records=%u",
            (unsigned int)record.quality,
            (unsigned int)record.application.parser_id,
            (unsigned int)record.application.record_count);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "clear transient payload parsed records=%u",
        (unsigned int)record.application.record_count);
    return true;
}

static bool
    wmbus_selftest_check_packet_sections_encrypted_payload(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusCryptoKeyStore key_store = {0};
    WmBusPacketRecord record = {0};

    key_store.count = 1U;

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, &key_store, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(!record.tpl.decrypted || record.tpl.key_index != 0U ||
       record.quality != WmBusPacketQualityParsed ||
       record.application.parser_id != WmBusParserIdApator162) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "encrypted transient parse failed dec=%s q=%u parser=%u",
            record.tpl.decrypted ? "YES" : "NO",
            (unsigned int)record.quality,
            (unsigned int)record.application.parser_id);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "encrypted transient payload parsed dec=YES");
    return true;
}

static bool
    wmbus_selftest_check_packet_sections_unsupported_decrypt(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_format_b_frame(
           wmbus_selftest_apator_encrypted_mode5, frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "format-B build failed");
        return false;
    }
    if(frame_len < 15U) {
        wmbus_selftest_set_detail(detail, detail_len, "unexpected normalized len");
        return false;
    }
    frame[14] = 0x88U;

    if(!wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(record.application.record_count != 0U || record.tpl.decrypted) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unsupported decrypt records=%u dec=%s",
            (unsigned int)record.application.record_count,
            record.tpl.decrypted ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "unsupported decrypt exposes no records");
    return true;
}

static bool check_packet_application_ci_scope(char* detail, size_t detail_len) {
    const uint8_t cis[] = {0x78, 0x79, 0x7B, 0x90, 0xA0, 0x7D, 0x7F, 0x72, 0x7A};
    for(size_t i = 0; i < COUNT_OF(cis); i++) {
        uint8_t normalized[32] = {0, 0x44, 0x93, 0x44, 1, 2, 3, 4, 1, 7};
        normalized[10] = cis[i];
        size_t pos = cis[i] == 0x72                                       ? 23U :
                     (cis[i] == 0x7A || cis[i] == 0x7D || cis[i] == 0x7F) ? 15U :
                                                                            11U;
        normalized[pos++] = 0x01;
        normalized[pos++] = 0x13;
        normalized[pos++] = 42;
        normalized[0] = pos - 1U;
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
        size_t len = 0;
        WmBusPacketRecord packet;
        bool expected = cis[i] == 0x72 || cis[i] == 0x7A || cis[i] == 0x78;
        if(!wmbus_selftest_frame_build_format_a(normalized, pos, frame, sizeof(frame), &len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, len, NULL, &packet) ||
           (packet.quality == WmBusPacketQualityParsed) != expected) {
            wmbus_selftest_set_detail(
                detail, detail_len, "CI=%02X expected parsed=%u", cis[i], expected);
            return false;
        }
    }
    wmbus_selftest_set_detail(detail, detail_len, "Only CI72/7A/78 variable data is parsed");
    return true;
}

static bool check_packet_truncated_headers(char* detail, size_t detail_len) {
    const struct {
        uint8_t ci;
        size_t len;
    } headers[] = {
        {0x72, 13},
        {0x7A, 5},
        {0x8C, 3},
        {0x8D, 9},
        {0x8E, 11},
        {0x8F, 17},
    };
    for(size_t i = 0; i < COUNT_OF(headers); i++) {
        for(size_t size = 1; size < headers[i].len; size++) {
            uint8_t normalized[32] = {0, 0x44, 0x93, 0x44, 1, 2, 3, 4, 1, 7};
            normalized[10] = headers[i].ci;
            normalized[11] = 0x01;
            normalized[12] = 0x13;
            normalized[13] = 42;
            size_t normalized_len = 10U + size;
            normalized[0] = normalized_len - 1U;
            uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
            size_t len = 0;
            WmBusPacketRecord packet;
            if(!wmbus_selftest_frame_build_format_a(
                   normalized, normalized_len, frame, sizeof(frame), &len) ||
               !wmbus_selftest_process_phy_frame_record(
                   WmBusRxModeC, WmBusFrameFormatA, frame, len, NULL, &packet) ||
               packet.quality != WmBusPacketQualityCrcOk || packet.application.record_count ||
               packet.tpl.decrypted) {
                wmbus_selftest_set_detail(
                    detail, detail_len, "CI=%02X truncated=%u", headers[i].ci, (unsigned)size);
                return false;
            }
        }
    }
    WmBusPhyFrame oversized = {
        .mode = WmBusRxModeC, .format = WmBusFrameFormatA, .len = WMBUS_PHY_FRAME_MAX_BYTES + 1U};
    WmBusPacketRecord packet;
    if(wmbus_packet_process_phy_frame(&oversized, NULL, &packet)) return false;
    wmbus_selftest_set_detail(detail, detail_len, "Truncated TPL/ELL and oversized PHY rejected");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_packet[] = {
    {"check_packet_application_ci_scope", check_packet_application_ci_scope},
    {"check_packet_truncated_headers", check_packet_truncated_headers},

    {"check_packet_process_mode5_no_key_does_not_decrypt",
     wmbus_selftest_check_packet_process_mode5_no_key_does_not_decrypt},
    {"check_packet_process_mode5_multiple_keys_uses_matching_slot",
     wmbus_selftest_check_packet_process_mode5_multiple_keys_uses_matching_slot},
    {"check_packet_process_mode5_wrong_key_does_not_decrypt",
     wmbus_selftest_check_packet_process_mode5_wrong_key_does_not_decrypt},
    {"check_packet_sections_clear_payload", wmbus_selftest_check_packet_sections_clear_payload},
    {"check_packet_sections_encrypted_payload",
     wmbus_selftest_check_packet_sections_encrypted_payload},
    {"check_packet_sections_unsupported_decrypt",
     wmbus_selftest_check_packet_sections_unsupported_decrypt},
};

const WmBusSelftestCheck* wmbus_selftest_packet_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_packet);
    return wmbus_selftest_checks_packet;
}
