// PC box storage (contract section 25, 74), PC plugin. See box.h.
//
// box.bin (little-endian, always BOX_SIZE = 15,972 bytes; version 2 since contract 74):
//   0  "NBOX"   4  u8 version (2)   5  u8 current box 0..11   6  u8 boxes (12)   7  u8 capacity (20)   8..19  u8 count of box 0..11
//   20 u16 record size (66)         22 u8 item slots (50)      23..31 0
//   32 + (box * 20 + slot) * 66 : record = Mon (44 bytes, the party's struct verbatim) + u16 nickname[11] (Unicode, END-terminated, empty = species name)
//   BOX_IT_OFF = 32 + 12 * 20 * 66 : the item storage, 50 x {u8 item id (0 = unused), u8 count 1..99}, the used slots first (in the order they were stored)
// Slots >= the box's count are zero. A file that is missing or whose size / magic / version / sizes / counts do not match is twelve empty boxes and no items.
// Version 1 (4 boxes, 16-byte header, the records right after it, 5,296 bytes) is read as well: box_begin() copies it into the new layout (the records move by +16 bytes, everything else is empty).
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "box.h"
#include "assets.h"
#include "gen/langs_gen.h"
#include "text.h"
#include "save.h"
#include "save_enc.h"
#include "gen/reg_gen.h"

_Static_assert(sizeof(Mon) == 44, "box.bin stores the Mon verbatim: bump BOX_VER and BOX_REC when it changes");
_Static_assert(BOX_REC == 44 + 2 * PARTY_NICK_CAP, "box record = Mon + nickname");
_Static_assert(sizeof(SAVE_PATH) > 9 && sizeof(SAVE_PATH) < 112, "box paths are derived from SAVE_PATH");

#define PATH_CAP 128
#define CHUNK 1024 // the copy's buffer (one heap block, freed at once)

static struct {
    uint8_t cur, n[BOX_COUNT];
    uint8_t it[2 * BOX_IT_N]; // the item storage (the copy's last 100 bytes)
} H; // the session's header (the copy's first 32 bytes) and items

// "<folder of save.bin><name>"
static void bpath(char* out, const char* name) {
    const size_t n = sizeof(SAVE_PATH) - 1 - 8; // SAVE_PATH ends with "save.bin"
    memcpy(out, SAVE_PATH, n);
    snprintf(out + n, PATH_CAP - n, "%s", name);
}

static void hdr_pack(uint8_t* b) {
    memset(b, 0, BOX_HDR);
    memcpy(b, "NBOX", 4);
    b[4] = BOX_VER;
    b[5] = H.cur;
    b[6] = BOX_COUNT;
    b[7] = BOX_CAP;
    for(int i = 0; i < BOX_COUNT; i++) b[8 + i] = H.n[i];
    b[20] = BOX_REC;
    b[21] = BOX_REC >> 8;
    b[22] = BOX_IT_N;
}

// A header of the current version (BOX_HDR bytes) or of version 1 (BOX_V1_HDR); `v1` tells which was read. The record size sits at 20 (v2) / 12 (v1), the counts at 8 (12 of them / 4).
static bool hdr_parse(const uint8_t* b, bool v1) {
    const int nb = v1 ? BOX_V1_COUNT : BOX_COUNT, ro = v1 ? 12 : 20;
    if(memcmp(b, "NBOX", 4) != 0 || b[4] != (v1 ? 1 : BOX_VER) || b[6] != nb || b[7] != BOX_CAP || (b[ro] | (b[ro + 1] << 8)) != BOX_REC || b[5] >= nb || (!v1 && b[22] != BOX_IT_N)) return false;
    for(int i = 0; i < nb; i++)
        if(b[8 + i] > BOX_CAP) return false;
    memset(&H, 0, sizeof(H));
    H.cur = b[5];
    for(int i = 0; i < nb; i++) H.n[i] = b[8 + i];
    return true;
}

