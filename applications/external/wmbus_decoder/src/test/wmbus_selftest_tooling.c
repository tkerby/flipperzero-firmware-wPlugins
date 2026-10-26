#include "wmbus_selftest_i.h"

#include "../protocol/decode/wmbus_decode.h"
#include "../protocol/format/wmbus_packet_formatter.h"
#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/format/wmbus_packet_summary.h"
#include "../protocol/model/wmbus_application_record.h"
#include "../protocol/packet/wmbus_packet_decode.h"
#include "../protocol/packet/wmbus_packet_security.h"

#include <stdio.h>
#include <string.h>

static bool wmbus_selftest_check_3of6_valid_single_byte(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58, 0xD0};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(!wmbus_decode_3of6(raw, sizeof(raw) * 8U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed");
        return false;
    }
    if(out_len != 1U || out[0] != 0x01U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected out_len=%u out0=%02X", (unsigned int)out_len, out[0]);
        return false;
    }
    out_len = 1U;
    if(wmbus_decode_3of6(raw, 12U, out, 0U, &out_len) || out_len != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "short output accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "decoded=01 len=1");
    return true;
}

static bool wmbus_selftest_check_supported_l_field_range(char* detail, size_t detail_len) {
    const size_t max_wire_len = wmbus_frame_len_format_a(WMBUS_FRAME_L_FIELD_MAX);
    const size_t max_t_raw_len = (max_wire_len * 12U + 7U) / 8U;
    const size_t format_b_normalized_len = WMBUS_FRAME_L_FIELD_MAX - 3U;
    uint8_t format_b_normalized[WMBUS_PHY_FRAME_MAX_BYTES] = {0};
    uint8_t format_b_wire[WMBUS_PHY_FRAME_MAX_BYTES] = {0};
    uint8_t format_b_roundtrip[WMBUS_PHY_FRAME_MAX_BYTES] = {0};
    const uint8_t invalid_format_b_fifo[] = {0x54U, 0x3DU, 128U};
    size_t format_b_wire_len = 0U;
    size_t invalid_fifo_len = 0U;
    WmBusFrameFormat invalid_fifo_format = WmBusFrameFormatUnknown;
    WmBusFrameNormalizeResult format_b_result = {0};

    format_b_normalized[0] = (uint8_t)(format_b_normalized_len - 1U);

    if(!wmbus_frame_l_field_valid(WMBUS_FRAME_L_FIELD_MIN) ||
       !wmbus_frame_l_field_valid(WMBUS_FRAME_L_FIELD_MAX) ||
       wmbus_frame_l_field_valid(WMBUS_FRAME_L_FIELD_MAX + 1U) ||
       !wmbus_frame_l_field_valid_for_format(127U, WmBusFrameFormatB) ||
       !wmbus_frame_l_field_valid_for_format(130U, WmBusFrameFormatB) ||
       wmbus_frame_l_field_valid_for_format(128U, WmBusFrameFormatB) ||
       wmbus_frame_l_field_valid_for_format(129U, WmBusFrameFormatB) ||
       wmbus_frame_expected_len(128U, WmBusFrameFormatB) != 0U ||
       wmbus_frame_expected_len(129U, WmBusFrameFormatB) != 0U ||
       wmbus_fifo_frame_length(
           WmBusRxModeC,
           invalid_format_b_fifo,
           sizeof(invalid_format_b_fifo),
           &invalid_fifo_len,
           &invalid_fifo_format) != WmBusCaptureLengthInvalid ||
       max_t_raw_len > WMBUS_PHY_FRAME_MAX_BYTES ||
       !wmbus_selftest_frame_build_format_b(
           format_b_normalized,
           format_b_normalized_len,
           format_b_wire,
           sizeof(format_b_wire),
           &format_b_wire_len) ||
       format_b_wire_len != (size_t)WMBUS_FRAME_L_FIELD_MAX + 1U ||
       !wmbus_frame_normalize(
           WmBusFrameFormatB,
           format_b_wire,
           format_b_wire_len,
           format_b_roundtrip,
           sizeof(format_b_roundtrip),
           &format_b_result) ||
       format_b_result.normalized_len != format_b_normalized_len ||
       memcmp(format_b_normalized, format_b_roundtrip, format_b_normalized_len) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "range=%u..%u wire=%u raw=%u invalid",
            (unsigned int)WMBUS_FRAME_L_FIELD_MIN,
            (unsigned int)WMBUS_FRAME_L_FIELD_MAX,
            (unsigned int)max_wire_len,
            (unsigned int)max_t_raw_len);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "range=%u..%u max_wire=%u max_t_raw=%u",
        (unsigned int)WMBUS_FRAME_L_FIELD_MIN,
        (unsigned int)WMBUS_FRAME_L_FIELD_MAX,
        (unsigned int)max_wire_len,
        (unsigned int)max_t_raw_len);
    return true;
}

