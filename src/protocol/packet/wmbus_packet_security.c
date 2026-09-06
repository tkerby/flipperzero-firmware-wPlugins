#include "wmbus_packet_security.h"

#include <furi.h>

#include "../crypto/wmbus_crypto_mode5.h"

#define TAG "WmBusDecoder"

uint8_t wmbus_packet_ell_security_mode(uint32_t sn) {
    return (uint8_t)((sn >> 29) & 0x07U);
}

bool wmbus_packet_ell_payload_is_encrypted(uint8_t security_mode) {
    return security_mode == WmBusEllSecurityModeAesCtr;
}

uint8_t wmbus_packet_tpl_security_mode(uint16_t cfg) {
    return (uint8_t)((cfg >> 8) & 0x1FU);
}

bool wmbus_packet_tpl_payload_is_protected(uint16_t cfg) {
    const uint8_t security_mode = wmbus_packet_tpl_security_mode(cfg);
    if(security_mode == WmBusTplSecurityModeClear) return false;

    /* Mode 5 with zero encrypted blocks carries a clear application payload. */
    if(security_mode == WmBusTplSecurityModeAesCbcIv && ((cfg >> 4) & 0x0FU) == 0U) {
        return false;
    }

    /* Unknown and unsupported protection modes remain closed. */
    return true;
}

static const char* wmbus_packet_decrypt_result_str(WmBusDecryptResult result) {
    switch(result) {
    case WmBusDecryptResultOk:
        return "ok";
    case WmBusDecryptResultInvalidArgs:
        return "invalid args";
    case WmBusDecryptResultInvalidEncryptedLength:
        return "invalid encrypted length";
    case WmBusDecryptResultCryptoError:
        return "crypto error";
    default:
        return "unknown";
    }
}

static const char* wmbus_packet_log_id(const WmBusPacketRecord* record) {
    if(!record) return "--------";
    return record->identity.meter_id[0] ? record->identity.meter_id : "--------";
}

static void
    wmbus_packet_log_decrypt_failure_reason(const WmBusPacketRecord* record, const char* reason) {
    if(!record || !reason || reason[0] == '\0') return;

    FURI_LOG_D(
        TAG,
        "decrypt failed id=%s ci=%02X cfg=%04X reason=%s",
        wmbus_packet_log_id(record),
        record->tpl.ci_field,
        record->tpl.cfg,
        reason);
}

bool wmbus_packet_resolve_application_payload(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusCryptoKeyStore* key_store,
    uint8_t* decrypt_workspace,
    size_t decrypt_workspace_size,
    WmBusPacketPayloadView* out_payload) {
    if(!frame || !record || !decrypt_workspace || decrypt_workspace_size == 0U || !out_payload) {
        return false;
    }

    record->tpl.decrypted = false;
    record->tpl.key_index = 0U;

    *out_payload = (WmBusPacketPayloadView){0};

    /* Only these CI values declare the supported variable-data application.
     * Other services (compact data, DLMS, SML, AFL, etc.) must not be guessed. */
    if(record->tpl.header_len == 0U || record->tpl.header_len >= frame_len ||
       (record->tpl.ci_field != 0x72U && record->tpl.ci_field != 0x7AU &&
        record->tpl.ci_field != 0x78U)) {
        return false;
    }
    const uint8_t* bytes = &frame[record->tpl.header_len];
    const size_t payload_len = frame_len - record->tpl.header_len;

    if(record->ell.has_session && record->ell.security_mode != 0U) {
        FURI_LOG_D(
            TAG,
            "unsupported ELL protection id=%s ci=%02X mode=%02X",
            wmbus_packet_log_id(record),
            record->dll.ci_field,
            record->ell.security_mode);
        return false;
    }

    if(record->tpl.header_type == WmBusTplHeaderNone ||
       !wmbus_packet_tpl_payload_is_protected(record->tpl.cfg)) {
        if(record->tpl.security_mode == WmBusTplSecurityModeAesCbcIv &&
           (((record->tpl.cfg >> 4) & 0x0FU) == 0U) &&
           (frame_len < (size_t)record->tpl.header_len + 2U ||
            frame[record->tpl.header_len] != 0x2FU ||
            frame[record->tpl.header_len + 1U] != 0x2FU)) {
            wmbus_packet_log_decrypt_failure_reason(record, "missing Mode5 2F2F check bytes");
            return false;
        }
        *out_payload = (WmBusPacketPayloadView){payload_len, bytes};
        return true;
    }

    if(record->tpl.security_mode != WmBusTplSecurityModeAesCbcIv) {
        FURI_LOG_D(
            TAG,
            "unsupported TPL protection id=%s ci=%02X cfg=%04X mode=%02X",
            wmbus_packet_log_id(record),
            record->tpl.ci_field,
            record->tpl.cfg,
            record->tpl.security_mode);
        return false;
    }

    if(payload_len > decrypt_workspace_size) return false;
    for(uint8_t i = 0; key_store && i < key_store->count && i < WMBUS_CRYPTO_KEY_STORE_MAX; i++) {
        WmBusDecryptResult result = wmbus_crypto_mode5_decrypt(
            bytes,
            payload_len,
            record->tpl.iv_address,
            record->tpl.acc,
            record->tpl.cfg,
            key_store->keys[i],
            decrypt_workspace);
        if(result != WmBusDecryptResultOk) {
            wmbus_packet_log_decrypt_failure_reason(
                record, wmbus_packet_decrypt_result_str(result));
            return false;
        }
        /* EN 13757-7 / OMS Mode 5 verification, never parser plausibility. */
        if(payload_len >= 2U && decrypt_workspace[0] == 0x2FU && decrypt_workspace[1] == 0x2FU) {
            *out_payload = (WmBusPacketPayloadView){payload_len, decrypt_workspace};
            record->tpl.decrypted = true;
            record->tpl.key_index = i;
            return true;
        }
    }
    return false;
}
