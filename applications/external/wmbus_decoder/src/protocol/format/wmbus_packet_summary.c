#include "wmbus_packet_summary.h"

#include <stdio.h>

#include "../packet/wmbus_packet_security.h"

const char* wmbus_packet_summary_frame_type(WmBusFrameFormat format) {
    switch(format) {
    case WmBusFrameFormatA:
        return "A";
    case WmBusFrameFormatB:
        return "B";
    case WmBusFrameFormatUnknown:
    default:
        return "-";
    }
}

static const char*
    wmbus_packet_summary_security_mode_name(uint8_t security_mode, bool ell_security) {
    if(ell_security) {
        switch(security_mode) {
        case WmBusEllSecurityModeClear:
            return "Clear";
        case WmBusEllSecurityModeAesCtr:
            return "AES-CTR";
        default:
            return NULL;
        }
    }

    switch(security_mode) {
    case WmBusTplSecurityModeClear:
        return "Clear";
    case WmBusTplSecurityModeManufacturer:
        return "Manufacturer";
    case WmBusTplSecurityModeAesCbcIv:
        return "AES-CBC IV";
    case WmBusTplSecurityModeAesCtrCmac:
        return "AES-CTR CMAC";
    default:
        return NULL;
    }
}

static void wmbus_packet_summary_format_security_mode(
    uint8_t security_mode,
    bool ell_security,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;

    const char* known_mode = wmbus_packet_summary_security_mode_name(security_mode, ell_security);
    if(known_mode) {
        snprintf(out, out_size, "%s", known_mode);
    } else {
        snprintf(out, out_size, "Mode %02X", security_mode);
    }
}

void wmbus_packet_summary_format_crypto_tag(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(ell && ell->has_ell && ell->has_session && ell->security_mode != 0U) {
        if(wmbus_packet_ell_payload_is_encrypted(ell->security_mode)) {
            snprintf(out, out_size, "EENC");
        } else {
            snprintf(out, out_size, "ES:%02X", ell->security_mode);
        }
        return;
    }

    if(tpl && tpl->header_type != WmBusTplHeaderNone) {
        if(tpl->decrypted) {
            snprintf(out, out_size, "DEC#%u", (unsigned int)tpl->key_index);
        } else if(wmbus_packet_tpl_payload_is_protected(tpl->cfg)) {
            snprintf(out, out_size, "ENC");
        } else if(tpl->security_mode == WmBusTplSecurityModeClear) {
            snprintf(out, out_size, "CLR");
        } else if(tpl->security_mode == WmBusTplSecurityModeManufacturer) {
            snprintf(out, out_size, "MFG");
        } else {
            snprintf(out, out_size, "S:%02X", tpl->security_mode);
        }
        return;
    }

    if(ell && ell->has_ell && ell->has_session) {
        snprintf(out, out_size, "ECLR");
    }
}

void wmbus_packet_summary_format_security_text(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    if(ell && ell->has_ell && ell->has_session && ell->security_mode != 0U) {
        char mode[20] = {0};
        wmbus_packet_summary_format_security_mode(ell->security_mode, true, mode, sizeof(mode));

        if(wmbus_packet_ell_payload_is_encrypted(ell->security_mode)) {
            snprintf(out, out_size, "ELL %s, encrypted", mode);
        } else {
            snprintf(out, out_size, "ELL %s", mode);
        }
        return;
    }

    if(tpl && tpl->header_type != WmBusTplHeaderNone) {
        char mode[20] = {0};
        wmbus_packet_summary_format_security_mode(tpl->security_mode, false, mode, sizeof(mode));

        if(tpl->decrypted) {
            snprintf(out, out_size, "%s, decrypted key #%u", mode, (unsigned int)tpl->key_index);
        } else if(wmbus_packet_tpl_payload_is_protected(tpl->cfg)) {
            snprintf(out, out_size, "%s, encrypted", mode);
        } else {
            snprintf(out, out_size, "%s", mode);
        }
    } else if(ell && ell->has_ell) {
        if(!ell->has_session) {
            snprintf(out, out_size, "ELL");
        } else {
            snprintf(out, out_size, "ELL Clear");
        }
    }
}

uint8_t wmbus_packet_summary_security_mode(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl) {
    if(ell && ell->has_ell && ell->has_session && ell->security_mode != 0U) {
        return ell->security_mode;
    }
    return tpl ? tpl->security_mode : 0U;
}

uint8_t
    wmbus_packet_summary_key_index(const WmBusPacketEllData* ell, const WmBusPacketTplData* tpl) {
    if(ell && ell->has_ell && ell->has_session && ell->security_mode != 0U) {
        return 0U;
    }
    return tpl ? tpl->key_index : 0U;
}
