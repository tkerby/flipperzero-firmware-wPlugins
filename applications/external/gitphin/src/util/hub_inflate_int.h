#pragma once

/* Inflater: buffer and table sizes, the Huffman table and the inflater state.
 * Internal to hub_inflate.c and its parts; not for other modules. */

#define INBUF_SIZE 256
#define FAST_BITS  8
#define FAST_SIZE  (1 << FAST_BITS)
#define MAX_BITS   15
#define STOPPED    (-100) /* internal: partial output complete */

typedef struct {
    uint16_t count[MAX_BITS + 1];
    uint16_t symbol[288];
    uint16_t fast[FAST_SIZE]; /* sym | len << 9, 0 = not in table */
} Huff;

typedef struct {
    HubInfRead rd;
    void* rctx;
    HubInfWrite wr;
    void* wctx;

    uint8_t* win;
    size_t wsize;
    uint8_t wbits; /* log2 of the stream window, 0 = not known yet */
    HubInfFar far;
    void* fctx;
    uint64_t fc_pos; /* far window: the block read last */
    size_t fc_len;
    uint8_t fc[512];
    size_t pos;
    size_t base; /* bytes already flushed (stream mode) */
    bool stream;
    bool partial;

    uint32_t bitbuf;
    int bitcnt;
    size_t inpos;
    size_t inlen;
    size_t in_total; /* bytes taken from the callback */
    bool in_eof;

    int err;
    uint32_t adler_a, adler_b;

    Huff lit, dist;
    uint8_t lens[320];
    uint8_t in[INBUF_SIZE];
} Inf;
