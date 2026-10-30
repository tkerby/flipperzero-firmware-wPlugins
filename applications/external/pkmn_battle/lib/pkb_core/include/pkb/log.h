#pragma once

/*
 * Binary battle log (.pbl), written to the SD card by the device and read by
 * host tools (tools/gen/decode_log.py, later pkb_harness replay).
 *
 * Layout (little endian):
 *   header  "PBL1", u8 version, u8 flags, u16 reserved, u32 unix time
 *   records u8 type, u16 length, payload[length]
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKB_LOG_VERSION 1
#define PKB_LOG_HEADER_LEN 12

typedef enum {
    PkbLogRnTx = 1,
    PkbLogRnRx = 2,
    PkbLogPartyTx = 3,
    PkbLogPartyRx = 4,
    PkbLogPatchTx = 5,
    PkbLogPatchRx = 6,
    /* u16 index, u8 our nibble, u8 GB nibble, u32 CRC of mirrored engine
     * state (0 until the engine exists) */
    PkbLogExchange = 7,
    /* u8 result (0 unknown, 1 win, 2 lose, 3 draw), u32 link bytes */
    PkbLogBattleEnd = 8,
    /* free-form ASCII */
    PkbLogNote = 9,
} PkbLogType;

typedef struct PkbLog {
    uint8_t* buf;
    uint32_t cap;
    uint32_t len;
    bool overflow;
} PkbLog;

void pkb_log_init(PkbLog* log, uint8_t* buf, uint32_t cap, uint32_t unix_time);
/* Appends one record; sets overflow (and drops it) if it does not fit. */
void pkb_log_record(PkbLog* log, PkbLogType type, const void* data, uint16_t len);
void pkb_log_exchange(PkbLog* log, uint16_t index, uint8_t mine, uint8_t gb, uint32_t crc);
void pkb_log_note(PkbLog* log, const char* text);

#ifdef __cplusplus
}
#endif
