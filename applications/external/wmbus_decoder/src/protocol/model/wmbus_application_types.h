#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WMBUS_PACKET_RECORD_MAX    12U
/* EN 13757-3 permits at most ten DIFE and ten VIFE bytes per record. */
#define WMBUS_APPLICATION_DIFE_MAX 10U
#define WMBUS_APPLICATION_VIFE_MAX 10U

/** Identifies the decoder that produced the application-level data. */
typedef enum {
    WmBusParserIdUnknown = 0,
    WmBusParserIdRaw,
    WmBusParserIdHeader,
    WmBusParserIdEll,
    WmBusParserIdShortTpl,
    WmBusParserIdLongTpl,
    WmBusParserIdDifVif,
    WmBusParserIdApator162,
    WmBusParserIdQWaterV2,
} WmBusParserId;

typedef enum {
    WmBusApplicationValueNone = 0,
    WmBusApplicationValueScalar,
    WmBusApplicationValueDateTime,
    WmBusApplicationValueUnavailable,
    WmBusApplicationValueRaw,
} WmBusApplicationValueType;

typedef enum {
    WmBusApplicationMeasurementTypeUnknown = 0,
    WmBusApplicationMeasurementTypeInstantaneous,
    WmBusApplicationMeasurementTypeMinimum,
    WmBusApplicationMeasurementTypeMaximum,
    WmBusApplicationMeasurementTypeAtError,
} WmBusApplicationMeasurementType;

typedef enum {
    WmBusApplicationQuantityUnknown = 0,
    WmBusApplicationQuantityVolume,
    WmBusApplicationQuantityEnergy,
    WmBusApplicationQuantityPower,
    WmBusApplicationQuantityVolumeFlow,
    WmBusApplicationQuantityFlowTemperature,
    WmBusApplicationQuantityReturnTemperature,
    WmBusApplicationQuantityTemperatureDifference,
    WmBusApplicationQuantityHeatCostAllocation,
    WmBusApplicationQuantityDate,
    WmBusApplicationQuantityDateTime,
    WmBusApplicationQuantityVersion,
    WmBusApplicationQuantityStatus,
    WmBusApplicationQuantityVoltage,
} WmBusApplicationQuantity;

typedef uint8_t WmBusApplicationDateTimePrecision;

enum {
    WmBusApplicationDateTimePrecisionDate = 0,
    WmBusApplicationDateTimePrecisionMinute,
    WmBusApplicationDateTimePrecisionSecond,
};

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    WmBusApplicationDateTimePrecision precision;
} WmBusApplicationDateTime;

typedef struct {
    /* The value type selects one representation; widest members come first to avoid padding. */
    union {
        int64_t value;
        uint8_t raw_bytes[8];
        WmBusApplicationDateTime value_datetime;
    };
    uint64_t storage_no;
    uint32_t tariff;
    WmBusApplicationValueType value_type;
    WmBusApplicationMeasurementType measurement_type;
    WmBusApplicationQuantity quantity;
    uint16_t subunit;
    uint8_t dif;
    uint8_t vif;
    uint8_t data_len;
    int8_t scale10;
} WmBusApplicationRecord;

typedef struct {
    WmBusParserId parser_id;
    uint8_t record_count;
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX];
} WmBusPacketApplicationData;
