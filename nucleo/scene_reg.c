// The Registro (contract section 30), a screen of the CASINO plugin since contract 76 (it was the PC plugin's; the plugin that has the room): pause menu -> Registro. LIST: the header "Visto N Capturado N",
// then one row per entry of files/reginfo.bin (rank number, name or "-----" while unseen, a code-drawn crystal mark: filled = caught, hollow = seen).
// Up / Down move (wraps), Left / Right turn the page, OK on a seen entry opens it, Back closes (Back long exits the app). ENTRY page 1: the medium front
// picture (as the party summary), name, "No.012" and the types; a CAUGHT entry has page 2: height, weight and the description (a text key of up to
// two pages whose lines are stacked); a seen-only entry stops at page 1. Left / Right / OK turn the page, Up / Down browse the seen
// entries, Back returns to the list. Everything is read on demand (section 27): the rank table once (<= 255 B, in the screen's heap block), one 8-byte
// record per entry shown, the names of the visible rows. Sounds: OK = confirm, Back = cancel (contract 28).
#include <stdlib.h>
#include <string.h>
#include "scene_reg.h"
#include "party.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "monpic.h"
#include "hud_gfx.h"
#include "sound.h"
#include "box.h"
#include "save.h"
#include "assets.h"
#include "battle_core.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"
#include "legend.h"

#define CAP 16         // codes of a name / label (a name token is <= 15 codes)
#define DESC_CAP 200    // codes of the description (TB_BUF)
#define MAX_VIS 6
#ifdef NUCLEO_DEVICE
#define RG_FILE "reginfo.bin"
#else
const char* g_reg_file = "reginfo.bin"; // host tests point it at a damaged copy
#define RG_FILE g_reg_file
#endif
#ifdef NUCLEO_DEVICE
#define RGA_FILE "regarea.bin"
#else
const char* g_rga_file = "regarea.bin"; // host tests point it at a damaged copy
#define RGA_FILE g_rga_file
#endif
#define RGA_HDR 8
#define NONE_CAP 20     // codes of "Area desconocida" (16 in es, the longest)
#define AR_MAX 12       // areas kept for one species (files/regarea.bin, tools/areac.py: the most any species has today is 11)
#define RG_HDR 12
#define RG_REC 8
#define PAD_X 3         // picture frame: blank columns / rows between the picture and the frame line
#define PAD_Y 2
#define LIST_X_rg 2
#define MARK_X 108      // the crystal mark (7 x 7); the scroll arrows are at 122
#define NO_TEXT 0xFFFF

enum { M_LIST = 0, M_ENTRY };
enum { PG_INFO = 0, PG_DATA, PG_AREA }; // entry pages: the picture / name / types, height / weight / description (caught only), the AREA page (contract 75; every seen entry)

typedef struct {
    uint8_t mode, n, cur, top, vis, page;
    uint8_t seen_n, caught_n, info, n_rec;
    uint8_t ent_caught;                  // the open entry is caught
    uint8_t gh;                          // contract 68: the species of the "???" silhouette entry (Genesa, once a rumor of the legend was heard and she is unseen), else 0
    uint8_t shy[REG_BYTES];              // contract 65: bit (species & 7) of byte (species >> 3) = the player owns a variocolor of that species (party, day care, PC boxes): the entry's star
    uint16_t ent_h, ent_w, ent_tx;       // its record (height dm, weight hg, description text id)
    int16_t fx, fy, fw, fh;              // entry page 1: the picture frame (fw = 0: no picture)
    uint8_t n_ty;
    MonPic pic;
    tchar l_seen[CAP], l_caught[CAP], l_ht[CAP], l_wt[CAP];
    tchar l_area[CAP], l_none[NONE_CAP], l_cry[CAP]; // contract 75: "Area", "Area desconocida", "Grito"
    uint8_t ar_n, ar_top;                // the AREA page: how many areas the open entry has for the player's affinity, the first row shown
    uint8_t ar[AR_MAX][2];               // {area index (AREA_KEYS position), how: 1 grass / 2 surf / 4 fishing}
    tchar name[MAX_VIS][CAP];            // names of the visible rows ("-----" for an unseen entry)
    tchar ename[CAP];                    // the open entry's name
    tchar ty[2][CAP];
    tchar desc[DESC_CAP];
    uint8_t rank[];                      // species id of rank 0..n-1
} DS;

static DS* g_rg;

bool scene_reg_active(void) {
    return g_rg != NULL;
}