// A damaged storage never reaches the bag: slots with item 0 or a count of 0 / above 99 are dropped and the rest keeps its order (the used slots first).
static void it_clean(void) {
    uint8_t k = 0;
    for(uint8_t i = 0; i < BOX_IT_N; i++) {
        const uint8_t id = H.it[2 * i], n = H.it[2 * i + 1];
        if(id && n && n <= 99) {
            H.it[2 * k] = id;
            H.it[2 * k + 1] = n;
            k++;
        }
    }
    memset(H.it + 2 * k, 0, (size_t)(2 * (BOX_IT_N - k)));
}

static uint32_t rec_off(uint8_t b, uint8_t i) {
    return BOX_HDR + ((uint32_t)b * BOX_CAP + i) * BOX_REC;
}

static bool hdr_write(const char* wrk) {
    uint8_t b[BOX_HDR];
    hdr_pack(b);
    return write_at(wrk, 0, b, BOX_HDR);
}

// ---------------------------------------------------------------------------------------------------- session
bool box_begin(void) {
    char bin[PATH_CAP], wrk[PATH_CAP];
    bpath(bin, "box.bin");
    bpath(wrk, "box.wrk");
    memset(&H, 0, sizeof(H));
    uint8_t hb[BOX_HDR];
    const long sz = file_size(bin);
    const bool v1 = sz == BOX_V1_SIZE; // (version 1: migrated into the new layout below; the file on the card is replaced only by a commit)
    const bool have = (sz == BOX_SIZE || v1) && read_at(bin, 0, hb, v1 ? BOX_V1_HDR : BOX_HDR) && hdr_parse(hb, v1);
    if(!have) memset(&H, 0, sizeof(H));
    remove_file(wrk);
    uint8_t* buf = heap_can_alloc(CHUNK) ? malloc(CHUNK) : NULL;
    if(!buf) return false;
    // `have` copies the data bytes of the old file into place: version 2 as it is, version 1 with the records moved up by BOX_HDR - BOX_V1_HDR (the new header and the empty item storage are made here)
    const uint32_t lo = v1 ? BOX_HDR : 0, hi = v1 ? BOX_HDR + BOX_V1_COUNT * BOX_CAP * BOX_REC : BOX_SIZE, sh = v1 ? BOX_HDR - BOX_V1_HDR : 0;
    bool ok = true;
    for(uint32_t off = 0; ok && off < BOX_SIZE; off += CHUNK) { // a sequential copy (the file only ever grows at its end)
        const uint32_t n = BOX_SIZE - off < CHUNK ? BOX_SIZE - off : CHUNK;
        memset(buf, 0, n);
        const uint32_t a = off > lo ? off : lo, e = off + n < hi ? off + n : hi;
        if(have && a < e) ok = read_at(bin, a - sh, buf + (a - off), e - a);
        if(ok && off == 0) hdr_pack(buf);
        ok = ok && write_at(wrk, off, buf, n);
    }
    free(buf);
    if(ok && have && !v1) ok = read_at(wrk, BOX_IT_OFF, H.it, sizeof(H.it)); // (the copy's own item storage: one 100-byte read; a bad slot is dropped by it_clean)
    if(!ok) remove_file(wrk);
    else it_clean();
    return ok;
}

void box_abort(void) {
    char wrk[PATH_CAP];
    bpath(wrk, "box.wrk");
    remove_file(wrk);
}

// cur -> old (when cur exists), then new -> cur; the old one is put back when the second step fails. `old` is left for the caller to delete.
static bool swap_in(const char* nw, const char* cur, const char* old) {
    const bool had = file_size(cur) >= 0;
    if(had) {
        remove_file(old);
        if(!rename_file(cur, old)) return false;
    }
    if(rename_file(nw, cur)) return true;
    if(had) rename_file(old, cur);
    return false;
}