static bool wmbus_selftest_check_3of6_reject_dangling_nibble(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_decode_3of6(raw, sizeof(raw) * 8U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "dangling nibble accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_3of6_reject_invalid_symbol(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x00};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_decode_3of6(raw, sizeof(raw) * 8U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid symbol accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_parser_plausibility(char* detail, size_t detail_len) {
    const uint8_t valid[] = {10, 0x44, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_c[] = {10, 0x45, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_mfg[] = {10, 0x44, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0};

    if(!wmbus_decode_is_plausible_frame(valid, sizeof(valid))) {
        wmbus_selftest_set_detail(detail, detail_len, "valid frame rejected");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(bad_c, sizeof(bad_c))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad C-field accepted");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(bad_mfg, sizeof(bad_mfg))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad manufacturer accepted");
        return false;
    }
    if(wmbus_decode_is_plausible_frame(valid, 10U)) {
        wmbus_selftest_set_detail(detail, detail_len, "too-short frame accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "valid=YES bad_c=NO bad_mfg=NO short=NO");
    return true;
}

static bool wmbus_selftest_check_tpl_security_modes(char* detail, size_t detail_len) {
    uint8_t clear_mode = wmbus_packet_tpl_security_mode(0x0000U);
    uint8_t aes_cbc_iv_mode = wmbus_packet_tpl_security_mode(0x8560U);
    uint8_t mfct_mode = wmbus_packet_tpl_security_mode(0x0100U);
    uint8_t aes_ctr_cmac_mode = wmbus_packet_tpl_security_mode(0x0800U);

    if(clear_mode != 0U || aes_cbc_iv_mode != 5U || mfct_mode != 1U || aes_ctr_cmac_mode != 8U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected modes clear=%u aes=%u mfct=%u ctr=%u",
            (unsigned int)clear_mode,
            (unsigned int)aes_cbc_iv_mode,
            (unsigned int)mfct_mode,
            (unsigned int)aes_ctr_cmac_mode);
        return false;
    }

    if(wmbus_packet_tpl_payload_is_protected(0x0000U) ||
       wmbus_packet_tpl_payload_is_protected(0x0500U) ||
       !wmbus_packet_tpl_payload_is_protected(0x8560U) ||
       !wmbus_packet_tpl_payload_is_protected(0x0100U) ||
       !wmbus_packet_tpl_payload_is_protected(0x0800U)) {
        wmbus_selftest_set_detail(detail, detail_len, "encrypted-payload policy mismatch");
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "clear=%u aes=%u ctr=%u",
        (unsigned int)clear_mode,
        (unsigned int)aes_cbc_iv_mode,
        (unsigned int)aes_ctr_cmac_mode);
    return true;
}

static bool wmbus_selftest_check_ci72_protected_payload(char* detail, size_t detail_len) {
    static const char telegram[] =
        "6644496a745215500a3755507202198370496a000723005005858daa383Bf87f14cfed9f6B64f618"
        "6963829322fcc6e85739dBf1f851558d5e19B395d4f028cf832Bd26f7961fdBc0dcB61Bc7567B53579"
        "BBd362421B09f58c25029fBfdf6fB14a2B05a54c67ad4c0BB2ee1e3caa388c0Bf80c63c2";
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(telegram, frame, sizeof(frame), &frame_len) ||
       frame_len != 117U ||
       !wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &record)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "CI72 fixture process failed len=%u", (unsigned int)frame_len);
        return false;
    }

    if(record.quality != WmBusPacketQualityCrcOk || record.dll.ci_field != 0x72U ||
       record.tpl.header_type != WmBusTplHeaderLong || record.tpl.header_len != 23U ||
       record.tpl.acc != 0x23U || record.tpl.cfg != 0x0550U || record.tpl.security_mode != 0x05U ||
       !wmbus_packet_tpl_payload_is_protected(record.tpl.cfg) ||
       strcmp(record.identity.manufacturer, "ZRI") != 0 ||
       strcmp(record.identity.meter_id, "70831902") != 0 || record.identity.version != 0x00U ||
       record.identity.dev_type != 0x07U || record.application.record_count != 0U ||
       record.application.parser_id != WmBusParserIdLongTpl) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "q=%u ci=%02X tpl=%u cfg=%04X sec=%02X id=%s parser=%u",
            (unsigned int)record.quality,
            record.dll.ci_field,
            (unsigned int)record.tpl.header_type,
            record.tpl.cfg,
            record.tpl.security_mode,
            record.identity.meter_id,
            (unsigned int)record.application.parser_id);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "CI72 long TPL Mode5 payload closed");
    return true;
}