static const tchar CHARS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '.', '?', 'N', 'o', 'm', 'k', 'g', ' ', 0}; // prefetched (zh streams its glyphs)
static const tchar UNSEEN[] = {'-', '-', '-', '-', '-', 0};

// ---------------------------------------------------------------------------------------------------- data
static void ld_rg(tchar* d, uint16_t id) {
    if(id == NO_TEXT || text_load(id, d, CAP) < 0) {
        d[0] = '?';
        d[1] = '?';
        d[2] = '?';
        d[3] = 0;
    }
}

static bool valid(uint8_t sp) {
    const BcData* d = party_data();
    return d && sp && sp < d->n_species;
}

static bool bit(const uint8_t* a, uint8_t sp) {
    return (a[sp >> 3] >> (sp & 7)) & 1;
}

static bool is_seen(uint8_t sp) { // (a caught species counts as seen even if a damaged save lost its seen bit)
    return valid(sp) && (bit(party_state()->reg_seen, sp) || bit(party_state()->reg_caught, sp));
}

// contract 68: the unseen Genesa after a rumor: a "???" row and a black silhouette (no types, no page 2). DS.gh holds her species id when that applies (set once, when the screen opens), else 0 (a damaged rank table can hold species 0: the test is `gh != 0 &&`)
#define is_gh(s, sp) ((s)->gh && (sp) == (s)->gh)

static bool is_caught(uint8_t sp) {
    return valid(sp) && bit(party_state()->reg_caught, sp);
}

static uint16_t u16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

// The 12-byte header of reginfo.bin; returns the number of entries and the record count, or 0 when the file is missing / not ours.
static int info_header(uint8_t* n_rec, uint32_t* off_rank) {
    uint8_t h[RG_HDR];
    if(!read_at(RG_FILE, 0, h, RG_HDR) || memcmp(h, "NRGI", 4) || h[4] != 1 || h[7] != RG_REC || h[6] < 1) return 0;
    const uint32_t off = (uint32_t)(h[8] | (h[9] << 8) | (h[10] << 16) | ((uint32_t)h[11] << 24));
    if(off < RG_HDR + (uint32_t)h[6] * RG_REC || file_size(RG_FILE) < (int32_t)(off + h[5])) return 0;
    *n_rec = h[6];
    *off_rank = off;
    return h[5];
}

// The record of species sp (indexed by species id); false when the file has none.
static bool rec_read(const DS* s, uint8_t sp, uint16_t* h, uint16_t* w, uint16_t* tx) {
    uint8_t b[RG_REC];
    *h = *w = 0;
    *tx = NO_TEXT;
    if(!s->info || sp >= s->n_rec || !read_at(RG_FILE, RG_HDR + (uint32_t)sp * RG_REC, b, RG_REC)) return false;
    *h = u16(b);
    *w = u16(b + 2);
    *tx = u16(b + 4);
    return true;
}

static void count_all(DS* s) {
    s->seen_n = s->caught_n = 0;
    for(int k = 0; k < s->n; k++) {
        if(is_seen(s->rank[k])) s->seen_n++;
        if(is_caught(s->rank[k])) s->caught_n++;
    }
}

// The names the visible rows draw (an unseen entry is "-----"), prefetched for the SD fonts.
static void load_rows_rg(DS* s) {
    const tchar* all[MAX_VIS + 4];
    int n = 0;
    for(int v = 0; v < s->vis; v++) {
        const int k = s->top + v;
        s->name[v][0] = 0;
        if(k >= s->n) continue;
        const uint8_t sp = s->rank[k];
        if(is_seen(sp)) ld_rg(s->name[v], reg_sp_txt(sp));
        else if(is_gh(s, sp)) ld_rg(s->name[v], NO_TEXT);
        else memcpy(s->name[v], UNSEEN, sizeof(UNSEEN));
        all[n++] = s->name[v];
    }
    all[n++] = s->l_seen;
    all[n++] = s->l_caught;
    all[n++] = CHARS;
    text_prefetch_many(all, n);
}

static void load_labels(DS* s) {
    ld_rg(s->l_seen, TXT_GL_REG_SEEN);
    ld_rg(s->l_caught, TXT_GL_REG_CAUGHT);
    ld_rg(s->l_ht, TXT_GL_REG_HT);
    ld_rg(s->l_wt, TXT_GL_REG_WT);
    ld_rg(s->l_area, TXT_GL_RGA_AREA);
    if(text_load(TXT_GL_RGA_NONE, s->l_none, NONE_CAP) < 0) s->l_none[0] = 0;
    ld_rg(s->l_cry, TXT_GL_RGA_CRY);
    const int p = font_pitch();
    s->vis = (uint8_t)((SCREEN_H - (p + 3)) / p);
    if(s->vis < 1) s->vis = 1;
    if(s->vis > MAX_VIS) s->vis = MAX_VIS;
}