bool box_commit(const World* w) {
    char wrk[PATH_CAP], bin[PATH_CAP], bold[PATH_CAP], snew[PATH_CAP], sold[PATH_CAP];
    bpath(wrk, "box.wrk");
    bpath(bin, "box.bin");
    bpath(bold, "box.old");
    bpath(snew, "save.new");
    bpath(sold, "save.old");
    uint8_t* b = heap_can_alloc(SAVE_LEN) ? malloc(SAVE_LEN) : NULL;
    if(!b) return false;
    save_encode(b, w);
    bool ok = write_file(snew, b, SAVE_LEN);
    free(b);
    if(ok) ok = swap_in(wrk, bin, bold);
    if(ok && !swap_in(snew, SAVE_PATH, sold)) { // the second file failed: the first goes back, nothing changed on disk
        if(file_size(bold) >= 0) rename_file(bold, bin);
        else remove_file(bin);
        ok = false;
    }
    if(ok) {
        remove_file(bold);
        remove_file(sold);
    } else {
        remove_file(snew);
    }
    return ok;
}

// ---------------------------------------------------------------------------------------------------- boxes
uint8_t box_cur(void) {
    return H.cur;
}

// ---------------------------------------------------------------------------------------------------- the item storage (contract 74)
uint8_t box_it_n(void) {
    uint8_t n = 0;
    while(n < BOX_IT_N && H.it[2 * n]) n++;
    return n;
}

bool box_it_get(uint8_t i, uint8_t* item, uint8_t* count) {
    if(i >= BOX_IT_N || !H.it[2 * i]) return false;
    *item = H.it[2 * i];
    *count = H.it[2 * i + 1];
    return true;
}

uint8_t box_it_count(uint8_t item) {
    for(uint8_t i = 0; i < BOX_IT_N && H.it[2 * i]; i++)
        if(H.it[2 * i] == item) return H.it[2 * i + 1];
    return 0;
}

static bool it_save(void) { // the whole storage (100 bytes) to the working copy
    char wrk[PATH_CAP];
    bpath(wrk, "box.wrk");
    return write_at(wrk, BOX_IT_OFF, H.it, sizeof(H.it));
}

// (A failed write changes nothing the caller keeps: it ends the visit with box_abort(), which drops the session's copy of the storage with the working file, and puts the bag back from its snapshot.)
int box_it_add(uint8_t item, uint8_t n) {
    uint8_t i = 0;
    while(i < BOX_IT_N && H.it[2 * i] && H.it[2 * i] != item) i++;
    if(!item || i >= BOX_IT_N) return 0;
    H.it[2 * i] = item; // (a new kind: the slot was zero)
    if(n > 99 - H.it[2 * i + 1]) n = (uint8_t)(99 - H.it[2 * i + 1]);
    H.it[2 * i + 1] = (uint8_t)(H.it[2 * i + 1] + n);
    if(!H.it[2 * i + 1]) H.it[2 * i] = 0; // nothing went into a fresh slot
    return n && !it_save() ? -1 : n;
}

bool box_it_take(uint8_t item, uint8_t n) {
    uint8_t i = 0;
    while(i < BOX_IT_N && H.it[2 * i] && H.it[2 * i] != item) i++;
    if(i >= BOX_IT_N || H.it[2 * i] != item || H.it[2 * i + 1] < n) return false;
    if(!(H.it[2 * i + 1] = (uint8_t)(H.it[2 * i + 1] - n))) { // an emptied slot goes: the later ones move up
        memmove(H.it + 2 * i, H.it + 2 * i + 2, (size_t)(2 * (BOX_IT_N - 1 - i)));
        H.it[2 * (BOX_IT_N - 1)] = H.it[2 * (BOX_IT_N - 1) + 1] = 0;
    }
    return it_save();
}

uint8_t box_n(uint8_t b) {
    return b < BOX_COUNT ? H.n[b] : 0;
}

bool box_set_cur(uint8_t b) {
    char wrk[PATH_CAP];
    if(b >= BOX_COUNT) return false;
    const uint8_t old = H.cur;
    H.cur = b;
    bpath(wrk, "box.wrk");
    if(hdr_write(wrk)) return true;
    H.cur = old;
    return false;
}