static bool wmbus_selftest_check_ci72_mode5_decrypt(char* detail, size_t detail_len) {
    static const char telegram[] =
        "6644496a745215500a3755507202198370496a000723005005858daa383Bf87f14cfed9f6B64f618"
        "6963829322fcc6e85739dBf1f851558d5e19B395d4f028cf832Bd26f7961fdBc0dcB61Bc7567B53579"
        "BBd362421B09f58c25029fBfdf6fB14a2B05a54c67ad4c0BB2ee1e3caa388c0Bf80c63c2";
    static const char ciphertext[] =
        "de7f2806121da08b5e60f02c4f177c7ea5e0ba4850178dc3b64cd9ad11484fbcc50487a978a8479b"
        "df51c12ada49371f18b340d386e99b65cd43b6de3f472b49a6c0b556950dad0cbbb0499200a2d791";
    uint8_t source[WMBUS_SELFTEST_BUF_MAX] = {0};
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    uint8_t encrypted[80] = {0};
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t source_len = 0U;
    size_t encrypted_len = 0U;
    size_t frame_len = 0U;
    WmBusFrameNormalizeResult normalize = {0};
    WmBusCryptoKeyStore key_store = {.count = 1U};
    static const uint8_t encrypted_blocks[] = {5U, 15U};

    if(!wmbus_selftest_hex_to_bytes(telegram, source, sizeof(source), &source_len) ||
       !wmbus_frame_normalize(
           WmBusFrameFormatA, source, source_len, normalized, sizeof(normalized), &normalize) ||
       normalize.normalized_len != 103U || !normalize.crc_ok ||
       !wmbus_selftest_hex_to_bytes(ciphertext, encrypted, sizeof(encrypted), &encrypted_len) ||
       encrypted_len != sizeof(encrypted)) {
        wmbus_selftest_set_detail(detail, detail_len, "CI72 fixture preparation failed");
        return false;
    }

    for(size_t i = 0U; i < COUNT_OF(encrypted_blocks); i++) {
        WmBusPacketRecord record = {0};

        normalized[21] = (uint8_t)((normalized[21] & 0x0FU) | (encrypted_blocks[i] << 4U));
        memcpy(&normalized[23], encrypted, sizeof(encrypted));
        if(!wmbus_selftest_frame_build_format_a(
               normalized, normalize.normalized_len, frame, sizeof(frame), &frame_len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, &key_store, &record)) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "CI72 N=%u fixture process failed",
                (unsigned int)encrypted_blocks[i]);
            return false;
        }

        if(record.tpl.header_type != WmBusTplHeaderLong || !record.tpl.decrypted ||
           record.tpl.key_index != 0U) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "CI72 N=%u type=%u dec=%s",
                (unsigned int)encrypted_blocks[i],
                (unsigned int)record.tpl.header_type,
                record.tpl.decrypted ? "YES" : "NO");
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "CI72 long TPL Mode5 N=5,15 decrypted=YES");
    return true;
}

