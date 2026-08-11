#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../frame/wmbus_frame.h"

typedef struct {
    uint16_t m_field;
    uint8_t ci_field;
} WmBusPacketDllData;

typedef struct {
    bool has_ell;
    uint8_t header_len;
    bool has_session;
    uint8_t security_mode;
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
    WmBusTplHeaderType header_type;
    uint16_t cfg;
    /** Effective CI after an optional ELL header. */
    uint8_t ci_field;
    uint8_t header_len;
    /* Address material in the Mode 5 IV order: M-field, ID, version, type. */
    uint8_t iv_address[8];
    uint8_t acc;
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
    uint8_t version;
    uint8_t dev_type;
} WmBusPacketIdentityData;
