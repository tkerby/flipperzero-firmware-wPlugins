// The Hall of Legacy file (contract section 50.6), ending plugin. See hof.h for the layout.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hof.h"
#include "assets.h"
#include "save.h"
#include "settings.h"

_Static_assert(HOF_REC == 4 + 1 + 1 + HOF_TEAM * (2 + 2 * HOF_NICK) + 4, "hof record = secs, n, pad, 6 x (species, level, nickname), CRC");
_Static_assert(sizeof(SAVE_PATH) > 9 && sizeof(SAVE_PATH) < 112, "the hof path is derived from SAVE_PATH");

#define HOF_PATH_CAP 128

void hof_path(char* out, int cap) {
    const size_t n = sizeof(SAVE_PATH) - 1 - 8; // SAVE_PATH ends with "save.bin"
    memcpy(out, SAVE_PATH, n);
    snprintf(out + n, (size_t)cap - n, "%s", HOF_PATH_SUFFIX);
}

static uint32_t hf_get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void hf_put32(uint8_t* p, uint32_t v) {
    for(int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

// The header (16 bytes) valid? Fills count / head.
static bool hf_hdr_ok(const uint8_t* b, uint8_t* count, uint8_t* head) {
    if(memcmp(b, "NHOF", 4) != 0 || b[4] != HOF_VER || b[7] != HOF_MAX || (b[8] | (b[9] << 8)) != HOF_REC || b[10] || b[11]) return false;
    if(b[5] > HOF_MAX || b[6] >= HOF_MAX || (b[5] == 0 && b[6] != 0)) return false;
    if(hf_get32(b + 12) != settings_crc32(b, 12)) return false;
    *count = b[5];
    *head = b[6];
    return true;
}

static bool hf_read_hdr(uint8_t* count, uint8_t* head) {
    char path[HOF_PATH_CAP];
    uint8_t b[HOF_HDR];
    hof_path(path, sizeof(path));
    if(file_size(path) != HOF_SIZE || !read_at(path, 0, b, sizeof(b))) return false;
    return hf_hdr_ok(b, count, head);
}

uint8_t hof_count(uint8_t* head) {
    uint8_t c = 0, h = 0;
    if(!hf_read_hdr(&c, &h)) c = h = 0;
    if(head) *head = h;
    return c;
}

// A record's bytes (HOF_REC, CRC included) -> *r; false when its CRC or a field is wrong.
static bool hf_rec_parse(const uint8_t* b, HofRun* r) {
    if(hf_get32(b + HOF_REC - 4) != settings_crc32(b, HOF_REC - 4)) return false;
    if(b[4] < 1 || b[4] > HOF_TEAM || b[5]) return false;
    memset(r, 0, sizeof(*r));
    r->secs = hf_get32(b);
    r->n = b[4];
    for(int k = 0; k < HOF_TEAM; k++) {
        const uint8_t* m = b + 6 + k * (2 + 2 * HOF_NICK);
        HofMon* o = &r->mon[k];
        o->species = m[0];
        o->level = m[1];
        for(int i = 0; i < HOF_NICK; i++) o->nick[i] = (uint16_t)(m[2 + 2 * i] | (m[3 + 2 * i] << 8));
        if(k < r->n && (!o->species || !o->level || o->level > 100 || o->nick[HOF_NICK - 1])) return false;
    }
    return true;
}

static void hf_rec_pack(uint8_t* b, const HofRun* r) {
    memset(b, 0, HOF_REC);
    hf_put32(b, r->secs);
    b[4] = r->n;
    for(int k = 0; k < HOF_TEAM; k++) {
        uint8_t* m = b + 6 + k * (2 + 2 * HOF_NICK);
        const HofMon* i = &r->mon[k];
        if(k >= r->n) continue;
        m[0] = i->species;
        m[1] = i->level;
        for(int j = 0; j < HOF_NICK - 1 && i->nick[j]; j++) {
            m[2 + 2 * j] = (uint8_t)i->nick[j];
            m[3 + 2 * j] = (uint8_t)(i->nick[j] >> 8);
        }
    }
    hf_put32(b + HOF_REC - 4, settings_crc32(b, HOF_REC - 4));
}

bool hof_get(uint8_t k, HofRun* r) {
    uint8_t count, head;
    if(!hf_read_hdr(&count, &head) || k >= count) return false;
    const unsigned slot = (head + HOF_MAX - k) % HOF_MAX;
    uint8_t b[HOF_REC];
    char path[HOF_PATH_CAP];
    hof_path(path, sizeof(path));
    return read_at(path, HOF_HDR + slot * HOF_REC, b, sizeof(b)) && hf_rec_parse(b, r);
}

bool hof_add(const HofRun* r) {
    if(!r->n || r->n > HOF_TEAM) return false;
    for(int k = 0; k < r->n; k++)
        if(!r->mon[k].species || !r->mon[k].level || r->mon[k].level > 100) return false;
    if(!heap_can_alloc(HOF_SIZE)) return false;
    uint8_t* f = calloc(1, HOF_SIZE);
    if(!f) return false;
    char path[HOF_PATH_CAP];
    hof_path(path, sizeof(path));
    uint8_t count = 0, head = 0;
    if(hf_read_hdr(&count, &head) && read_at(path, 0, f, HOF_SIZE)) {
        head = count ? (uint8_t)((head + 1) % HOF_MAX) : 0; // the next slot (the oldest run once the ring is full); the first run goes to slot 0
    } else { // a missing or damaged file: a fresh ring with this run alone
        memset(f, 0, HOF_SIZE);
        count = 0;
        head = 0;
    }
    hf_rec_pack(f + HOF_HDR + head * HOF_REC, r);
    if(count < HOF_MAX) count++;
    memcpy(f, "NHOF", 4);
    f[4] = HOF_VER;
    f[5] = count;
    f[6] = head;
    f[7] = HOF_MAX;
    f[8] = HOF_REC & 0xFF;
    f[9] = HOF_REC >> 8;
    f[10] = f[11] = 0;
    hf_put32(f + 12, settings_crc32(f, 12));
    const bool ok = write_file(path, f, HOF_SIZE);
    free(f);
    return ok;
}

bool hof_from_party(HofRun* r) {
    const Party* p = party_player();
    const PartyState* ps = party_state();
    memset(r, 0, sizeof(*r));
    if(!p || !p->count) return false;
    r->n = p->count > HOF_TEAM ? HOF_TEAM : p->count;
    r->secs = save_time_secs();
    for(uint8_t k = 0; k < r->n; k++) {
        r->mon[k].species = p->mon[k].species;
        r->mon[k].level = p->mon[k].level;
        for(int i = 0; i < HOF_NICK - 1 && ps->nick_u[k][i]; i++) r->mon[k].nick[i] = ps->nick_u[k][i];
    }
    return true;
}
