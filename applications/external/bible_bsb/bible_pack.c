#include "bible_pack.h"

static uint16_t bible_pack_read_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t bible_pack_read_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool bible_pack_header_ok(const uint8_t* pack, size_t pack_len, uint16_t* count) {
    if(!pack || pack_len < BIBLE_PACK_HEADER_SIZE) return false;
    if(pack[0] != BIBLE_PACK_MAGIC_0 || pack[1] != BIBLE_PACK_MAGIC_1 ||
       pack[2] != BIBLE_PACK_MAGIC_2 || pack[3] != BIBLE_PACK_MAGIC_3) {
        return false;
    }
    if(bible_pack_read_u16(pack + 6) != BIBLE_PACK_RECORD_SIZE) return false;

    uint8_t window = pack[8];
    uint8_t lookahead = pack[9];
    if(window < 4 || window > BIBLE_PACK_MAX_WINDOW || lookahead >= window) return false;

    uint16_t n = bible_pack_read_u16(pack + 4);
    size_t directory = (size_t)BIBLE_PACK_HEADER_SIZE + (size_t)n * BIBLE_PACK_RECORD_SIZE;
    if(directory > pack_len) return false;

    *count = n;
    return true;
}

static void bible_pack_read_entry(const uint8_t* rec, BiblePackEntry* out) {
    out->book = rec[0];
    out->chapter = rec[1];
    out->comp_size = bible_pack_read_u16(rec + 2);
    out->raw_size = bible_pack_read_u16(rec + 4);
    out->data_offset = bible_pack_read_u32(rec + 6);
}

bool bible_pack_find(
    const uint8_t* pack,
    size_t pack_len,
    uint8_t book,
    uint8_t chapter,
    BiblePackEntry* out) {
    if(!out) return false;

    uint16_t count = 0;
    if(!bible_pack_header_ok(pack, pack_len, &count)) return false;

    uint32_t lo = 0;
    uint32_t hi = count;
    while(lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t* rec =
            pack + BIBLE_PACK_HEADER_SIZE + (size_t)mid * BIBLE_PACK_RECORD_SIZE;
        BiblePackEntry entry;
        bible_pack_read_entry(rec, &entry);

        if(entry.book < book || (entry.book == book && entry.chapter < chapter)) {
            lo = mid + 1;
        } else if(entry.book == book && entry.chapter == chapter) {
            if(entry.comp_size == 0 || entry.raw_size == 0) return false;
            *out = entry;
            return true;
        } else {
            hi = mid;
        }
    }
    return false;
}
