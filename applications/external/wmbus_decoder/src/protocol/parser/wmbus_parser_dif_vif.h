#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_packet.h"

bool wmbus_parser_dif_vif_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application);
bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count);

/* Transient record view; data points into the caller's payload. */
typedef struct {
    WmBusApplicationRecord record;
    const uint8_t* data;
    uint8_t first_vife;
    uint8_t vife_count;
} WmBusDataRecord;

typedef enum {
    WmBusDataRecordEnd,
    WmBusDataRecordOk,
    WmBusDataRecordInvalid,
} WmBusDataRecordStatus;

WmBusDataRecordStatus wmbus_dif_vif_next_record(
    const uint8_t* payload,
    size_t payload_len,
    size_t* offset,
    WmBusDataRecord* out);