static void rec_pack(uint8_t* p, const BoxRec* r) {
    memcpy(p, &r->mon, sizeof(Mon));
    memcpy(p + sizeof(Mon), r->nick, sizeof(r->nick));
}

static void rec_unpack(BoxRec* r, const uint8_t* p) {
    memcpy(&r->mon, p, sizeof(Mon));
    memcpy(r->nick, p + sizeof(Mon), sizeof(r->nick));
    r->nick[PARTY_NICK_CAP - 1] = 0;
}

// Does the record look like a creature? (a flipped bit must not reach the party)
static bool mon_ok(const Mon* m) {
    const BcData* d = party_data();
    if(!d || m->species == 0 || m->species >= d->n_species) return false;
    if(m->level < 1 || m->level > 100 || m->max_hp == 0 || m->hp > m->max_hp) return false;
    for(int i = 0; i < 4; i++)
        if(m->moves[i] >= d->n_moves) return false;
    return true;
}

bool box_get(uint8_t b, uint8_t i, BoxRec* r) {
    char wrk[PATH_CAP];
    uint8_t p[BOX_REC];
    if(b >= BOX_COUNT || i >= H.n[b]) return false;
    bpath(wrk, "box.wrk");
    if(!read_at(wrk, rec_off(b, i), p, BOX_REC)) return false;
    rec_unpack(r, p);
    return mon_ok(&r->mon);
}

bool box_add(uint8_t b, const BoxRec* r) {
    char wrk[PATH_CAP];
    uint8_t p[BOX_REC];
    if(b >= BOX_COUNT || H.n[b] >= BOX_CAP) return false;
    bpath(wrk, "box.wrk");
    rec_pack(p, r);
    if(!write_at(wrk, rec_off(b, H.n[b]), p, BOX_REC)) return false;
    H.n[b]++;
    if(hdr_write(wrk)) return true;
    H.n[b]--;
    return false;
}

bool box_remove(uint8_t b, uint8_t i) {
    char wrk[PATH_CAP];
    uint8_t p[BOX_REC];
    if(b >= BOX_COUNT || i >= H.n[b]) return false;
    bpath(wrk, "box.wrk");
    for(uint8_t k = i + 1; k < H.n[b]; k++) // the later ones move up, one record at a time
        if(!read_at(wrk, rec_off(b, k), p, BOX_REC) || !write_at(wrk, rec_off(b, k - 1), p, BOX_REC)) return false;
    memset(p, 0, BOX_REC);
    H.n[b]--;
    if(!write_at(wrk, rec_off(b, H.n[b]), p, BOX_REC) || !hdr_write(wrk)) return false;
    return true;
}

// ---------------------------------------------------------------------------------------------------- names
// Unicode nickname -> codes of the active language (ASCII is itself, the rest comes from the pairs of lang/<xx>/kb.bin: u16 code + u16 Unicode from byte 20, n at
// byte 12; a character without a pair is '?'). This is the part of names.c (UI plugin) that the box needs: the PC plugin cannot call the UI plugin.
static void nick_codes(tchar* out, const uint16_t* u, int cap) {
    uint8_t* kb = NULL;
    uint16_t n = 0;
    int i = 0;
    for(; u[i] && i < PARTY_NICK_CAP - 1 && i < cap - 1; i++)
        if(u[i] > 0x7E && !kb) {
            char path[28];
            uint8_t h[20];
            snprintf(path, sizeof(path), "lang/%s/kb.bin", LANG_DIR[text_language()]);
            if(read_at(path, 0, h, sizeof(h)) && !memcmp(h, "NKBD", 4) && (n = (uint16_t)(h[12] | (h[13] << 8))) <= 512 && heap_can_alloc(4u * n) &&
               (kb = malloc(4u * n + 1)) && !read_at(path, 20, kb, 4u * n)) {
                free(kb);
                kb = NULL;
            }
            if(!kb) n = 0;
        }
    for(i = 0; u[i] && i < PARTY_NICK_CAP - 1 && i < cap - 1; i++) {
        tchar c = u[i] < 0x7F ? (tchar)u[i] : (tchar)'?';
        for(uint16_t k = 0; u[i] > 0x7E && kb && k < n; k++)
            if((uint16_t)(kb[4 * k + 2] | (kb[4 * k + 3] << 8)) == u[i]) c = (tchar)(kb[4 * k] | (kb[4 * k + 1] << 8));
        out[i] = c;
    }
    out[i] = 0;
    free(kb);
}