static bool wmbus_selftest_check_ell_chained_tpl_headers(char* detail, size_t detail_len) {
    static const uint8_t no_tpl_header[] = {
        20U,   0x44U, 0x49U, 0x6AU, 0x74U, 0x52U, 0x15U, 0x50U, 0x0AU, 0x37U, 0x8DU,
        0x00U, 0x23U, 0x67U, 0x45U, 0x23U, 0x01U, 0x00U, 0x00U, 0x78U, 0x2FU,
    };
    static const uint8_t short_tpl[] = {
        25U,   0x44U, 0x49U, 0x6AU, 0x74U, 0x52U, 0x15U, 0x50U, 0x0AU, 0x37U, 0x8DU, 0x00U, 0x23U,
        0x67U, 0x45U, 0x23U, 0x01U, 0x00U, 0x00U, 0x7AU, 0x24U, 0x00U, 0x00U, 0x00U, 0x2FU, 0x2FU,
    };
    static const uint8_t long_tpl[] = {
        33U,   0x44U, 0x49U, 0x6AU, 0x74U, 0x52U, 0x15U, 0x50U, 0x0AU, 0x37U, 0x8DU, 0x00U,
        0x23U, 0x67U, 0x45U, 0x23U, 0x01U, 0x00U, 0x00U, 0x72U, 0x02U, 0x19U, 0x83U, 0x70U,
        0x49U, 0x6AU, 0x00U, 0x07U, 0x24U, 0x00U, 0x00U, 0x05U, 0x2FU, 0x2FU,
    };
    const struct {
        const uint8_t* normalized;
        size_t normalized_len;
        WmBusTplHeaderType header_type;
        uint8_t tpl_ci;
        uint8_t header_len;
        uint8_t tpl_acc;
        const char* meter_id;
        uint8_t dev_type;
    } cases[] = {
        {no_tpl_header,
         sizeof(no_tpl_header),
         WmBusTplHeaderNone,
         0x78U,
         20U,
         0x00U,
         "50155274",
         0x37U},
        {short_tpl, sizeof(short_tpl), WmBusTplHeaderShort, 0x7AU, 24U, 0x24U, "50155274", 0x37U},
        {long_tpl, sizeof(long_tpl), WmBusTplHeaderLong, 0x72U, 32U, 0x24U, "70831902", 0x07U},
    };
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;

    for(size_t i = 0U; i < COUNT_OF(cases); i++) {
        WmBusPacketRecord record = {0};
        if(!wmbus_selftest_frame_build_format_a(
               cases[i].normalized, cases[i].normalized_len, frame, sizeof(frame), &frame_len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "case=%u process failed", (unsigned int)i);
            return false;
        }

        if(!record.ell.has_ell || !record.ell.has_session || record.ell.header_len != 19U ||
           record.dll.ci_field != 0x8DU || record.tpl.ci_field != cases[i].tpl_ci ||
           record.tpl.header_type != cases[i].header_type ||
           record.tpl.header_len != cases[i].header_len || record.tpl.acc != cases[i].tpl_acc ||
           strcmp(record.identity.manufacturer, "ZRI") != 0 ||
           strcmp(record.identity.meter_id, cases[i].meter_id) != 0 ||
           record.identity.dev_type != cases[i].dev_type) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "case=%u ell=%u/%u ci=%02X/%02X tpl=%u/%u acc=%02X id=%s type=%02X",
                (unsigned int)i,
                record.ell.has_ell ? 1U : 0U,
                record.ell.header_len,
                record.dll.ci_field,
                record.tpl.ci_field,
                (unsigned int)record.tpl.header_type,
                record.tpl.header_len,
                record.tpl.acc,
                record.identity.meter_id,
                record.identity.dev_type);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "ELL effective CI and TPL headers decoded");
    return true;
}

static bool wmbus_selftest_check_clear_ell_uses_tpl_security(char* detail, size_t detail_len) {
    WmBusPacketEllData ell = {
        .has_ell = true,
        .has_session = true,
        .security_mode = 0U,
    };
    WmBusPacketTplData tpl = {
        .header_type = WmBusTplHeaderLong,
        .cfg = 0x8560U,
        .security_mode = 0x05U,
        .decrypted = true,
        .key_index = 2U,
    };
    char crypto[16] = {0};
    char security[48] = {0};

    wmbus_packet_summary_format_crypto_tag(&ell, &tpl, crypto, sizeof(crypto));
    wmbus_packet_summary_format_security_text(&ell, &tpl, security, sizeof(security));
    if(strcmp(crypto, "DEC#2") != 0 || strcmp(security, "AES-CBC IV, decrypted key #2") != 0 ||
       wmbus_packet_summary_security_mode(&ell, &tpl) != 0x05U ||
       wmbus_packet_summary_key_index(&ell, &tpl) != 2U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "clear ELL tag=%s text=%s mode=%02X key=%u",
            crypto,
            security,
            wmbus_packet_summary_security_mode(&ell, &tpl),
            (unsigned int)wmbus_packet_summary_key_index(&ell, &tpl));
        return false;
    }

    ell.security_mode = 0x01U;
    wmbus_packet_summary_format_crypto_tag(&ell, &tpl, crypto, sizeof(crypto));
    if(strcmp(crypto, "EENC") != 0 || wmbus_packet_summary_security_mode(&ell, &tpl) != 0x01U ||
       wmbus_packet_summary_key_index(&ell, &tpl) != 0U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "protected ELL tag=%s mode=%02X key=%u",
            crypto,
            wmbus_packet_summary_security_mode(&ell, &tpl),
            (unsigned int)wmbus_packet_summary_key_index(&ell, &tpl));
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "clear ELL reports nested TPL security");
    return true;
}

