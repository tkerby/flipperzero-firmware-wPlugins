/*
 * nb_virtual.h - a "virtual card" NbBackend that serves a card image loaded from a file.
 *
 * Load a dump, and the PC sees that card as if it had been tapped on the reader:
 * both the PN532 engine (libnfc) and the CCID engine (PC/SC) just see an NbBackend
 * whose detect() reports the card and whose reads return the dump's data.
 *
 * Accepted files (auto-detected from the content, not the extension):
 *   - Flipper .nfc  : "Device type" = Mifare Classic, NTAG/Ultralight (and old
 *                     "Mifare Ultralight"/"NTAGxxx"), or ISO14443-3A / UID (UID only).
 *   - hex text      : MIFARE Classic Tool style dumps, one 32-hex-digit line per block
 *                     (optional "+Sector: n" lines, '-' / '?' = unknown).
 *   - raw binary    : 320 / 1024 / 4096 bytes = MIFARE Classic Mini / 1K / 4K (.mfd, .bin,
 *                     .dump ...); any other multiple of 4 from 64 to 4092 bytes = Ultralight /
 *                     NTAG pages.
 *
 * MIFARE Classic is simulated with the real access-condition rules: you need the right
 * key (read from the sector trailer in the dump), Key A is never readable, Key B only when
 * the access bits say so, and writes obey the access bits.  Blocks/keys the dump marks as
 * unknown ("??") cannot be read or authenticated, like a card you don't have the key for.
 *
 * Writes from the PC change the in-RAM image only; nothing is written back to the file.
 *
 * Streaming loader: nbv_load_begin(), nbv_load_feed() any number of times with chunks of
 * the file, nbv_load_end().  Portable C; no Flipper dependencies.
 */
#pragma once
#include "nb_backend.h"

typedef enum { ModeUndecided, ModeFlipper, ModeHexText, ModeBinary } LoadMode;
typedef enum { DevNone, DevMfc, DevUl, DevUid } DevType;
typedef enum { KindEmpty, KindMfc, KindUl, KindUid } Kind;

struct NbVirtual {
    NbBackend be;
    uint32_t (*clock)(void);

    /* ---- the card ---- */
    bool loaded;
    volatile bool present;
    Kind kind;
    NbTag tag;
    char name[28];
    bool dirty;
    unsigned units; /* Classic: blocks, Ultralight: pages */
    uint8_t data[4096]; /* MAX_BYTES */
    uint8_t known[1024 / 8]; /* MAX_UNITS / 8 */
    uint8_t key_unk[40]; /* MAX_SECTORS */

    /* ---- loader state ---- */
    LoadMode mode;
    uint8_t head[64]; /* HEAD_LEN */
    size_t head_len;
    char line[160]; /* LINE_MAX_LEN */
    size_t line_len;
    bool line_overflow;
    size_t bin_len;
    DevType dev;
    bool saw_filetype;
    uint8_t uid[NB_UID_MAX];
    uint8_t uid_len;
    bool have_uid;
    uint8_t atqa[2];
    bool have_atqa;
    uint8_t sak;
    bool have_sak;
    unsigned mfc_blocks; /* from "Mifare Classic type", 0 = not given */
    unsigned pages_total; /* from "Pages total", 0 = not given */
    unsigned unit_seen; /* highest unit index seen + 1 */
    unsigned hex_next; /* hex text: next block number */
    char err[64]; /* ERR_LEN */
};

typedef struct NbVirtual NbVirtual;

NbVirtual* nbv_alloc(void);
void nbv_free(NbVirtual* v);


/* Millisecond clock the CCID engine uses to pace its card-presence polling. */
void nbv_set_clock(NbVirtual* v, uint32_t (*now_ms)(void));

/* --- loading ---------------------------------------------------------------- */
void nbv_load_begin(NbVirtual* v);
/* false = the file is not usable; nbv_error() says why. Once false, stays false. */
bool nbv_load_feed(NbVirtual* v, const uint8_t* data, size_t len);
bool nbv_load_end(NbVirtual* v);
/* begin + feed + end for a file that is already in memory */
bool nbv_load_buffer(NbVirtual* v, const uint8_t* data, size_t len);
const char* nbv_error(const NbVirtual* v);
/* For callers that fail before/while feeding (e.g. cannot open the file). */
void nbv_fail(NbVirtual* v, const char* msg);

/* --- the loaded card -------------------------------------------------------- */
bool nbv_loaded(const NbVirtual* v);
const char* nbv_name(const NbVirtual* v); /* e.g. "MIFARE Classic 1K", "NTAG215" */
const NbTag* nbv_tag(const NbVirtual* v);

/* "Remove" / "insert" the card, like taking it off the reader.  Safe to call from
 * another thread than the one running the protocol engine. */
void nbv_set_present(NbVirtual* v, bool present);
bool nbv_present(const NbVirtual* v);

/* The table the protocol engines use. Valid until nbv_free(). */
const NbBackend* nbv_backend(NbVirtual* v);
