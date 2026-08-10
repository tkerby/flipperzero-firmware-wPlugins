#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../frame/wmbus_frame.h"

typedef struct {
    uint8_t l_field;
    uint8_t c_field;
    uint16_t m_field;
    uint8_t id[4];
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;
} WmBusPacketDllData;

typedef struct {
    bool has_ell;
    uint8_t header_len;
    uint8_t cc;
    uint8_t acc;
    bool has_session;
    uint32_t sn;
    uint8_t security_mode;
    uint16_t payload_crc;
    bool decrypted;
    uint8_t key_index;
} WmBusPacketEllData;

typedef enum {
    WmBusEllSecurityModeClear = 0x00,
    WmBusEllSecurityModeAesCtr = 0x01,
} WmBusEllSecurityMode;

typedef enum {
    WmBusTplHeaderNone = 0,
    WmBusTplHeaderShort,
    WmBusTplHeaderLong,
} WmBusTplHeaderType;

typedef enum {
    WmBusTplSecurityModeClear = 0x00,
    WmBusTplSecurityModeManufacturer = 0x01,
    WmBusTplSecurityModeAesCbcIv = 0x05,
    WmBusTplSecurityModeAesCtrCmac = 0x08,
} WmBusTplSecurityMode;

typedef struct {
    /** Effective CI after an optional ELL header. */
    uint8_t ci_field;
    WmBusTplHeaderType header_type;
    uint8_t header_len;
    /* Address material in the Mode 5 IV order: M-field, ID, version, type. */
    uint8_t iv_address[8];
    uint8_t acc;
    uint8_t tpl_status;
    uint16_t cfg;
    uint8_t security_mode;
    bool decrypted;
    uint8_t key_index;
} WmBusPacketTplData;

/** Non-owning view valid only for the current packet-processing call. */
typedef struct {
    uint16_t application_len;
    const uint8_t* application_bytes;
} WmBusPacketPayloadView;

typedef struct {
    char manufacturer[WMBUS_MFG_STR_LEN];
    char meter_id[WMBUS_ID_STR_LEN];
    bool meter_id_is_bcd;
    uint8_t version;
    uint8_t dev_type;
} WmBusPacketIdentityData;
