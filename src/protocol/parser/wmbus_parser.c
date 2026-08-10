#include "wmbus_parser.h"

#include "../../core/wmbus_types.h"

static const char* const wmbus_parser_names[] = {
    [WmBusParserIdUnknown] = "Unknown",
    [WmBusParserIdRaw] = "Raw",
    [WmBusParserIdHeader] = "Header",
    [WmBusParserIdEll] = "ELL",
    [WmBusParserIdShortTpl] = "Short TPL",
    [WmBusParserIdLongTpl] = "Long TPL",
    [WmBusParserIdDifVif] = "DifVif",
    [WmBusParserIdApator162] = "Apator162",
};

const char* wmbus_parser_id_name(WmBusParserId parser_id) {
    size_t index = (size_t)parser_id;
    if(index >= sizeof(wmbus_parser_names) / sizeof(wmbus_parser_names[0]) ||
       !wmbus_parser_names[index]) {
        return wmbus_parser_names[WmBusParserIdUnknown];
    }
    return wmbus_parser_names[index];
}

bool wmbus_parser_ci_has_ell(uint8_t ci) {
    switch(ci) {
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        return true;
    default:
        return false;
    }
}

bool wmbus_parser_ell_has_session_fields(uint8_t ci) {
    return ci == 0x8DU || ci == 0x8FU;
}

uint8_t wmbus_parser_ell_security_mode(uint32_t sn) {
    return (uint8_t)((sn >> 29) & 0x07U);
}

bool wmbus_parser_ell_security_likely_encrypted(uint32_t sn) {
    return wmbus_parser_ell_security_mode(sn) == 0x01U;
}

uint8_t wmbus_parser_tpl_security_mode(uint16_t cfg) {
    return (uint8_t)((cfg >> 8) & 0x1FU);
}

bool wmbus_parser_tpl_has_encrypted_payload(uint16_t cfg) {
    uint8_t security_mode = wmbus_parser_tpl_security_mode(cfg);
    if(security_mode == 0U) {
        return false;
    }
    if(security_mode == 0x05U && ((cfg >> 4) & 0x0FU) == 0U) {
        return false;
    }

    /* Unsupported security modes stay closed. */
    return true;
}
