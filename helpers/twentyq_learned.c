#include "twentyq_learned.h"

#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void wr_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void wr_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static bool name_eq(const char* a, const char* b) {
    while(*a && *b) {
        if(lower(*a++) != lower(*b++)) return false;
    }
    return *a == *b;
}

void tq_learned_init(Tq_Learned* ln, uint32_t base_crc32) {
    memset(ln, 0, sizeof(*ln));
    ln->base_crc32 = base_crc32;
}

void tq_learned_clear(Tq_Learned* ln) {
    for(uint16_t i = 0; i < ln->count; i++) {
        if(!ln->records[i]) continue;
        free(ln->records[i]->qid);
        free(ln->records[i]->nib);
        free(ln->records[i]);
        ln->records[i] = NULL;
    }
    ln->count = 0;
    ln->dirty = true;
}

bool tq_learned_load(Tq_Learned* ln, const uint8_t* data, size_t len, uint32_t base_crc32) {
    tq_learned_clear(ln);
    ln->base_crc32 = base_crc32;
    ln->dirty = false;
    if(len < 12) return false;
    if(rd_u32(data) != TQ_LEARN_MAGIC) return false;
    if(rd_u16(data + 4) != TQ_LEARN_VERSION) return false;
    uint16_t count = rd_u16(data + 6);
    if(rd_u32(data + 8) != base_crc32) return false; /* dataset was replaced */
    if(count > TQ_LEARN_MAX_RECORDS) return false;

    size_t off = 12;
    for(uint16_t r = 0; r < count; r++) {
        if(off + 1 > len) goto malformed;
        uint8_t name_len = data[off++];
        if(name_len == 0 || name_len > TQ_MAX_NAME) goto malformed;
        if(off + name_len + 3u > len) goto malformed;

        char name[TQ_MAX_NAME + 1];
        memcpy(name, data + off, name_len);
        name[name_len] = '\0';
        off += name_len;

        uint8_t category = data[off++];
        uint16_t answers = rd_u16(data + off);
        off += 2;
        if(answers > TQ_LEARN_MAX_ANSWERS) goto malformed;
        if(off + (size_t)answers * 3u > len) goto malformed;

        uint16_t qids[TQ_LEARN_MAX_ANSWERS];
        uint8_t nibs[TQ_LEARN_MAX_ANSWERS];
        for(uint16_t k = 0; k < answers; k++) {
            qids[k] = rd_u16(data + off);
            nibs[k] = data[off + 2] & 0x0Fu;
            off += 3;
        }
        if(!tq_learned_add(ln, name, category, qids, nibs, answers)) goto malformed;
    }
    ln->dirty = false;
    return true;

malformed:
    tq_learned_clear(ln);
    ln->dirty = false;
    return false;
}

uint8_t* tq_learned_serialize(const Tq_Learned* ln, size_t* out_len) {
    size_t len = 12;
    for(uint16_t i = 0; i < ln->count; i++) {
        len += 1 + strlen(ln->records[i]->name) + 1 + 2 + (size_t)ln->records[i]->answer_count * 3;
    }
    uint8_t* buf = malloc(len);
    wr_u32(buf, TQ_LEARN_MAGIC);
    wr_u16(buf + 4, TQ_LEARN_VERSION);
    wr_u16(buf + 6, ln->count);
    wr_u32(buf + 8, ln->base_crc32);
    size_t off = 12;
    for(uint16_t i = 0; i < ln->count; i++) {
        const Tq_LearnedRecord* r = ln->records[i];
        uint8_t nl = (uint8_t)strlen(r->name);
        buf[off++] = nl;
        memcpy(buf + off, r->name, nl);
        off += nl;
        buf[off++] = r->category;
        wr_u16(buf + off, r->answer_count);
        off += 2;
        for(uint16_t k = 0; k < r->answer_count; k++) {
            wr_u16(buf + off, r->qid[k]);
            buf[off + 2] = r->nib[k];
            off += 3;
        }
    }
    *out_len = off;
    return buf;
}

bool tq_learned_add(
    Tq_Learned* ln,
    const char* name,
    uint8_t category,
    const uint16_t* qids,
    const uint8_t* nibbles,
    uint16_t answer_count) {
    if(ln->count >= TQ_LEARN_MAX_RECORDS) return false;
    if(!name || !name[0]) return false;
    if(answer_count > TQ_LEARN_MAX_ANSWERS) answer_count = TQ_LEARN_MAX_ANSWERS;

    Tq_LearnedRecord* r = malloc(sizeof(Tq_LearnedRecord));
    memset(r, 0, sizeof(*r));
    size_t nl = strlen(name);
    if(nl > TQ_MAX_NAME) nl = TQ_MAX_NAME;
    memcpy(r->name, name, nl);
    r->name[nl] = '\0';
    r->category = (category < TQ_CATEGORY_COUNT) ? category : (TQ_CATEGORY_COUNT - 1);
    r->answer_count = answer_count;
    r->qid = malloc(sizeof(uint16_t) * (answer_count ? answer_count : 1));
    r->nib = malloc(answer_count ? answer_count : 1);

    /* Insertion-sort by question id so lookups can binary-search. */
    for(uint16_t k = 0; k < answer_count; k++) {
        uint16_t q = qids[k];
        uint8_t v = nibbles[k] & 0x0Fu;
        int16_t pos = (int16_t)k;
        while(pos > 0 && r->qid[pos - 1] > q) {
            r->qid[pos] = r->qid[pos - 1];
            r->nib[pos] = r->nib[pos - 1];
            pos--;
        }
        r->qid[pos] = q;
        r->nib[pos] = v;
    }

    ln->records[ln->count++] = r;
    ln->dirty = true;
    return true;
}

bool tq_learned_find(const Tq_Learned* ln, const char* name, uint16_t* out_index) {
    for(uint16_t i = 0; i < ln->count; i++) {
        if(name_eq(ln->records[i]->name, name)) {
            if(out_index) *out_index = i;
            return true;
        }
    }
    return false;
}

bool tq_learned_pyes(
    const Tq_Learned* ln,
    uint16_t record,
    uint16_t qid,
    const float* lut,
    float* out_pyes) {
    if(record >= ln->count) return false;
    const Tq_LearnedRecord* r = ln->records[record];
    uint16_t lo = 0, hi = r->answer_count;
    while(lo < hi) {
        uint16_t mid = (uint16_t)(lo + (hi - lo) / 2);
        if(r->qid[mid] == qid) {
            *out_pyes = lut[r->nib[mid] & 0x0Fu];
            return true;
        }
        if(r->qid[mid] < qid) {
            lo = (uint16_t)(mid + 1);
        } else {
            hi = mid;
        }
    }
    return false;
}