// The AREA page's data (contract 75, files/regarea.bin, tools/areac.py): the zones whose wild tables hold the open entry's species, for the player's affinity (a Lunar player sees the lists of the
// Lunar view). A missing / damaged file, an unknown species or no entry = no areas ("unknown area"); nothing here can fail loudly.
static void area_load(DS* s) {
    uint8_t h[RGA_HDR], b[1 + 2 * AR_MAX];
    uint16_t o[2];
    const uint8_t sp = s->rank[s->cur];
    s->ar_n = s->ar_top = 0;
    if(!is_seen(sp) || !read_at(RGA_FILE, 0, h, RGA_HDR) || memcmp(h, "NRGA", 4) || h[4] != 1 || sp >= h[5] || !read_at(RGA_FILE, RGA_HDR + 4u * sp, o, 4)) return;
    const uint16_t off = o[party_state()->aff == AFF_LUNAR];
    if(!off || !read_at(RGA_FILE, off, b, 1) || b[0] < 1 || b[0] > AR_MAX || !read_at(RGA_FILE, off + 1u, b + 1, 2u * b[0])) return;
    for(int k = 0; k < b[0]; k++) {
        if(b[1 + 2 * k] >= h[6] || b[1 + 2 * k] >= AREA_COUNT || !b[2 + 2 * k]) break;
        s->ar[s->ar_n][0] = b[1 + 2 * k];
        s->ar[s->ar_n++][1] = b[2 + 2 * k];
    }
}

static int area_rows(void) { // rows of the AREA page (below the header and its rule: the header sits 1 px down so the accent of "Área" clears the screen's top edge, contract 78)
    return (SCREEN_H - (font_pitch() + 5)) / font_pitch();
}

// Opens entry `cur`: record, name, types, picture (page 1). A caught entry also reads its description.
static void load_entry(DS* s) {
    const uint8_t sp = s->rank[s->cur];
    const BcData* d = party_data();
    s->ent_caught = is_caught(sp);
    rec_read(s, sp, &s->ent_h, &s->ent_w, &s->ent_tx);
    ld_rg(s->ename, is_gh(s, sp) ? NO_TEXT : reg_sp_txt(sp));
    s->n_ty = 0;
    s->ty[0][0] = s->ty[1][0] = 0;
    if(valid(sp) && d && !is_gh(s, sp)) {
        const SpeciesDef* sd = bc_species(d, sp);
        ld_rg(s->ty[0], reg_ty_txt(sd->type1));
        s->n_ty = 1;
        if(sd->type2 != sd->type1) {
            ld_rg(s->ty[1], reg_ty_txt(sd->type2));
            s->n_ty = 2;
        }
    }
    area_load(s);
    s->desc[0] = 0;
    if(s->ent_caught && s->ent_tx != NO_TEXT && text_load(s->ent_tx, s->desc, DESC_CAP) < 0) s->desc[0] = 0;
    monpic_free(&s->pic);
    s->fw = 0;
    if(monpic_load_kind(&s->pic, sp, BP_FRONT_MED)) {
        if(is_gh(s, sp)) memset(s->pic.blk, 0xFF, (size_t)s->pic.b.bpr * s->pic.b.h); // the silhouette: every picture pixel black (the mask keeps the shape)
        s->fw = (int16_t)(s->pic.b.w + 2 * PAD_X);
        s->fh = (int16_t)(s->pic.b.h + 2 * PAD_Y);
        s->fx = 2;
        s->fy = (int16_t)((SCREEN_H - s->fh) / 2);
        if(s->fy < 0) s->fy = 0;
    }
}

static void prefetch_entry(DS* s) {
    const tchar* all[MAX_VIS + 6];
    int n = 0;
    all[n++] = CHARS;
    if(s->page == PG_INFO) {
        all[n++] = s->ename;
        all[n++] = s->ty[0];
        all[n++] = s->ty[1];
        if(is_seen(s->rank[s->cur])) all[n++] = s->l_cry;
    } else if(s->page == PG_AREA) { // the names of the visible rows (they share the list's name buffers: to_list loads the list's again)
        all[n++] = s->ename;
        all[n++] = s->l_area;
        all[n++] = s->l_none;
        for(int v = 0; v < MAX_VIS; v++) {
            const int k = s->ar_top + v;
            s->name[v][0] = 0;
            if(k < s->ar_n) {
                ld_rg(s->name[v], (uint16_t)(TXT_GL_AREA_VILLA_ALISO + s->ar[k][0]));
                all[n++] = s->name[v];
            }
        }
    } else {
        all[n++] = s->l_ht;
        all[n++] = s->l_wt;
        all[n++] = s->desc;
    }
    text_prefetch_many(all, n);
}