static bool wmbus_selftest_check_long_tpl_ci_headers(char* detail, size_t detail_len) {
    static const uint8_t long_cis[] = {
        0x53U, 0x55U, 0x5BU, 0x5FU, 0x60U, 0x64U, 0x68U, 0x6CU, 0x6DU, 0x6FU, 0x72U, 0x75U,
        0x7CU, 0x7EU, 0x80U, 0x82U, 0x87U, 0x8BU, 0x9FU, 0xC0U, 0xC2U, 0xC3U, 0xC5U,
    };
    uint8_t normalized[] = {
        24U,   0x44U, 0x49U, 0x6AU, 0x74U, 0x52U, 0x15U, 0x50U, 0x0AU, 0x37U, 0U,    0x02U, 0x19U,
        0x83U, 0x70U, 0x49U, 0x6AU, 0x00U, 0x07U, 0x23U, 0x00U, 0x00U, 0x05U, 0x2FU, 0x2FU,
    };
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;

    for(size_t i = 0U; i < COUNT_OF(long_cis); i++) {
        WmBusPacketRecord record = {0};
        normalized[10] = long_cis[i];

        if(!wmbus_selftest_frame_build_format_a(
               normalized, sizeof(normalized), frame, sizeof(frame), &frame_len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "CI=%02X frame process failed", long_cis[i]);
            return false;
        }
        if(record.tpl.header_type != WmBusTplHeaderLong || record.tpl.header_len != 23U ||
           record.tpl.cfg != 0x0500U || record.tpl.decrypted ||
           strcmp(record.identity.manufacturer, "ZRI") != 0 ||
           strcmp(record.identity.meter_id, "70831902") != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "CI=%02X type=%u cfg=%04X dec=%u id=%s",
                long_cis[i],
                (unsigned int)record.tpl.header_type,
                record.tpl.cfg,
                record.tpl.decrypted ? 1U : 0U,
                record.identity.meter_id);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "long CIs=%u Mode5 N=0 payload clear",
        (unsigned int)COUNT_OF(long_cis));
    return true;
}

static bool wmbus_selftest_check_ell_security_modes(char* detail, size_t detail_len) {
    uint32_t clear_sn = 0x01234567UL;
    uint32_t aes_ctr_sn = 0x21234567UL;

    if(!wmbus_packet_ci_has_ell(0x8DU) || !wmbus_packet_ci_has_ell(0x8EU) ||
       !wmbus_packet_ci_has_ell(0x8FU) || wmbus_packet_ci_has_ell(0x7AU)) {
        wmbus_selftest_set_detail(detail, detail_len, "ci detection mismatch");
        return false;
    }

    if(!wmbus_packet_ell_has_session_fields(0x8DU) || wmbus_packet_ell_has_session_fields(0x8EU) ||
       !wmbus_packet_ell_has_session_fields(0x8FU)) {
        wmbus_selftest_set_detail(detail, detail_len, "session-field detection mismatch");
        return false;
    }

    const uint8_t clear_mode = wmbus_packet_ell_security_mode(clear_sn);
    const uint8_t aes_ctr_mode = wmbus_packet_ell_security_mode(aes_ctr_sn);
    if(clear_mode != WmBusEllSecurityModeClear || aes_ctr_mode != WmBusEllSecurityModeAesCtr ||
       wmbus_packet_ell_payload_is_encrypted(clear_mode) ||
       !wmbus_packet_ell_payload_is_encrypted(aes_ctr_mode)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "security heuristic mismatch clear=%u aes=%u",
            (unsigned int)clear_mode,
            (unsigned int)aes_ctr_mode);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "8D/8F session AES-CTR mode=%u", (unsigned int)aes_ctr_mode);
    return true;
}

