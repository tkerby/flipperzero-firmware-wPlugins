#include <string.h>
#include <pkb/log.h>

static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

void pkb_log_init(PkbLog* log, uint8_t* buf, uint32_t cap, uint32_t unix_time) {
    log->buf = buf;
    log->cap = cap;
    log->len = 0;
    log->overflow = cap < PKB_LOG_HEADER_LEN;
    if(log->overflow) return;
    memcpy(buf, "PBL1", 4);
    buf[4] = PKB_LOG_VERSION;
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;
    put32(buf + 8, unix_time);
    log->len = PKB_LOG_HEADER_LEN;
}

void pkb_log_record(PkbLog* log, PkbLogType type, const void* data, uint16_t len) {
    if(log->len + 3u + len > log->cap) {
        log->overflow = true;
        return;
    }
    uint8_t* p = log->buf + log->len;
    p[0] = (uint8_t)type;
    p[1] = (uint8_t)len;
    p[2] = (uint8_t)(len >> 8);
    if(len) memcpy(p + 3, data, len);
    log->len += 3u + len;
}

void pkb_log_exchange(PkbLog* log, uint16_t index, uint8_t mine, uint8_t gb, uint32_t crc) {
    uint8_t rec[8] = {(uint8_t)index, (uint8_t)(index >> 8), mine, gb};
    put32(rec + 4, crc);
    pkb_log_record(log, PkbLogExchange, rec, sizeof(rec));
}

void pkb_log_note(PkbLog* log, const char* text) {
    size_t n = strlen(text);
    if(n > 0xFFFF) n = 0xFFFF;
    pkb_log_record(log, PkbLogNote, text, (uint16_t)n);
}