// ---------------------------------------------------------------------------------------------------- scene
static void d_enter(SceneCtx* c, void* d) {
    DS* s = d;
    load_labels(s);
    count_all(s);
    load_rows_rg(s);
    scene_dirty(c, D_WORLD);
}

static void d_leave(SceneCtx* c, void* d) {
    (void)c;
    DS* s = d;
    monpic_free(&s->pic);
    if(g_rg == s) g_rg = NULL;
    free(s);
}

static void follow(DS* s) { // keep the cursor in the visible window
    if(s->cur < s->top) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
}

static void close_screen_rg(SceneCtx* c) {
    scene_pop(c);
    scene_wake(c, 1);
}

static void to_list(SceneCtx* c, DS* s) {
    monpic_free(&s->pic);
    s->mode = M_LIST;
    follow(s);
    load_rows_rg(s);
    scene_dirty(c, D_WORLD);
}

static void to_entry(SceneCtx* c, DS* s, int page) {
    s->mode = M_ENTRY;
    s->page = (uint8_t)page;
    load_entry(s);
    if(!is_seen(s->rank[s->cur]) || (s->page == PG_DATA && !s->ent_caught)) s->page = PG_INFO;
    prefetch_entry(s);
    scene_dirty(c, D_WORLD);
}

static void set_page(SceneCtx* c, DS* s, int page) {
    s->page = (uint8_t)page;
    prefetch_entry(s);
    scene_dirty(c, D_WORLD);
}

// Left / Right in the list: a page of rows, the cursor keeps its row in the window; at the end of the list it goes to the last / first entry.
static void turn_page(SceneCtx* c, DS* s, int dir) {
    const int maxtop = s->n > s->vis ? s->n - s->vis : 0;
    int nt = s->top + dir * s->vis;
    nt = nt < 0 ? 0 : nt > maxtop ? maxtop : nt;
    int nc;
    if(nt != s->top) nc = s->cur + (nt - s->top);
    else nc = dir > 0 ? s->n - 1 : 0;
    if(nc < 0) nc = 0;
    if(nc > s->n - 1) nc = s->n - 1;
    s->top = (uint8_t)nt;
    s->cur = (uint8_t)nc;
    follow(s);
    load_rows_rg(s);
    scene_dirty(c, D_COMPOSE);
}

static void d_input(SceneCtx* c, void* d, const InputEv* ev) {
    DS* s = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) scene_clear(c);
        else if(ev->type == EV_SHORT) {
            sound_play(SND_FX_CANCEL); // (contract 28)
            if(s->mode == M_ENTRY) to_list(c, s);
            else close_screen_rg(c);
        }
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    const bool press = ev->type == EV_PRESS;
    if(s->mode == M_LIST) {
        if(!s->n) return;
        if(ev->key == KEY_UP || ev->key == KEY_DOWN) {
            s->cur = (uint8_t)((s->cur + (ev->key == KEY_DOWN ? 1 : s->n - 1)) % s->n);
            follow(s);
            load_rows_rg(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_LEFT || ev->key == KEY_RIGHT) {
            turn_page(c, s, ev->key == KEY_RIGHT ? 1 : -1);
        } else if(ev->key == KEY_OK && press && (is_seen(s->rank[s->cur]) || is_gh(s, s->rank[s->cur]))) {
            sound_play(SND_FX_CONFIRM); // (contract 28)
            to_entry(c, s, 0);
        }
        return;
    }
    // entry
    const bool seen = is_seen(s->rank[s->cur]);
    const int last = seen ? PG_AREA : PG_INFO, nxt = s->page == PG_INFO && !s->ent_caught ? PG_AREA : s->page + 1, prv = s->page == PG_AREA && !s->ent_caught ? PG_INFO : s->page - 1;
    if(ev->key == KEY_LEFT && press) {
        if(s->page > 0) set_page(c, s, prv);
        else if(seen) sound_play((uint16_t)(SND_CRY + s->rank[s->cur])); // contract 75: Left from the first page is the CRY action
    } else if(ev->key == KEY_RIGHT && press) {
        if(s->page < last) set_page(c, s, nxt);
    } else if(ev->key == KEY_OK && press) {
        sound_play(SND_FX_CONFIRM); // (contract 28)
        if(s->page < last) set_page(c, s, nxt);
        else to_list(c, s);
    } else if(ev->key == KEY_UP || ev->key == KEY_DOWN) { // the previous / next SEEN entry (no wrap); on the AREA page the rows scroll first
        const int dir = ev->key == KEY_DOWN ? 1 : -1;
        if(s->page == PG_AREA && (dir < 0 ? s->ar_top > 0 : s->ar_top + area_rows() < s->ar_n)) {
            s->ar_top = (uint8_t)(s->ar_top + dir);
            prefetch_entry(s);
            scene_dirty(c, D_COMPOSE);
            return;
        }
        int k = s->cur + dir;
        while(k >= 0 && k < s->n && !is_seen(s->rank[k])) k += dir;
        if(k < 0 || k >= s->n) return;
        const uint8_t pg = s->page;
        s->cur = (uint8_t)k;
        follow(s);
        to_entry(c, s, pg);
    }
}