static bool wmbus_selftest_check_format_fields_text_uses_context(char* detail, size_t detail_len) {
    WmBusPacketApplicationData application = {0};
    WmBusApplicationRecord* record = NULL;
    FuriString* fields = furi_string_alloc();

    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "fields alloc failed");
        return false;
    }

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append volume primary failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->scale10 = -3;
    record->data_len = 4U;
    wmbus_application_record_set_value(record, 150);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append volume history failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->storage_no = 1U;
    record->scale10 = -5;
    record->data_len = 6U;
    wmbus_application_record_set_value(record, INT64_C(139587760654128));

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append energy primary failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityEnergy;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->scale10 = -2;
    record->data_len = 2U;
    wmbus_application_record_set_value(record, 2);

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append energy tariff failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityEnergy;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->tariff = 1U;
    record->scale10 = 0;
    record->data_len = 6U;
    wmbus_application_record_set_value(record, INT64_C(468274118951));

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append status failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityStatus;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    if(!wmbus_application_record_set_raw_bytes(record, (const uint8_t[]){0x92, 0x01}, 2U)) {
        wmbus_selftest_set_detail(detail, detail_len, "status raw encode failed");
        furi_string_free(fields);
        return false;
    }
    if(record->data_len != 2U) {
        wmbus_selftest_set_detail(detail, detail_len, "raw setter lost data_len");
        furi_string_free(fields);
        return false;
    }

    if(!wmbus_application_record_append(&application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append maximum context failed");
        furi_string_free(fields);
        return false;
    }
    record->quantity = WmBusApplicationQuantityEnergy;
    record->measurement_type = WmBusApplicationMeasurementTypeMaximum;
    record->storage_no = 2U;
    record->tariff = 3U;
    record->subunit = 4U;
    record->data_len = 1U;
    wmbus_application_record_set_value(record, 12);

    wmbus_record_formatter_format_joined(
        application.records, application.record_count, ';', fields);

    if(strcmp(
           furi_string_get_cstr(fields),
           "Volume=0.150 m3;Energy=0.02 Wh;Energy[T1]=468274118951 Wh;Status=9201;"
           "Volume[S1]=1395877606.54128 m3;Energy[S2 T3 U4 max]=12 Wh") != 0) {
        wmbus_selftest_set_detail(detail, detail_len, "fields=%s", furi_string_get_cstr(fields));
        furi_string_free(fields);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "fields=%s", furi_string_get_cstr(fields));
    furi_string_free(fields);
    return true;
}

static bool wmbus_selftest_check_packet_detail_retains_volume(char* detail, size_t detail_len) {
    WmBusPacketRecord packet = {
        .quality = WmBusPacketQualityParsed,
        .mode = WmBusRxModeC,
        .format = WmBusFrameFormatA,
        .rssi = -72,
        .identity.dev_type = 0x07,
        .dll.ci_field = 0x7A,
        .tpl.ci_field = 0x7A,
        .application.parser_id = WmBusParserIdDifVif,
    };
    snprintf(packet.identity.manufacturer, sizeof(packet.identity.manufacturer), "TST");
    snprintf(packet.identity.meter_id, sizeof(packet.identity.meter_id), "12345678");

    WmBusApplicationRecord* record = NULL;
    if(!wmbus_application_record_append(&packet.application, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "append volume failed");
        return false;
    }
    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->scale10 = -3;
    record->data_len = 4U;
    wmbus_application_record_set_value(record, 150);

    FuriString* text = furi_string_alloc();
    if(!text) {
        wmbus_selftest_set_detail(detail, detail_len, "detail alloc failed");
        return false;
    }

    wmbus_packet_format_detail_text(&packet, text);
    const char* detail_text = furi_string_get_cstr(text);
    if(strncmp(
           detail_text,
           "Radio mode: C, format: A\nRSSI: -72 dBm\n",
           sizeof("Radio mode: C, format: A\nRSSI: -72 dBm\n") - 1U) != 0 ||
       !strstr(detail_text, "\nCI field: 7A\n") || !strstr(detail_text, "\nVolume=0.150 m3")) {
        wmbus_selftest_set_detail(detail, detail_len, "detail=%s", detail_text);
        furi_string_free(text);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "detail volume retained=YES");
    furi_string_free(text);
    return true;
}

