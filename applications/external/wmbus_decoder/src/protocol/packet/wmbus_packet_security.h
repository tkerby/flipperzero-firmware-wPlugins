#pragma once

#include "wmbus_packet.h"

uint8_t wmbus_packet_ell_security_mode(uint32_t sn);
bool wmbus_packet_ell_payload_is_encrypted(uint8_t security_mode);
uint8_t wmbus_packet_tpl_security_mode(uint16_t cfg);
bool wmbus_packet_tpl_payload_is_protected(uint16_t cfg);

bool wmbus_packet_resolve_application_payload(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    const WmBusCryptoKeyStore* key_store,
    uint8_t* decrypt_workspace,
    size_t decrypt_workspace_size,
    WmBusPacketPayloadView* out_payload);