// ---------------------------------------------------------------------------------------------------- drawing
static void d_draw_world(SceneCtx* c, void* d) {
    DS* s = d;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, 0);
    if(s->mode != M_ENTRY || s->page != 0 || !s->pic.blk || !s->fw) return;
    for(int y = 0; y < s->fh; y++) {
        if(y == 0 || y == s->fh - 1) lvl_span(lvl, s->fy + y, s->fx + 1, s->fx + s->fw - 1, 3); // trimmed corners
        else {
            lvl_put(lvl, s->fx, s->fy + y, 3);
            lvl_put(lvl, s->fx + s->fw - 1, s->fy + y, 3);
        }
    }
    blit_masked(lvl, &s->pic.b, s->fx + PAD_X, s->fy + PAD_Y);
}

// ASCII text in the language font (every font has 0x20-0x7E); returns the x after it.
static int put_ascii(const char* t, int x, int y) {
    for(; *t; t++) {
        font_draw_glyph((tchar)(uint8_t)*t, x, y, 1);
        x += font_adv((tchar)(uint8_t)*t);
    }
    return x;
}

static int ascii_px(const char* t) {
    int w = 0;
    for(; *t; t++) w += font_adv((tchar)(uint8_t)*t);
    return w;
}

static int tiny_y_rg(int row_y) { // tiny glyphs sit on the language font's baseline
    return row_y + font_baseline() - 5;
}

static void draw_mark(int x, int y, bool caught) {
    static const uint8_t FILLED[7] = {0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x10}; // a crystal (diamond); caught = solid with a white glint
    static const uint8_t HOLLOW[7] = {0x10, 0x28, 0x44, 0x82, 0x44, 0x28, 0x10};  // seen = the same shape as an outline
    ov_bitmap(x, y, 7, 7, caught ? FILLED : HOLLOW, 1, 1);
    if(caught) ov_fill(x + 2, y + 2, 1, 1, 0);
}

static void draw_list_rg(DS* s) {
    const int p = font_pitch();
    // header: "Visto 12        Capturado 5" (the counts in the language font; the tiny font when 3-digit counts would not fit)
    char a[8], b[8];
    hud_num(a, s->seen_n);
    hud_num(b, s->caught_n);
    const int lw1 = font_text_width(s->l_seen), lw2 = font_text_width(s->l_caught);
    const bool tiny = lw1 + 3 + ascii_px(a) + 4 + lw2 + 3 + ascii_px(b) > SCREEN_W - 2 * LIST_X_rg - 8;
    font_draw_text(s->l_seen, LIST_X_rg, 0);
    font_draw_text(s->l_caught, 121 - lw2 - 3 - (tiny ? tiny_width(b) : ascii_px(b)), 0);
    if(tiny) {
        tiny_draw(a, LIST_X_rg + lw1 + 3, tiny_y_rg(0));
        tiny_draw(b, 121 - tiny_width(b), tiny_y_rg(0));
    } else {
        put_ascii(a, LIST_X_rg + lw1 + 3, 0);
        put_ascii(b, 121 - ascii_px(b), 0);
    }
    ov_fill(2, p + 1, SCREEN_W - 4, 1, 1);
    const int y0 = p + 3, nx = LIST_X_rg + font_cell_w() + 2, nw = 3 * font_adv('0');
    for(int v = 0; v < s->vis && s->top + v < s->n; v++) {
        const int k = s->top + v, y = y0 + v * p;
        const uint8_t sp = s->rank[k];
        if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X_rg, y, 1);
        char no[8];
        hud_num(no, k + 1);
        char pad[4] = {'0', '0', '0', 0};
        const int len = (int)strlen(no);
        memcpy(pad + 3 - len, no, (size_t)len);
        put_ascii(pad, nx, y);
        font_draw_text(s->name[v], nx + nw + 4, y);
        if(is_seen(sp)) draw_mark(MARK_X, y + (font_baseline() > 7 ? font_baseline() - 7 : 0), is_caught(sp)); // (on the capitals' rows)
        if(bit(s->shy, sp)) star_draw(MARK_X - 7, tiny_y_rg(y)); // contract 65: the player owns a variocolor of it
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(122, y0, 5, 3, UP, 1, 1);
    if(s->top + s->vis < s->n) ov_bitmap(122, SCREEN_H - 4, 5, 3, DN, 1, 1);
}

