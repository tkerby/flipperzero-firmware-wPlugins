/**
 * 20q.dat reader. Pure C11 -- no Furi headers, so tests/ can link it with
 * plain cc. All file access goes through Tq_Reader (SPEC 6.7).
 *
 * The dataset is NEVER loaded into RAM. Columns are streamed on demand
 * (SPEC 1, "the single most important design consequence").
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TQ_MAGIC          0x51303246UL /* "F20Q" little-endian */
#define TQ_FORMAT_VERSION 1
#define TQ_HEADER_SIZE    0xC0
#define TQ_LUT_SIZE       16

/* Guard rails, SPEC 5.6. */
#define TQ_MAX_ENTITIES  16384
#define TQ_MAX_QUESTIONS 1024

#define TQ_FLAG_HAS_PRIORS     (1u << 0)
#define TQ_FLAG_HAS_CATEGORIES (1u << 1)
#define TQ_FLAG_HAS_FAMILIES   (1u << 2)

#define TQ_MAX_NAME       22
#define TQ_MAX_QTEXT      72
#define TQ_CATEGORY_COUNT 4

typedef enum {
    TqOk = 0,
    TqErrIo,
    TqErrMagic,
    TqErrVersion,
    TqErrRange,
    TqErrCrc,
} Tq_Error;

/** Byte-range reader. ctx is a Furi File* on device, a FILE* in tests. */
typedef struct {
    bool (*read_at)(void* ctx, uint32_t offset, void* buf, size_t len);
    void* ctx;
    uint32_t size; /* total file size in bytes, used for CRC verification */
} Tq_Reader;

typedef struct {
    Tq_Reader reader;

    uint32_t entity_count;
    uint32_t question_count;
    uint32_t stride; /* bytes per matrix column = (N + 1) / 2 */

    uint16_t flags;
    uint32_t matrix_off;
    uint32_t ent_index_off;
    uint32_t ent_text_off;
    uint32_t qst_index_off;
    uint32_t qst_text_off;
    uint32_t prior_off;
    uint32_t category_off;
    uint32_t qrank_off;
    uint32_t family_off;

    uint32_t crc32;
    uint32_t build_epoch;
    char dataset_id[33];

    float lut[TQ_LUT_SIZE];

    /* Small hot tables cached in RAM: 2 * Q + Q bytes, <= 3 KB at Q=1024. */
    uint16_t* qrank; /* question ids, best first */
    uint16_t* qrank_pos; /* inverse of qrank: rank of question j */
    uint8_t* family; /* family id per question, 0 if absent */
    uint8_t* category; /* category id per entity, N bytes, 3 if absent */
} Tq_Dataset;

/**
 * Parses the header and caches the ranking tables. Does not read the matrix.
 * verify_crc streams the whole data region (SPEC 9) -- skip it for tests.
 */
Tq_Error tq_dataset_open(Tq_Dataset* ds, Tq_Reader reader, bool verify_crc);
void tq_dataset_close(Tq_Dataset* ds);

const char* tq_error_str(Tq_Error err);

/** Reads column j into colbuf, which must hold ds->stride bytes. */
bool tq_dataset_read_column(const Tq_Dataset* ds, uint32_t j, uint8_t* colbuf);

/** P(yes) for entity i out of a column previously read by read_column. */
static inline float tq_pyes(const Tq_Dataset* ds, const uint8_t* col, uint32_t i) {
    uint8_t b = col[i >> 1];
    /* SPEC 5.3: even entity = low nibble. */
    uint8_t nib = (i & 1u) ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0Fu);
    return ds->lut[nib];
}

/** Copies a NUL-terminated name/text out of the text blocks. */
bool tq_dataset_entity_name(const Tq_Dataset* ds, uint32_t i, char* out, size_t out_size);
bool tq_dataset_question_text(const Tq_Dataset* ds, uint32_t j, char* out, size_t out_size);

/** Case-insensitive exact search over the sorted entity-name block. */
bool tq_dataset_find_entity(const Tq_Dataset* ds, const char* name, uint32_t* out_index);

/**
 * Streams the whole prior block into out[entity_count] as normalised-ish
 * weights in (0, 1]. One sequential read, not N scattered ones.
 */
bool tq_dataset_read_priors(const Tq_Dataset* ds, float* out);

float tq_dataset_prior(const Tq_Dataset* ds, uint32_t i);
uint8_t tq_dataset_category(const Tq_Dataset* ds, uint32_t i);
uint8_t tq_dataset_family(const Tq_Dataset* ds, uint32_t j);

#ifdef __cplusplus
}
#endif