int box_name(const BoxRec* r, tchar* out, int cap) {
    if(r->nick[0]) {
        nick_codes(out, r->nick, cap);
        return 0;
    }
    int len = text_load(reg_sp_txt(r->mon.species), out, cap);
    if(len < 0) out[0] = len = 0;
    return len;
}

// ---------------------------------------------------------------------------------------------------- party <-> box
void box_from_party(BoxRec* r, uint8_t slot) {
    PartyState* ps = party_state();
    r->mon = ps->party.mon[slot];
    memcpy(r->nick, ps->nick_u[slot], sizeof(r->nick));
    r->nick[PARTY_NICK_CAP - 1] = 0;
}

void box_party_drop(uint8_t slot) {
    PartyState* ps = party_state();
    Party* p = &ps->party;
    NickRow* nc = party_nick_cache(); // the code cache moves with the rows
    if(slot >= p->count) return;
    for(uint8_t k = slot; k + 1 < p->count; k++) {
        p->mon[k] = p->mon[k + 1];
        memcpy(ps->nick_u[k], ps->nick_u[k + 1], sizeof(ps->nick_u[0]));
        memcpy(nc[k], nc[k + 1], sizeof(nc[0]));
    }
    p->count--;
    memset(&p->mon[p->count], 0, sizeof(Mon));
    memset(ps->nick_u[p->count], 0, sizeof(ps->nick_u[0]));
    memset(nc[p->count], 0, sizeof(nc[0]));
}

bool box_to_party(const BoxRec* r) {
    PartyState* ps = party_state();
    Party* p = &ps->party;
    if(p->count >= BC_MAX_PARTY) return false;
    p->mon[p->count] = r->mon;
    memcpy(ps->nick_u[p->count], r->nick, sizeof(r->nick));
    ps->nick_u[p->count][PARTY_NICK_CAP - 1] = 0;
    nick_codes(party_nick_cache()[p->count], r->nick, PARTY_NICK_CAP);
    p->count++;
    return true;
}

int box_store_pending(void) {
    BoxRec r;
    if(!party_pending_get(&r.mon)) return 0;
    memcpy(r.nick, party_pending_nick(), sizeof(r.nick));
    r.nick[PARTY_NICK_CAP - 1] = 0;
    for(uint8_t k = 0; k < BOX_COUNT; k++) {
        const uint8_t b = (uint8_t)((H.cur + k) % BOX_COUNT);
        if(H.n[b] >= BOX_CAP) continue;
        if(!box_add(b, &r)) return -1;
        if(b != H.cur) box_set_cur(b);
        party_pending_clear();
        return b + 1;
    }
    party_pending_clear(); // does not let you capture with every box full; here the creature is simply let go
    return 0;
}

// ---------------------------------------------------------------------------------------------------- rollback
void box_snap_take(BoxSnap* s) {
    const PartyState* ps = party_state();
    memcpy(s->bag, ps->bag, sizeof(s->bag));
    s->n_bag = ps->n_bag;
    s->party = ps->party;
    memcpy(s->nick, ps->nick_u, sizeof(s->nick));
    memcpy(s->cache, party_nick_cache(), sizeof(s->cache));
    s->pend = g_pending;
}

void box_snap_restore(const BoxSnap* s) {
    PartyState* ps = party_state();
    memcpy(ps->bag, s->bag, sizeof(s->bag));
    ps->n_bag = s->n_bag;
    ps->party = s->party;
    memcpy(ps->nick_u, s->nick, sizeof(s->nick));
    memcpy(party_nick_cache(), s->cache, sizeof(s->cache));
    g_pending = s->pend;
}