// "0.9m" / "12.5kg" (tenths); "???" when the record has no value.
static void fmt_tenths(char* o, unsigned v, const char* unit) {
    if(!v) {
        strcpy(o, "???");
        return;
    }
    char t[8];
    int n = hud_num(t, (int)(v / 10));
    memcpy(o, t, (size_t)n);
    o[n++] = '.';
    o[n++] = (char)('0' + v % 10);
    strcpy(o + n, unit);
}

// 7 x 7 marks of how a species is met in a zone (contract 75): tall grass / cave floor, surfing, fishing
static const uint8_t HOW_ICON[3][7] = {{0x10, 0x54, 0x54, 0x54, 0x7C, 0x7C, 0x00}, {0x00, 0x66, 0x98, 0x00, 0x66, 0x98, 0x00}, {0x10, 0x10, 0x10, 0x10, 0x90, 0x60, 0x00}};

static void draw_area(DS* s) { // the AREA page: "Area" + the species' name, a rule, one zone per row with its how-icons at the right
    const int p = font_pitch();
    font_draw_text(s->l_area, 3, 1); // y 1, not 0: the accent of the capital A in "Área" touched the top edge of the screen (contract 78)
    font_draw_text(s->ename, SCREEN_W - 4 - font_text_width(s->ename), 1);
    ov_fill(2, p + 2, SCREEN_W - 4, 1, 1);
    const int y0 = p + 5, rows = area_rows();
    if(!s->ar_n) {
        font_draw_text(s->l_none, 4, y0);
        return;
    }
    for(int v = 0; v < rows && s->ar_top + v < s->ar_n; v++) {
        const int k = s->ar_top + v, y = y0 + v * p, how = s->ar[k][1];
        font_draw_text(s->name[v], 4, y);
        int x = 116;
        for(int b = 2; b >= 0; b--)
            if((how >> b) & 1) {
                ov_bitmap(x - 7, y + (font_baseline() > 7 ? font_baseline() - 7 : 0), 7, 7, HOW_ICON[b], 1, 1);
                x -= 9;
            }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->ar_top > 0) ov_bitmap(122, y0, 5, 3, UP, 1, 1);
    if(s->ar_top + rows < s->ar_n) ov_bitmap(122, SCREEN_H - 4, 5, 3, DN, 1, 1);
}

