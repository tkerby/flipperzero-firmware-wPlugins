/**
 * learned.dat overlay (SPEC 5.7). Pure C -- the Furi layer hands this module
 * a byte buffer and takes one back; it never touches storage itself.
 *
 * Learned entities occupy virtual indices N .. N + count - 1.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "twentyq_dataset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TQ_LEARN_MAGIC       0x4C303246UL /* "F20L" */
#define TQ_LEARN_VERSION     1
#define TQ_LEARN_MAX_RECORDS 128
#define TQ_LEARN_MAX_ANSWERS 30

typedef struct {
    char name[TQ_MAX_NAME + 1];
    uint8_t category;
    uint16_t answer_count;
    uint16_t* qid; /* sorted ascending, binary-searched */
    uint8_t* nib;
} Tq_LearnedRecord;

typedef struct {
    Tq_LearnedRecord* records[TQ_LEARN_MAX_RECORDS];
    uint16_t count;
    uint32_t base_crc32;
    bool dirty;
} Tq_Learned;

void tq_learned_init(Tq_Learned* ln, uint32_t base_crc32);
void tq_learned_clear(Tq_Learned* ln);

/**
 * Parses a learned.dat image. Returns false if it is malformed or was built
 * against a different 20q.dat, in which case the caller renames the file to
 * learned.dat.stale and carries on (SPEC 9).
 */
bool tq_learned_load(Tq_Learned* ln, const uint8_t* data, size_t len, uint32_t base_crc32);

/** Serialises to a freshly malloc'd buffer the caller frees. */
uint8_t* tq_learned_serialize(const Tq_Learned* ln, size_t* out_len);

/** false when the overlay is full (SPEC 9: 128 records). */
bool tq_learned_add(
    Tq_Learned* ln,
    const char* name,
    uint8_t category,
    const uint16_t* qids,
    const uint8_t* nibbles,
    uint16_t answer_count);

bool tq_learned_find(const Tq_Learned* ln, const char* name, uint16_t* out_index);

/** Quantised answer for (record, question), or false if the record never said. */
bool tq_learned_pyes(
    const Tq_Learned* ln,
    uint16_t record,
    uint16_t qid,
    const float* lut,
    float* out_pyes);

#ifdef __cplusplus
}
#endif
