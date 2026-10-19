#include "nb_virtual.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BYTES    4096 /* MIFARE Classic 4K, or up to 1024 Ultralight pages */
#define MAX_UNITS    1024 /* 16-byte blocks (Classic) or 4-byte pages (Ultralight) */
#define MAX_SECTORS  40
#define LINE_MAX_LEN 160
#define HEAD_LEN     64 /* bytes buffered to decide the file format */
#define ERR_LEN      64

/* ------------------------------------------------------------------ helpers */

/* ------------------------------------------------------------------ helpers */

static bool fail(NbVirtual* v, const char* fmt, ...) {
    if(v->err[0] == 0) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(v->err, sizeof(v->err), fmt, ap);
        va_end(ap);
        if(v->err[0] == 0) snprintf(v->err, sizeof(v->err), "Load failed");
    }
    return false;
}

static void set_known(NbVirtual* v, unsigned u) {
    if(u < MAX_UNITS) v->known[u >> 3] |= (uint8_t)(1u << (u & 7));
}

static bool is_known(const NbVirtual* v, unsigned u) {
    return u < MAX_UNITS && ((v->known[u >> 3] >> (u & 7)) & 1);
}

static int hexval(int c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool is_unk_char(char c) {
    return c == '?' || c == '-';
}

/* "04 5F ?? 12" or "045F??12" -> bytes. unk[i] = byte had a '?'/'-' in it (value 0).
 * Returns the byte count, or -1 on bad syntax / more than `max` bytes. */
static int parse_bytes(const char* s, uint8_t* out, uint8_t* unk, int max) {
    int n = 0;
    for(;;) {
        while(*s == ' ' || *s == '\t')
            s++;
        if(!*s) break;
        char a = s[0], b = s[1];
        if(!b) return -1;
        bool ua = is_unk_char(a), ub = is_unk_char(b);
        int ha = hexval(a), hb = hexval(b);
        if((ha < 0 && !ua) || (hb < 0 && !ub)) return -1;
        if(n >= max) return -1;
        out[n] = (ua || ub) ? 0 : (uint8_t)((ha << 4) | hb);
        unk[n] = (ua || ub) ? 1 : 0;
        n++;
        s += 2;
    }
    return n;
}

static bool parse_uint(const char* s, unsigned* out) {
    if(*s < '0' || *s > '9') return false;
    unsigned x = 0;
    while(*s >= '0' && *s <= '9') {
        x = x * 10 + (unsigned)(*s - '0');
        if(x > 100000) return false;
        s++;
    }
    *out = x;
    return *s == 0 || *s == ' ';
}

static bool ci_eq(const char* a, const char* b) {
    while(*a && *b) {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

/* ---------------------------------------------------------- Classic geometry */

static bool is_trailer(unsigned b) {
    return b < 128 ? (b % 4) == 3 : ((b - 128) % 16) == 15;
}
static unsigned trailer_of(unsigned b) {
    return b < 128 ? (b | 3u) : 128 + (((b - 128) | 15u));
}
static unsigned sector_of(unsigned b) {
    return b < 128 ? b / 4 : 32 + (b - 128) / 16;
}
/* which of the 4 access-bit groups (3 = the trailer) a block belongs to */
static unsigned group_of(unsigned b) {
    if(b < 128) return b % 4;
    unsigned off = (b - 128) % 16;
    return off == 15 ? 3 : off / 5;
}

static bool ac_valid(const uint8_t* tr) {
    uint8_t b6 = tr[6], b7 = tr[7], b8 = tr[8];
    uint8_t n7 = (uint8_t)~b7, n8 = (uint8_t)~b8;
    return (b6 & 0x0F) == ((n7 >> 4) & 0x0F) && ((b6 >> 4) & 0x0F) == (n8 & 0x0F) &&
           (b7 & 0x0F) == ((n8 >> 4) & 0x0F);
}

/* C1C2C3 of a group as a number 0..7 (C1 is the high bit). A corrupt access-bit field
 * falls back to the factory "transport" setting instead of locking the card for good. */
static unsigned acc_code(const uint8_t* tr, unsigned g) {
    if(!ac_valid(tr)) return g == 3 ? 1 : 0;
    unsigned c1 = (tr[7] >> (4 + g)) & 1;
    unsigned c2 = (tr[8] >> g) & 1;
    unsigned c3 = (tr[8] >> (4 + g)) & 1;
    return (c1 << 2) | (c2 << 1) | c3;
}

/* ------------------------------------------------------------ storing units */

static void store_block(NbVirtual* v, unsigned blk, const uint8_t* b, const uint8_t* unk) {
    memcpy(v->data + blk * 16, b, 16);
    bool any = false;
    for(int i = 0; i < 16; i++)
        if(!unk[i]) any = true;
    if(any) set_known(v, blk);
    if(is_trailer(blk)) {
        unsigned s = sector_of(blk);
        if(s < MAX_SECTORS) {
            uint8_t f = 0;
            for(int i = 0; i < 6; i++)
                if(unk[i]) f |= 1;
            for(int i = 10; i < 16; i++)
                if(unk[i]) f |= 2;
            v->key_unk[s] = f;
        }
    }
    if(blk + 1 > v->unit_seen) v->unit_seen = blk + 1;
}

static void store_page(NbVirtual* v, unsigned page, const uint8_t* b, const uint8_t* unk) {
    memcpy(v->data + page * 4, b, 4);
    for(int i = 0; i < 4; i++) {
        if(!unk[i]) {
            set_known(v, page);
            break;
        }
    }
    if(page + 1 > v->unit_seen) v->unit_seen = page + 1;
}

/* --------------------------------------------------------- building the card */

typedef struct {
    unsigned blocks;
    NbTagType type;
    const char* name;
    uint8_t sak;
    uint8_t atqa0; /* low bits of ATQA byte 0 (type), before the UID-size bits */
} MfcInfo;

static const MfcInfo MFC_INFO[] = {
    {20, NbTagMfcMini, "MIFARE Classic Mini", 0x09, 0x04},
    {64, NbTagMfc1k, "MIFARE Classic 1K", 0x08, 0x04},
    {256, NbTagMfc4k, "MIFARE Classic 4K", 0x18, 0x02},
};

/* The ATQA field in .nfc files is stored in either byte order depending on the
 * firmware that wrote it ("00 04" for a 1K, "44 00" for an NTAG).  On the air the low
 * byte comes first and the high byte is 0 for every card we handle, so swap if needed. */
static void norm_atqa(uint8_t a[2]) {
    if(a[0] == 0x00 && a[1] != 0x00) {
        uint8_t t = a[0];
        a[0] = a[1];
        a[1] = t;
    }
}

static bool atqa_plausible(const uint8_t a[2], uint8_t atqa0, uint8_t uid_len) {
    uint8_t size_bits = (uint8_t)(uid_len == 7 ? 0x40 : 0x00);
    return a[1] == 0x00 && (a[0] & 0x0F) == atqa0 && (a[0] & 0xC0) == size_bits;
}

/* No UID line in the file (raw dumps): read UID / SAK / ATQA out of block 0. */
static bool derive_from_block0(NbVirtual* v) {
    if(v->have_uid) return true;
    if(!is_known(v, 0)) return fail(v, "Block 0 missing: UID unknown");
    const uint8_t* b = v->data;
    if((uint8_t)(b[0] ^ b[1] ^ b[2] ^ b[3]) == b[4]) { /* 4-byte UID + BCC */
        v->uid_len = 4;
        memcpy(v->uid, b, 4);
        v->sak = b[5];
        v->atqa[0] = b[6];
        v->atqa[1] = b[7];
    } else { /* assume a 7-byte UID */
        v->uid_len = 7;
        memcpy(v->uid, b, 7);
        v->sak = b[7];
        v->atqa[0] = b[8];
        v->atqa[1] = b[9];
    }
    v->have_uid = v->have_sak = v->have_atqa = true;
    return true;
}

static bool build_mfc(NbVirtual* v, unsigned blocks) {
    const MfcInfo* mi = NULL;
    for(size_t i = 0; i < sizeof(MFC_INFO) / sizeof(MFC_INFO[0]); i++)
        if(MFC_INFO[i].blocks == blocks) mi = &MFC_INFO[i];
    if(!mi) return fail(v, "Unsupported block count %u", blocks);
    if(!derive_from_block0(v)) return false;
    if(v->uid_len != 4 && v->uid_len != 7) return fail(v, "Unsupported UID length %u", v->uid_len);

    NbTag t;
    memset(&t, 0, sizeof(t));
    t.sak = mi->sak; /* a SAK that would make this look like another kind of card is replaced */
    if(v->have_sak) {
        NbTag probe;
        memset(&probe, 0, sizeof(probe));
        probe.sak = v->sak;
        if(nb_tag_type(&probe) == mi->type) t.sak = v->sak;
    }
    t.atqa[0] = (uint8_t)((v->uid_len == 7 ? 0x40 : 0x00) | mi->atqa0);
    t.atqa[1] = 0x00;
    if(v->have_atqa) {
        uint8_t a[2] = {v->atqa[0], v->atqa[1]};
        norm_atqa(a);
        if(atqa_plausible(a, mi->atqa0, v->uid_len)) {
            t.atqa[0] = a[0];
            t.atqa[1] = a[1];
        }
    }
    memcpy(t.uid, v->uid, v->uid_len);
    t.uid_len = v->uid_len;

    v->tag = t;
    v->kind = KindMfc;
    v->units = blocks;
    snprintf(v->name, sizeof(v->name), "%s", mi->name);
    return true;
}

static void ul_name(unsigned pages, char* out, size_t n) {
    switch(pages) {
    case 16:
        snprintf(out, n, "MIFARE Ultralight");
        break;
    case 20:
        snprintf(out, n, "Ultralight EV1 (11)");
        break;
    case 41:
        snprintf(out, n, "Ultralight EV1 (21)");
        break;
    case 48:
        snprintf(out, n, "Ultralight C");
        break;
    case 45:
        snprintf(out, n, "NTAG213");
        break;
    case 135:
        snprintf(out, n, "NTAG215");
        break;
    case 231:
        snprintf(out, n, "NTAG216");
        break;
    default:
        snprintf(out, n, "NTAG/UL %u pages", pages);
    }
}

static bool build_ul(NbVirtual* v, unsigned pages) {
    if(pages < 4 || pages > MAX_UNITS) return fail(v, "Bad page count %u", pages);
    if(!v->have_uid) { /* UID0-2 | BCC0 | UID3-6 | BCC1 */
        if(!is_known(v, 0) || !is_known(v, 1)) return fail(v, "Pages 0-1 missing: UID unknown");
        memcpy(v->uid, v->data, 3);
        memcpy(v->uid + 3, v->data + 4, 4);
        v->uid_len = 7;
        v->have_uid = true;
    }
    if(v->uid_len != 4 && v->uid_len != 7) return fail(v, "Unsupported UID length %u", v->uid_len);

    NbTag t;
    memset(&t, 0, sizeof(t));
    t.sak = 0x00; /* always: this is what makes the engines treat it as Ultralight/NTAG */
    t.atqa[0] = (uint8_t)((v->uid_len == 7 ? 0x40 : 0x00) | 0x04);
    if(v->have_atqa) {
        uint8_t a[2] = {v->atqa[0], v->atqa[1]};
        norm_atqa(a);
        if(atqa_plausible(a, 0x04, v->uid_len)) {
            t.atqa[0] = a[0];
            t.atqa[1] = a[1];
        }
    }
    memcpy(t.uid, v->uid, v->uid_len);
    t.uid_len = v->uid_len;

    v->tag = t;
    v->kind = KindUl;
    v->units = pages;
    ul_name(pages, v->name, sizeof(v->name));
    return true;
}

static bool build_uid_only(NbVirtual* v) {
    if(!v->have_uid) return fail(v, "No UID in file");
    NbTag t;
    memset(&t, 0, sizeof(t));
    t.atqa[0] = (uint8_t)((v->uid_len == 7 ? 0x40 : 0x00) | 0x04);
    if(v->have_atqa) {
        t.atqa[0] = v->atqa[0];
        t.atqa[1] = v->atqa[1];
        norm_atqa(t.atqa);
    }
    t.sak = v->have_sak ? v->sak : 0x00;
    memcpy(t.uid, v->uid, v->uid_len);
    t.uid_len = v->uid_len;
    v->tag = t;
    v->kind = KindUid;
    v->units = 0;
    snprintf(v->name, sizeof(v->name), "UID-only card");
    return true;
}

/* ------------------------------------------------------------ Flipper .nfc */

static bool flipper_line(NbVirtual* v, char* line) {
    char* colon = strchr(line, ':');
    if(!colon) return true;
    *colon = 0;
    char* key = line;
    char* kend = colon;
    while(kend > key && (kend[-1] == ' ' || kend[-1] == '\t'))
        *--kend = 0;
    char* val = colon + 1;
    while(*val == ' ' || *val == '\t')
        val++;
    size_t vl = strlen(val);
    while(vl && (val[vl - 1] == ' ' || val[vl - 1] == '\t'))
        val[--vl] = 0;

    if(ci_eq(key, "Filetype")) {
        if(!strstr(val, "Flipper NFC")) return fail(v, "Not a Flipper NFC file");
        v->saw_filetype = true;
        return true;
    }
    if(ci_eq(key, "Device type")) {
        if(ci_eq(val, "Mifare Classic")) {
            v->dev = DevMfc;
        } else if(
            ci_eq(val, "NTAG/Ultralight") || ci_eq(val, "Mifare Ultralight") ||
            strncmp(val, "NTAG", 4) == 0) {
            v->dev = DevUl;
        } else if(ci_eq(val, "ISO14443-3A") || ci_eq(val, "UID")) {
            v->dev = DevUid;
        } else {
            return fail(v, "Unsupported: %.28s", val);
        }
        return true;
    }
    if(ci_eq(key, "UID")) {
        uint8_t unk[NB_UID_MAX];
        int n = parse_bytes(val, v->uid, unk, NB_UID_MAX);
        if(n != 4 && n != 7 && n != 10) return fail(v, "Bad UID line");
        for(int i = 0; i < n; i++)
            if(unk[i]) return fail(v, "UID has unknown bytes");
        v->uid_len = (uint8_t)n;
        v->have_uid = true;
        return true;
    }
    if(ci_eq(key, "ATQA")) {
        uint8_t unk[2];
        if(parse_bytes(val, v->atqa, unk, 2) != 2 || unk[0] || unk[1])
            return fail(v, "Bad ATQA line");
        v->have_atqa = true;
        return true;
    }
    if(ci_eq(key, "SAK")) {
        uint8_t unk[1];
        if(parse_bytes(val, &v->sak, unk, 1) != 1 || unk[0]) return fail(v, "Bad SAK line");
        v->have_sak = true;
        return true;
    }
    if(ci_eq(key, "Mifare Classic type")) {
        if(ci_eq(val, "Mini"))
            v->mfc_blocks = 20;
        else if(ci_eq(val, "1K"))
            v->mfc_blocks = 64;
        else if(ci_eq(val, "4K"))
            v->mfc_blocks = 256;
        else
            return fail(v, "Unsupported Classic: %.24s", val);
        return true;
    }
    if(ci_eq(key, "Pages total")) {
        if(!parse_uint(val, &v->pages_total) || v->pages_total > MAX_UNITS)
            return fail(v, "Bad 'Pages total'");
        return true;
    }
    if(strncmp(key, "Block ", 6) == 0) {
        unsigned n;
        uint8_t b[16], unk[16];
        if(!parse_uint(key + 6, &n) || n >= 256) return fail(v, "Bad block number");
        if(parse_bytes(val, b, unk, 16) != 16) return fail(v, "Block %u: need 16 bytes", n);
        store_block(v, n, b, unk);
        return true;
    }
    if(strncmp(key, "Page ", 5) == 0) {
        unsigned n;
        uint8_t b[4], unk[4];
        if(!parse_uint(key + 5, &n) || n >= MAX_UNITS) return fail(v, "Bad page number");
        if(parse_bytes(val, b, unk, 4) != 4) return fail(v, "Page %u: need 4 bytes", n);
        store_page(v, n, b, unk);
        return true;
    }
    return true; /* signature, counters, tearing, format version, ... : not needed */
}

static bool finish_flipper(NbVirtual* v) {
    if(!v->saw_filetype) return fail(v, "Not a Flipper NFC file");
    switch(v->dev) {
    case DevMfc: {
        unsigned blocks = v->mfc_blocks;
        if(!blocks) {
            if(!v->unit_seen) return fail(v, "No block data");
            blocks = v->unit_seen <= 20 ? 20 : (v->unit_seen <= 64 ? 64 : 256);
        }
        if(v->unit_seen > blocks) return fail(v, "Block %u beyond card size", v->unit_seen - 1);
        return build_mfc(v, blocks);
    }
    case DevUl: {
        unsigned pages = v->pages_total ? v->pages_total : v->unit_seen;
        if(!pages) return fail(v, "No page data");
        if(v->unit_seen > pages) return fail(v, "Page %u beyond card size", v->unit_seen - 1);
        return build_ul(v, pages);
    }
    case DevUid:
        return build_uid_only(v);
    default:
        return fail(v, "No Device type in file");
    }
}

/* ------------------------------------------------------ hex text (MCT style) */

static bool hex_line(NbVirtual* v, char* line) {
    if(line[0] == '+') return true; /* "+Sector: n" */
    uint8_t b[16], unk[16];
    if(parse_bytes(line, b, unk, 16) != 16)
        return fail(v, "Line %u: need 16 bytes", v->hex_next + 1);
    if(v->hex_next >= 256) return fail(v, "More than 256 blocks");
    store_block(v, v->hex_next++, b, unk);
    return true;
}

static bool finish_hex(NbVirtual* v) {
    if(v->hex_next != 20 && v->hex_next != 64 && v->hex_next != 256)
        return fail(v, "Unexpected block count %u", v->hex_next);
    return build_mfc(v, v->hex_next);
}

/* ------------------------------------------------------------------- binary */

static bool looks_like_text(const uint8_t* d, size_t n) {
    if(n > 16) n = 16;
    for(size_t i = 0; i < n; i++)
        if(!((d[i] >= 0x20 && d[i] <= 0x7E) || d[i] == '\r' || d[i] == '\n' || d[i] == '\t'))
            return false;
    return n > 0;
}

static bool finish_binary(NbVirtual* v) {
    size_t n = v->bin_len;
    if(n < 64) return fail(v, "File too small (%u bytes)", (unsigned)n);
    if(looks_like_text(v->data, n)) return fail(v, "Unrecognised text file");
    if(n == 2048) return fail(v, "Classic 2K dumps not supported (size 2048)");
    if(n == 320 || n == 1024 || n == 4096) {
        unsigned blocks = (unsigned)(n / 16);
        for(unsigned i = 0; i < blocks; i++)
            set_known(v, i);
        return build_mfc(v, blocks);
    }
    if(n % 4 == 0 && n <= MAX_BYTES) {
        unsigned pages = (unsigned)(n / 4);
        for(unsigned i = 0; i < pages; i++)
            set_known(v, i);
        return build_ul(v, pages);
    }
    return fail(v, "Unrecognised dump size (%u bytes)", (unsigned)n);
}

/* ------------------------------------------------------------ the loader core */

static bool end_line(NbVirtual* v) {
    bool overflow = v->line_overflow;
    v->line[v->line_len] = 0;
    char* l = v->line;
    v->line_len = 0;
    v->line_overflow = false;
    if(l[0] == 0 || l[0] == '#') return true;
    if(overflow) { /* a line we cannot hold: fine unless it is card data */
        if(strncmp(l, "Block ", 6) == 0 || strncmp(l, "Page ", 5) == 0 || v->mode == ModeHexText)
            return fail(v, "Line too long");
        return true;
    }
    return v->mode == ModeFlipper ? flipper_line(v, l) : hex_line(v, l);
}

static bool text_feed(NbVirtual* v, const uint8_t* d, size_t n) {
    for(size_t i = 0; i < n; i++) {
        uint8_t c = d[i];
        if(c == '\n' || c == '\r') {
            if(!end_line(v)) return false;
        } else if(c == 0) {
            return fail(v, "Binary data in text file");
        } else if(v->line_len < sizeof(v->line) - 1) {
            v->line[v->line_len++] = (char)c;
        } else {
            v->line_overflow = true;
        }
    }
    return true;
}

static bool binary_feed(NbVirtual* v, const uint8_t* d, size_t n) {
    if(n > MAX_BYTES - v->bin_len) return fail(v, "File too large (max %u bytes)", MAX_BYTES);
    memcpy(v->data + v->bin_len, d, n);
    v->bin_len += n;
    return true;
}

static bool feed_mode(NbVirtual* v, const uint8_t* d, size_t n) {
    return v->mode == ModeBinary ? binary_feed(v, d, n) : text_feed(v, d, n);
}

/* Look at the first bytes and pick the file format, then replay them. */
static bool decide(NbVirtual* v) {
    const uint8_t* h = v->head;
    size_t n = v->head_len;
    if(n == 0) return fail(v, "Empty file");
    if(n >= 3 && h[0] == 0xEF && h[1] == 0xBB && h[2] == 0xBF) { /* UTF-8 BOM */
        h += 3;
        n -= 3;
    }
    if(n >= 9 && memcmp(h, "Filetype:", 9) == 0) {
        v->mode = ModeFlipper;
    } else if(n >= 7 && memcmp(h, "+Sector", 7) == 0) {
        v->mode = ModeHexText;
    } else {
        bool hex = n >= 33 && (h[32] == '\r' || h[32] == '\n');
        for(size_t i = 0; hex && i < 32; i++)
            if(hexval(h[i]) < 0 && !is_unk_char((char)h[i])) hex = false;
        v->mode = hex ? ModeHexText : ModeBinary;
    }
    return feed_mode(v, h, n);
}

/* --------------------------------------------------------------- public: load */

void nbv_load_begin(NbVirtual* v) {
    NbBackend be = v->be;
    uint32_t (*clock)(void) = v->clock;
    memset(v, 0, sizeof(*v));
    v->be = be;
    v->clock = clock;
}

bool nbv_load_feed(NbVirtual* v, const uint8_t* d, size_t n) {
    if(v->err[0]) return false;
    if(v->mode == ModeUndecided) {
        size_t take = HEAD_LEN - v->head_len;
        if(take > n) take = n;
        memcpy(v->head + v->head_len, d, take);
        v->head_len += take;
        d += take;
        n -= take;
        if(v->head_len < HEAD_LEN) return true; /* n == 0 here */
        if(!decide(v)) return false;
    }
    return n == 0 ? true : feed_mode(v, d, n);
}

bool nbv_load_end(NbVirtual* v) {
    if(v->err[0]) return false;
    if(v->mode == ModeUndecided && !decide(v)) return false;
    if(v->mode != ModeBinary && v->line_len > 0 && !end_line(v)) return false;

    bool ok;
    switch(v->mode) {
    case ModeFlipper:
        ok = finish_flipper(v);
        break;
    case ModeHexText:
        ok = finish_hex(v);
        break;
    default:
        ok = finish_binary(v);
        break;
    }
    if(!ok) return false;
    v->loaded = true;
    v->present = true;
    return true;
}

bool nbv_load_buffer(NbVirtual* v, const uint8_t* d, size_t n) {
    nbv_load_begin(v);
    return nbv_load_feed(v, d, n) && nbv_load_end(v);
}

const char* nbv_error(const NbVirtual* v) {
    return v->err[0] ? v->err : "";
}

void nbv_fail(NbVirtual* v, const char* msg) {
    fail(v, "%s", msg);
}

/* ------------------------------------------------------------ the simulation */

static int v_detect(void* ctx, NbTag* out) {
    NbVirtual* v = ctx;
    if(!v->loaded || !v->present) return 0;
    *out = v->tag;
    return 1;
}

static bool v_is_present(void* ctx) {
    NbVirtual* v = ctx;
    return v->loaded && v->present;
}

static int v_apdu(void* ctx, const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_max) {
    (void)ctx;
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_max;
    return NB_ERR_UNSUPPORTED; /* no ISO14443-4 cards in this loader */
}

/* Verify the key against the sector trailer. */
static int mfc_check(NbVirtual* v, unsigned blk, bool key_b, const uint8_t key[6]) {
    if(!v->loaded || !v->present) return NB_ERR_TIMEOUT;
    if(v->kind != KindMfc) return NB_ERR_IO;
    if(blk >= v->units) return NB_ERR_IO;
    unsigned t = trailer_of(blk);
    if(t >= v->units) return NB_ERR_IO;
    unsigned s = sector_of(blk);
    if(!is_known(v, t) || (s < MAX_SECTORS && (v->key_unk[s] & (key_b ? 2 : 1))))
        return NB_ERR_AUTH;
    const uint8_t* tr = v->data + t * 16;
    return memcmp(key, key_b ? tr + 10 : tr, 6) == 0 ? NB_OK : NB_ERR_AUTH;
}

static int v_mfc_auth(void* ctx, uint8_t blk, bool key_b, const uint8_t key[6]) {
    return mfc_check(ctx, blk, key_b, key);
}

static int v_mfc_read(void* ctx, uint8_t blk, bool key_b, const uint8_t key[6], uint8_t out[16]) {
    NbVirtual* v = ctx;
    int r = mfc_check(v, blk, key_b, key);
    if(r != NB_OK) return r;
    const uint8_t* tr = v->data + trailer_of(blk) * 16;
    if(is_trailer(blk)) {
        unsigned code = acc_code(tr, 3);
        memcpy(out, tr, 16);
        memset(out, 0, 6); /* Key A can never be read back */
        /* Key B is only readable (with Key A) when the access bits allow it. The access bits
         * themselves are always returned: real cards are vague about reading them with Key B
         * and handing them out is the harmless choice for the PC-side tools. */
        if(key_b || !(code == 0 || code == 1 || code == 2)) memset(out + 10, 0, 6);
        return NB_OK;
    }
    unsigned code = acc_code(tr, group_of(blk));
    bool ok = code == 7 ? false : ((code == 3 || code == 5) ? key_b : true);
    if(!ok) return NB_ERR_AUTH; /* the card NAKs */
    if(!is_known(v, blk)) return NB_ERR_IO; /* the dump has no data for this block */
    memcpy(out, v->data + blk * 16, 16);
    return NB_OK;
}

static int
    v_mfc_write(void* ctx, uint8_t blk, bool key_b, const uint8_t key[6], const uint8_t in[16]) {
    NbVirtual* v = ctx;
    int r = mfc_check(v, blk, key_b, key);
    if(r != NB_OK) return r;
    if(blk == 0) return NB_ERR_IO; /* manufacturer block is read-only, like a real card */
    unsigned t = trailer_of(blk);
    uint8_t* tr = v->data + t * 16;
    if(is_trailer(blk)) {
        unsigned code = acc_code(tr, 3);
        bool w_key = (code == 0 || code == 1) ? !key_b :
                                                ((code == 4 || code == 3) ? key_b : false);
        bool w_ac = code == 1 ? !key_b : ((code == 3 || code == 5) ? key_b : false);
        bool ch_a = memcmp(in, tr, 6) != 0, ch_ac = memcmp(in + 6, tr + 6, 4) != 0,
             ch_b = memcmp(in + 10, tr + 10, 6) != 0;
        if((ch_a && !w_key) || (ch_ac && !w_ac) || (ch_b && !w_key)) return NB_ERR_AUTH;
        if(!ac_valid(in)) return NB_ERR_IO; /* a real card would be bricked; refuse instead */
        memcpy(tr, in, 16);
        v->dirty = true;
        unsigned s = sector_of(blk);
        if(s < MAX_SECTORS) {
            if(ch_a) v->key_unk[s] &= (uint8_t)~1u;
            if(ch_b) v->key_unk[s] &= (uint8_t)~2u;
        }
        set_known(v, blk);
        return NB_OK;
    }
    unsigned code = acc_code(tr, group_of(blk));
    bool w = code == 0 ? true : ((code == 4 || code == 6 || code == 3) ? key_b : false);
    if(!w) return NB_ERR_AUTH;
    memcpy(v->data + blk * 16, in, 16);
    v->dirty = true;
    set_known(v, blk);
    return NB_OK;
}

static int v_mful_read(void* ctx, uint8_t page, uint8_t out[16]) {
    NbVirtual* v = ctx;
    if(!v->loaded || !v->present) return NB_ERR_TIMEOUT;
    if(v->kind != KindUl || page >= v->units) return NB_ERR_IO;
    for(unsigned i = 0; i < 4; i++)
        memcpy(out + 4 * i, v->data + ((page + i) % v->units) * 4, 4); /* reads roll over */
    return NB_OK;
}

static int v_mful_write(void* ctx, uint8_t page, const uint8_t in[4]) {
    NbVirtual* v = ctx;
    if(!v->loaded || !v->present) return NB_ERR_TIMEOUT;
    if(v->kind != KindUl || page < 3 || page >= v->units) return NB_ERR_IO; /* UID / lock pages */
    uint8_t* p = v->data + page * 4;
    if(page == 3) {
        for(int i = 0; i < 4; i++)
            p[i] |= in[i]; /* OTP area: bits can only be set */
    } else {
        memcpy(p, in, 4);
        v->dirty = true;
    }
    set_known(v, page);
    return NB_OK;
}

static uint32_t v_tick(void* ctx) {
    NbVirtual* v = ctx;
    return v->clock ? v->clock() : 0;
}

/* ---------------------------------------------------------------- public: misc */

static void init_backend(NbVirtual* v) {
    memset(&v->be, 0, sizeof(v->be));
    v->be.ctx = v;
    v->be.detect = v_detect;
    v->be.apdu = v_apdu;
    v->be.raw = NULL;
    v->be.mfc_auth = v_mfc_auth;
    v->be.mfc_read = v_mfc_read;
    v->be.mfc_write = v_mfc_write;
    v->be.mful_read = v_mful_read;
    v->be.mful_write = v_mful_write;
    v->be.release = NULL;
    v->be.field = NULL;
    v->be.tick_ms = v_tick;
    v->be.is_present = v_is_present;
}

NbVirtual* nbv_alloc(void) {
    NbVirtual* v = malloc(sizeof(NbVirtual));
    if(!v) return NULL;
    memset(v, 0, sizeof(*v));
    init_backend(v);
    return v;
}

void nbv_free(NbVirtual* v) {
    free(v);
}

void nbv_set_clock(NbVirtual* v, uint32_t (*now_ms)(void)) {
    v->clock = now_ms;
}

bool nbv_loaded(const NbVirtual* v) {
    return v->loaded;
}

const char* nbv_name(const NbVirtual* v) {
    return v->loaded ? v->name : "";
}

const NbTag* nbv_tag(const NbVirtual* v) {
    return &v->tag;
}

void nbv_set_present(NbVirtual* v, bool present) {
    v->present = present;
}

bool nbv_present(const NbVirtual* v) {
    return v->loaded && v->present;
}

const NbBackend* nbv_backend(NbVirtual* v) {
    return &v->be;
}