static void draw_entry(DS* s) {
    const int p = font_pitch();
    if(s->page == PG_AREA) {
        draw_area(s);
        return;
    }
    if(s->page == PG_INFO) {
        const int x0 = s->fw ? s->fx + s->fw + 4 : 8;
        int y = 1;
        font_draw_text(s->ename, x0, y);
        y += p;
        char no[8], num[8];
        hud_num(num, s->cur + 1);
        no[0] = 'N';
        no[1] = 'o';
        no[2] = '.';
        memset(no + 3, '0', 3);
        no[6] = 0;
        const int len = (int)strlen(num);
        memcpy(no + 6 - len, num, (size_t)len);
        put_ascii(no, x0, y);
        if(bit(s->shy, s->rank[s->cur])) star_draw(x0 + ascii_px(no) + 4, tiny_y_rg(y)); // contract 65
        y += p + 1;
        for(int i = 0; i < s->n_ty; i++) font_draw_text(s->ty[i], x0, y + i * p);
        if(is_seen(s->rank[s->cur])) { // more on the next page (the AREA page at least) and the CRY action: "< Grito" at the bottom of the text column
            static const uint8_t ARW_L[5] = {0x20, 0x60, 0xE0, 0x60, 0x20};
            font_draw_glyph(GLYPH_MORE, SCREEN_W - 3 - font_cell_w(), SCREEN_H - p, 1);
            ov_bitmap(x0, SCREEN_H - p + p / 2 - 2, 3, 5, ARW_L, 1, 1);
            font_draw_text(s->l_cry, x0 + 6, SCREEN_H - p);
        }
        return;
    }
    // page 2: height, weight, the description
    char hs[12], ws[12];
    fmt_tenths(hs, s->ent_h, "m");
    fmt_tenths(ws, s->ent_w, "kg");
    const int hx = font_text_width(s->l_ht) + 3;
    font_draw_text(s->l_ht, 3, 0);
    put_ascii(hs, 3 + hx, 0);
    const int wx = SCREEN_W - 4 - ascii_px(ws);
    put_ascii(ws, wx, 0);
    font_draw_text(s->l_wt, wx - 3 - font_text_width(s->l_wt), 0);
    ov_fill(2, p + 1, SCREEN_W - 4, 1, 1);
    int y = p + 4;
    const int rows = (SCREEN_H - y) / p;
    int used = 0;
    for(int pos = 0; pos >= 0 && used < rows; pos = text_next_page(s->desc, pos))
        for(int ln = 0; used < rows; ln++) {
            const int ls = text_line_start(s->desc, pos, ln);
            if(ls < 0) break;
            text_draw_line(s->desc, ls, 4, y, 99);
            y += p;
            used++;
        }
}

static void d_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    DS* s = d;
    if(s->mode == M_LIST) draw_list_rg(s);
    else draw_entry(s);
}

const SceneOps scene_reg_ops = {
    .enter = d_enter,
    .leave = d_leave,
    .input = d_input,
    .draw_world = d_draw_world,
    .draw_overlay = d_draw_overlay,
    .pauses_below = true,
};

// Contract 65: the Registro's star. Sets the bit of every species of which a creature stored in box.bin (the file on the card, no session needed) is a variocolor (the record's bytes: species at 0, the DVs at 3 .. 6).
// The file is read as it is: the header, then the used records two at a time (132 B); a missing / foreign file adds nothing, a damaged record at worst a wrong bit.
// (Contract 76: moved here from box.c with the Registro; the path is "<folder of save.bin>box.bin", the same derivation as box.c's bpath.)
_Static_assert(sizeof(SAVE_PATH) > 9 && sizeof(SAVE_PATH) < 112, "box paths are derived from SAVE_PATH");
#ifdef NUCLEO_DEVICE
static
#endif
void reg_scan_shiny(uint8_t* bits) { // (not static on the host: tools/host_shop/box_check.c reads an old box.bin through it)
    char path[128];
    uint8_t h[BOX_HDR], p[2 * BOX_REC];
    memcpy(path, SAVE_PATH, sizeof(SAVE_PATH) - 1 - 8); // SAVE_PATH ends with "save.bin"
    memcpy(path + sizeof(SAVE_PATH) - 1 - 8, "box.bin", 8);
    if(!read_at(path, 0, h, BOX_HDR) || memcmp(h, "NBOX", 4) != 0) return;
    const bool v1 = h[4] == 1; // (an old file nobody has migrated yet: four boxes, the records right after a 16-byte header)
    const uint32_t base = v1 ? BOX_V1_HDR : BOX_HDR;
    for(uint8_t b = 0; b < (v1 ? BOX_V1_COUNT : BOX_COUNT); b++)
        for(uint8_t i = 0; i < h[8 + b] && i < BOX_CAP; i += 2) {
            const uint8_t n = h[8 + b] - i < 2 ? (uint8_t)(h[8 + b] - i) : 2;
            if(!read_at(path, base + ((uint32_t)b * BOX_CAP + i) * BOX_REC, p, (uint32_t)n * BOX_REC)) return;
            for(const uint8_t* r = p; r < p + n * BOX_REC; r += BOX_REC)
                if(r[4] == 10 && r[5] == 10 && r[6] == 10) bits[r[0] >> 3] |= (uint8_t)(1 << (r[0] & 7));
        }
}