static bool wmbus_selftest_check_packet_quality_policy(char* detail, size_t detail_len) {
    WmBusPacketRecord record = {
        .quality = WmBusPacketQualityCrcOk,
        .rssi = -82,
    };

    if(record.quality != WmBusPacketQualityCrcOk) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected quality=%u", (unsigned int)record.quality);
        return false;
    }
    if(!wmbus_packet_record_passes_policy(&record, WmBusPacketQualityCrcOk, 0)) {
        wmbus_selftest_set_detail(detail, detail_len, "disabled RSSI gate rejected record");
        return false;
    }
    if(!wmbus_packet_record_passes_policy(&record, WmBusPacketQualityHeaderOk, -85)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "enabled RSSI gate rejected strong-enough record");
        return false;
    }
    if(wmbus_packet_record_passes_policy(&record, WmBusPacketQualityHeaderOk, -80)) {
        wmbus_selftest_set_detail(detail, detail_len, "enabled RSSI gate accepted weak record");
        return false;
    }
    if(wmbus_packet_record_passes_policy(&record, WmBusPacketQualityParsed, 0)) {
        wmbus_selftest_set_detail(detail, detail_len, "quality gate accepted unparsed record");
        return false;
    }

    record.quality = WmBusPacketQualityParsed;
    if(record.quality != WmBusPacketQualityParsed ||
       !wmbus_packet_record_passes_policy(&record, WmBusPacketQualityParsed, 0)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "parsed quality failed quality=%u", (unsigned int)record.quality);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "policy rssi_off=YES rssi_gate=YES quality=YES");
    return true;
}

static bool check_scaled_value_extremes(char* detail, size_t detail_len) {
    char text[40];
    wmbus_application_format_scaled_value(INT64_MIN, 0, text, sizeof(text));
    if(strcmp(text, "-9223372036854775808") != 0) return false;
    wmbus_application_format_scaled_value(1, INT8_MIN, text, sizeof(text));
    if(strcmp(text, "1e-128") != 0) return false;
    WmBusApplicationRecord record = {
        .quantity = WmBusApplicationQuantityVolume,
        .value_type = WmBusApplicationValueScalar,
        .value = 1,
        .scale10 = INT8_MIN,
    };
    uint32_t volume;
    if(wmbus_application_find_total_volume(&record, 1U, &volume)) return false;
    wmbus_selftest_set_detail(
        detail, detail_len, "Extreme scales avoid overflowing powers of ten");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_tooling[] = {
    {"check_scaled_value_extremes", check_scaled_value_extremes},
    {"check_3of6_valid_single_byte", wmbus_selftest_check_3of6_valid_single_byte},
    {"check_supported_l_field_range", wmbus_selftest_check_supported_l_field_range},
    {"check_3of6_reject_dangling_nibble", wmbus_selftest_check_3of6_reject_dangling_nibble},
    {"check_3of6_reject_invalid_symbol", wmbus_selftest_check_3of6_reject_invalid_symbol},
    {"check_parser_plausibility", wmbus_selftest_check_parser_plausibility},
    {"check_tpl_security_modes", wmbus_selftest_check_tpl_security_modes},
    {"check_ci72_protected_payload", wmbus_selftest_check_ci72_protected_payload},
    {"check_ci72_mode5_decrypt", wmbus_selftest_check_ci72_mode5_decrypt},
    {"check_ell_chained_tpl_headers", wmbus_selftest_check_ell_chained_tpl_headers},
    {"check_clear_ell_uses_tpl_security", wmbus_selftest_check_clear_ell_uses_tpl_security},
    {"check_long_tpl_ci_headers", wmbus_selftest_check_long_tpl_ci_headers},
    {"check_ell_security_modes", wmbus_selftest_check_ell_security_modes},
    {"check_format_fields_text_uses_context",
     wmbus_selftest_check_format_fields_text_uses_context},
    {"check_packet_detail_retains_volume", wmbus_selftest_check_packet_detail_retains_volume},
    {"check_packet_quality_policy", wmbus_selftest_check_packet_quality_policy},
};

const WmBusSelftestCheck* wmbus_selftest_tooling_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_tooling);
    return wmbus_selftest_checks_tooling;
}