bool scene_reg_open(SceneCtx* c) {
    if(g_rg || !c->world || c->n >= MAX_SCENES) return false;
    uint8_t n_rec = 0;
    uint32_t off = 0;
    int n = info_header(&n_rec, &off);
    const BcData* bd = party_data();
    const bool info = n > 0;
    if(!info) n = bd && bd->n_species > 1 ? bd->n_species - 1 : 0; // no / bad reginfo.bin: the species in id order, nothing else
    DS* s = calloc(1, sizeof(DS) + (size_t)n);
    if(!s) return false;
    s->n = (uint8_t)n;
    s->info = info;
    s->n_rec = n_rec;
    if(lg_flag(c->world, LG_FLAG_RUMOR) && !is_seen(SP_GENESA)) s->gh = SP_GENESA;
    if(info && !read_at(RG_FILE, off, s->rank, (uint32_t)n)) { // the table cannot be read: the same fallback
        s->info = 0;
        const int fb = bd && bd->n_species > 1 ? bd->n_species - 1 : 0;
        s->n = (uint8_t)(fb < n ? fb : n);
    }
    if(!s->info)
        for(int k = 0; k < s->n; k++) s->rank[k] = (uint8_t)(k + 1);
    const PartyState* ps = party_state(); // contract 65: which species have a variocolor in the player's hands
    for(uint8_t k = 0; k <= ps->party.count && k <= BC_MAX_PARTY; k++) {
        const Mon* m = k < ps->party.count ? &ps->party.mon[k] : &ps->dc.mon; // (the last pass is the day care's creature)
        if(m->species && bc_shiny(m)) s->shy[m->species >> 3] |= (uint8_t)(1 << (m->species & 7));
    }
    reg_scan_shiny(s->shy);
    g_rg = s;
    if(!scene_push(c, &scene_reg_ops, s)) {
        g_rg = NULL;
        free(s);
        return false;
    }
    return true;
}

#ifndef NUCLEO_DEVICE
int scene_reg_mode(void) { return g_rg ? g_rg->mode : -1; }
int scene_reg_cur(void) { return g_rg ? g_rg->cur : -1; }
int scene_reg_top(void) { return g_rg ? g_rg->top : -1; }
int scene_reg_page(void) { return g_rg ? g_rg->page : -1; }
int scene_reg_count(void) { return g_rg ? g_rg->n : -1; }
int scene_reg_vis(void) { return g_rg ? g_rg->vis : -1; }
int scene_reg_species(int k) { return g_rg && k >= 0 && k < g_rg->n ? g_rg->rank[k] : 0; }
bool scene_reg_star(int species) { return g_rg && species > 0 && species < 256 && bit(g_rg->shy, (uint8_t)species); }
bool scene_reg_has_info(void) { return g_rg && g_rg->info; }
void scene_reg_counts(int* seen, int* caught) {
    *seen = g_rg ? g_rg->seen_n : -1;
    *caught = g_rg ? g_rg->caught_n : -1;
}
const tchar* scene_reg_row_name(int v) { return g_rg && v >= 0 && v < g_rg->vis ? g_rg->name[v] : NULL; }
int scene_reg_areas(int* area, int* how, int max) { // contract 75: the open entry's AREA rows (area index, how bits); the number of areas
    if(!g_rg) return -1;
    for(int k = 0; k < g_rg->ar_n && k < max; k++) {
        area[k] = g_rg->ar[k][0];
        how[k] = g_rg->ar[k][1];
    }
    return g_rg->ar_n;
}
int scene_reg_area_top(void) { return g_rg ? g_rg->ar_top : -1; }
int scene_reg_area_rows(void) { return area_rows(); }
const tchar* scene_reg_area_name(int v) { return g_rg && v >= 0 && v < MAX_VIS ? g_rg->name[v] : NULL; } // the name codes of the visible AREA row v
int scene_reg_entry_black(void) {
    if(!g_rg || g_rg->mode != M_ENTRY || !g_rg->pic.blk) return -1;
    for(uint32_t i = 0, n = (uint32_t)g_rg->pic.b.bpr * g_rg->pic.b.h; i < n; i++)
        if(g_rg->pic.blk[i] != 0xFF) return 0;
    return 1;
}
void scene_reg_heap(uint32_t* bytes, uint32_t* blocks) {
    *bytes = *blocks = 0;
    if(!g_rg) return;
    *bytes = (uint32_t)(sizeof(DS) + g_rg->n);
    *blocks = 1;
    if(g_rg->pic.blk) {
        *bytes += (uint32_t)(((g_rg->pic.b.w + 3) / 4 + (g_rg->pic.b.w + 7) / 8) * g_rg->pic.b.h);
        (*blocks)++;
    }
}
#endif
