// The ENDING plugin's scene (contract section 50): the Hall of Legacy (every creature of the party one by one with its picture, name and level, then the player's summary), the record in hof.bin and the save at home,
// the credits roll with the parade of creature pictures, FIN; and the Hall of Legacy browser the PC menu opens. One scene (ES), one mode at a time: M_SHOW (a run's creatures: the champion's own run, or a browsed one),
// M_SUM, M_CREDITS, M_FIN. Screens are drawn over a white background: the pictures into the gray levels (draw_world), text and tiny digits into the overlay.
#include <stdlib.h>
#include <string.h>
#include "ending_host.h"
#include "hof.h"
#include "box.h"
#include "party.h"
#include "world.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "monpic.h"
#include "hud_gfx.h"
#include "sound.h"
#include "save.h"
#include "save_enc.h"
#include "assets.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"

#define CAP 24          // codes of a name / label
#define BLK_CAP 96      // codes of a credits block (5 lines of 18 + breaks)
#define BLK_SLOTS 4     // credits blocks kept loaded (consecutive ones: item number & 3)
#define PIC_SLOTS 2     // parade pictures kept loaded (they are spaced by a text block: two at most are near the screen)
#define CR_ITEMS 40
#define CR_PICS 10      // pictures in the parade
#define SHOW_MS 3600    // a creature stays this long (OK goes on at once)
#define SUM_MS 4200
#define FIN_MS 1500     // FIN waits this long before OK ends it
#define FIN_AUTO_MS 14000
#define CR_MS_PER_PX 64 // the credits scroll one pixel per 64 ms (15.6 px/s); OK held = x4
#define GAP 7           // px between credits blocks
#define PIC_H 46        // a parade slot: the 42-px picture + 2 px each side
#define PIC_Y 11        // creature screens: the picture's top (the heading above, the name below)

enum { M_SHOW = 0, M_SUM, M_CREDITS, M_FIN };
enum { W_NONE = 0, W_CONGRATS, W_FAILED, W_CLOSE }; // what to do when the dialog above has closed
enum { IT_TEXT = 0, IT_PIC };

typedef struct {
    uint8_t kind, arg;  // IT_TEXT: arg = the index in CR_TXT; IT_PIC: arg = the species
    int16_t y, h;       // top and height on the credits canvas
} Item;

typedef struct {
    uint8_t mode, wait, browse, seq;        // browse: a Hall of Legacy browser (no record, no credits); seq: the champion's ending (record, save, credits)
    uint8_t k, run, nruns;                  // creature shown, run shown (0 = the newest), runs the file holds
    uint8_t n_items, n_pics;
    uint8_t blk_of[BLK_SLOTS];              // the item number each credits slot holds (0xFF = none)
    uint32_t t;                             // ms in this mode
    int32_t pos;                            // credits scroll, in 1/1000 px of the canvas
    int32_t height;                         // canvas height
    HofRun cur;                             // the run in view
    MonPic pic;                             // the creature in view
    MonPic par[PIC_SLOTS];                  // parade pictures
    uint8_t par_of[PIC_SLOTS];              // the item number each holds (0xFF = none)
    tchar name[CAP], l_title[CAP], l_time[CAP], l_team[CAP], who[8], fin[CAP];
    tchar tm[12];                           // "H:MM" of the run in view
    Item item[CR_ITEMS];
    tchar blk[BLK_SLOTS][BLK_CAP];
} ES;

static ES* g;
extern const SceneOps scene_ending_ops;

bool ending_active(void) {
    return g != NULL;
}

// The credits blocks in order: each key holds up to 5 lines (text breaks). Fonts: the licences of CREDITS.md, kept in every language as written by their authors.
static const uint16_t CR_TXT[] = {
    TXT_GL_C9_CR_TITLE, TXT_GL_C9_CR_SUB,  TXT_GL_C9_CR_DESIGN, TXT_GL_C9_CR_ART, TXT_GL_C9_CR_MUSIC, TXT_GL_C9_CR_FONTS, TXT_GL_C9_CR_F1, TXT_GL_C9_CR_F2,
    TXT_GL_C9_CR_F3,    TXT_GL_C9_CR_F4,   TXT_GL_C9_CR_F5,     TXT_GL_C9_CR_F6,  TXT_GL_C9_CR_F7,    TXT_GL_C9_CR_LIC,   TXT_GL_C9_CR_THANKS,
};
#define N_TXT ((int)(sizeof(CR_TXT) / sizeof(CR_TXT[0])))
// a parade picture follows block b when bit b is set (never two without a block between them)
#define PIC_AFTER 0x755Du // after blocks 0 2 3 4 6 8 10 12 13 14

// ---------------------------------------------------------------------------------------------------- helpers
static void ld(tchar* d, uint16_t id, int cap) {
    if(text_load(id, d, cap) < 0) d[0] = 0;
}

static int tiny_y(int row_y) { // tiny glyphs sit on the language font's baseline
    return row_y + font_baseline() - 5;
}

// Lines of a credits block: the text's lines of EVERY page (a page break is just another line break here: the translation checker wants at most two lines a page)
static int cr_lines(const tchar* t) {
    int n = 1;
    for(int i = 0; t[i]; i++) n += t[i] == TX_NL || t[i] == TX_PAGE;
    return n;
}

// "H:MM" as codes (the colon is in every language font, not in the tiny one)
static void fmt_time(tchar* o, uint32_t secs) {
    char t[12];
    int k = hud_num(t, (int)(secs / 3600)), n = 0;
    for(int i = 0; i < k; i++) o[n++] = (tchar)t[i];
    o[n++] = ':';
    const int m = (int)((secs / 60) % 60);
    o[n++] = (tchar)('0' + m / 10);
    o[n++] = (tchar)('0' + m % 10);
    o[n] = 0;
}

static void world_flag_on(World* w, uint16_t id) { // (a plain bit: world_flag_set is not in the plugin's symbol table, and no NPC depends on these flags before the next map load)
    w->flags[id >> 3] |= (uint8_t)(1u << (id & 7));
}

static void free_pics(ES* s) {
    monpic_free(&s->pic);
    for(int i = 0; i < PIC_SLOTS; i++) {
        monpic_free(&s->par[i]);
        s->par_of[i] = 0xFF;
    }
}

static void resume(SceneCtx* c, ES* s);
static void say(SceneCtx* c, ES* s, uint16_t id, uint8_t after) {
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) resume(c, s); // no dialog: carry on as if it had been read
}

// ---------------------------------------------------------------------------------------------------- the creature screens
// Creature k of the run in view: the picture, the name (the nickname, else the species) and the labels.
static void load_mon(ES* s) {
    const HofMon* m = &s->cur.mon[s->k];
    BoxRec r;
    memset(&r, 0, sizeof(r));
    r.mon.species = m->species;
    memcpy(r.nick, m->nick, sizeof(r.nick));
    box_name(&r, s->name, CAP);
    monpic_free(&s->pic);
    monpic_load_kind(&s->pic, m->species, BP_FRONT_MED);
    fmt_time(s->tm, s->cur.secs);
    const tchar* all[6] = {s->name, s->l_title, s->l_time, s->l_team, s->who, s->tm};
    text_prefetch_many(all, 6);
    s->t = 0;
}

// Browser: run `r` (0 = the newest). False when that run cannot be read (a damaged record).
static bool load_run(ES* s, uint8_t r) {
    HofRun h;
    if(!hof_get(r, &h)) return false;
    s->cur = h;
    s->run = r;
    s->k = 0;
    load_mon(s);
    return true;
}

// The next readable run from `from` in direction dir (+1 older, -1 newer), wrapping; false when none loads.
static bool step_run(ES* s, int dir) {
    for(int i = 1; i <= s->nruns; i++) {
        const uint8_t r = (uint8_t)((s->run + dir * i + s->nruns * 2) % s->nruns);
        if(load_run(s, r)) return true;
    }
    return false;
}

static void enter_mode(SceneCtx* c, ES* s, uint8_t mode) {
    s->mode = mode;
    s->t = 0;
    scene_dirty(c, D_WORLD);
}

// ---------------------------------------------------------------------------------------------------- the record and the save at home
// The game as it stands, but with the hero at home (the new-game place): Continuar then puts him there, champion flag set, run flags cleared (saves after the Hall of Legacy and starts in the bedroom).
static bool save_home(const World* w) {
    if(!heap_can_alloc(SAVE_LEN)) return false;
    uint8_t* b = malloc(SAVE_LEN);
    if(!b) return false;
    save_encode(b, w);
    b[5] = START_MAP;
    b[6] = START_I;
    b[7] = START_J;
    b[8] = START_FACING & 7;
    put32(b + SAVE_LEN - SAVE_TAIL, settings_crc32(b, SAVE_LEN - SAVE_TAIL));
    const bool ok = write_file(SAVE_PATH, b, SAVE_LEN);
    free(b);
    return ok;
}

// After the summary: the flags, the record (hof.bin) and the save. True = everything reached the card.
static bool record(SceneCtx* c, ES* s) {
    World* w = c->world;
    world_flag_on(w, FLAG_LEAGUE_CHAMPION);
    world_league_reset(w); // the run is over: the members can be fought again
    const bool a = hof_add(&s->cur);
    const bool b = save_home(w);
    return a && b;
}

// ---------------------------------------------------------------------------------------------------- credits
// Lays the canvas out: the blocks in order with a picture after the ones in PIC_AFTER; heights from the lines of each text.
static void credits_layout(ES* s) {
    const int p = font_pitch();
    int y = 0, np = 0;
    uint8_t par[CR_PICS];
    int npar = 0;
    const Party* pt = party_player();
    const PartyState* ps = party_state();
    for(int k = 0; k < pt->count && npar < CR_PICS; k++) par[npar++] = pt->mon[k].species; // the team first ...
    for(int sp = 1; sp < SP_COUNT && npar < CR_PICS; sp++) { // ... then the species caught, in Registro order
        if(!((ps->reg_caught[sp >> 3] >> (sp & 7)) & 1)) continue;
        bool dup = false;
        for(int k = 0; k < npar; k++) dup |= par[k] == sp;
        if(!dup) par[npar++] = (uint8_t)sp;
    }
    s->n_items = 0;
    for(int b = 0; b < N_TXT && s->n_items < CR_ITEMS - 1; b++) {
        tchar tmp[BLK_CAP];
        int lines = 1;
        if(text_load(CR_TXT[b], tmp, BLK_CAP) >= 0) lines = cr_lines(tmp);
        Item* it = &s->item[s->n_items++];
        it->kind = IT_TEXT;
        it->arg = (uint8_t)b;
        it->y = (int16_t)y;
        it->h = (int16_t)(lines * p);
        y += it->h + GAP;
        if(((PIC_AFTER >> b) & 1) && np < npar && s->n_items < CR_ITEMS - 1) {
            Item* pi = &s->item[s->n_items++];
            pi->kind = IT_PIC;
            pi->arg = par[np++];
            pi->y = (int16_t)y;
            pi->h = PIC_H;
            y += PIC_H + GAP;
        }
    }
    s->n_pics = (uint8_t)np;
    s->height = y;
    s->pos = -(int32_t)SCREEN_H * 1000; // the first block comes up from the bottom edge
    memset(s->blk_of, 0xFF, sizeof(s->blk_of));
    memset(s->par_of, 0xFF, sizeof(s->par_of));
}

// Loads / frees what the scroll position needs: text blocks near the screen, parade pictures near the screen.
static void credits_feed(ES* s) {
    const int top = (int)(s->pos / 1000);
    int pic_ord = 0;
    for(int i = 0; i < s->n_items; i++) {
        const Item* it = &s->item[i];
        const int y = it->y - top; // screen row of the item's top
        const bool near = y < SCREEN_H + 6 && y + it->h > -6;
        if(it->kind == IT_PIC) {
            const int sl = pic_ord++ & (PIC_SLOTS - 1);
            if(near && s->par_of[sl] != i) {
                monpic_free(&s->par[sl]);
                s->par_of[sl] = (uint8_t)i;
                monpic_load_kind(&s->par[sl], it->arg, BP_FRONT_MED);
            } else if(!near && s->par_of[sl] == i) {
                monpic_free(&s->par[sl]);
                s->par_of[sl] = 0xFF;
            }
            continue;
        }
        if(!near) continue;
        const int sl = it->arg & (BLK_SLOTS - 1);
        if(s->blk_of[sl] == i) continue;
        ld(s->blk[sl], CR_TXT[it->arg], BLK_CAP);
        s->blk_of[sl] = (uint8_t)i;
        text_prefetch(s->blk[sl]);
    }
}

static void credits_start(SceneCtx* c, ES* s) {
    free_pics(s);
    credits_layout(s);
    credits_feed(s);
    sound_play(END_SND_CREDITS);
    enter_mode(c, s, M_CREDITS);
}

// ---------------------------------------------------------------------------------------------------- scene
static void s_enter(SceneCtx* c, void* d) {
    ES* s = d;
    ld(s->l_title, TXT_GL_C9_HOF_TITLE, CAP);
    ld(s->l_time, TXT_GL_CARD_TIME, CAP);
    ld(s->l_team, TXT_GL_MENU_TEAM, CAP);
    ld(s->who, TXT_GL_MENU_CARD, 8); // "{player}"
    if(s->mode == M_CREDITS) {
        credits_start(c, s);
    } else {
        load_mon(s);
        if(!s->browse) sound_play(END_SND_HOF);
    }
    scene_dirty(c, D_WORLD);
    scene_wake(c, 40);
}

static void s_leave(SceneCtx* c, void* d) {
    (void)c;
    ES* s = d;
    free_pics(s);
    if(g == s) g = NULL;
    free(s);
}

static void finish(SceneCtx* c) {
    scene_pop(c);
    scene_wake(c, 1);
}

static void fin_start(SceneCtx* c, ES* s) {
    free_pics(s);
    ld(s->fin, TXT_GL_C9_CR_FIN, CAP);
    text_prefetch(s->fin);
    sound_play(END_SND_FIN);
    enter_mode(c, s, M_FIN);
}

// The summary is over (OK or its time): the congratulations, then the record.
static void after_summary(SceneCtx* c, ES* s) {
    say(c, s, TXT_GL_C9_HOF_CONGRATS, W_CONGRATS);
}

static void next_mon(SceneCtx* c, ES* s) {
    if(s->k + 1 < s->cur.n) {
        s->k++;
        load_mon(s);
        scene_dirty(c, D_WORLD);
    } else {
        enter_mode(c, s, M_SUM);
    }
}

// The dialog above has closed.
static void resume(SceneCtx* c, ES* s) {
    const uint8_t a = s->wait;
    s->wait = W_NONE;
    scene_dirty(c, D_WORLD);
    if(a == W_CONGRATS) {
        if(!record(c, s)) say(c, s, TXT_GL_C9_HOF_FAIL, W_FAILED);
        else credits_start(c, s);
    } else if(a == W_FAILED) {
        credits_start(c, s);
    } else if(a == W_CLOSE) {
        finish(c);
    }
}

static void s_input(SceneCtx* c, void* d, const InputEv* ev) {
    ES* s = d;
    if(s->wait) resume(c, s);
    if(!c->n || scene_top_ops(c) != &scene_ending_ops) return;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) {
        scene_clear(c);
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(s->browse) { // the Hall of Legacy browser: Left / Right = the run's creatures, Up / Down = the runs, Back = leave
        if(ev->key == KEY_BACK && ev->type == EV_SHORT) {
            sound_play(SND_FX_CANCEL);
            finish(c);
        } else if(press || rep) {
            if(ev->key == KEY_LEFT || ev->key == KEY_RIGHT) {
                s->k = (uint8_t)((s->k + (ev->key == KEY_RIGHT ? 1 : s->cur.n - 1)) % s->cur.n);
                load_mon(s);
                scene_dirty(c, D_WORLD);
            } else if((ev->key == KEY_UP || ev->key == KEY_DOWN) && s->nruns > 1) {
                if(step_run(s, ev->key == KEY_DOWN ? 1 : -1)) scene_dirty(c, D_WORLD);
            }
        }
        return;
    }
    if(s->mode == M_SHOW) {
        if((ev->key == KEY_OK && press) || (ev->key == KEY_BACK && ev->type == EV_SHORT)) {
            sound_play(SND_FX_CONFIRM);
            next_mon(c, s);
        }
    } else if(s->mode == M_SUM) {
        if((ev->key == KEY_OK && press) || (ev->key == KEY_BACK && ev->type == EV_SHORT)) {
            sound_play(SND_FX_CONFIRM);
            after_summary(c, s);
        }
    } else if(s->mode == M_CREDITS) {
        if(ev->key == KEY_BACK && ev->type == EV_SHORT) fin_start(c, s); // skip the roll
    } else if(s->mode == M_FIN) {
        if(ev->key == KEY_OK && press && s->t >= FIN_MS) finish(c);
    }
}

static void s_update(SceneCtx* c, void* d, uint32_t dt) {
    ES* s = d;
    if(s->wait) {
        resume(c, s);
        return;
    }
    if(s->browse) return;
    s->t += dt;
    if(s->mode == M_SHOW) {
        if(s->t >= SHOW_MS) next_mon(c, s);
        else scene_wake(c, SHOW_MS - s->t);
    } else if(s->mode == M_SUM) {
        if(s->t >= SUM_MS) after_summary(c, s);
        else scene_wake(c, SUM_MS - s->t);
    } else if(s->mode == M_CREDITS) {
        const int old = (int)(s->pos / 1000);
        const uint32_t step = (c->held & KEY_OK) ? 4u : 1u; // OK held = x4
        s->pos += (int32_t)(dt * 1000u * step / CR_MS_PER_PX);
        if((int)(s->pos / 1000) != old) {
            credits_feed(s);
            scene_dirty(c, D_WORLD);
        }
        if(s->pos / 1000 >= s->height) fin_start(c, s);
        else scene_wake(c, CR_MS_PER_PX / 2);
    } else { // M_FIN
        if(s->t >= FIN_AUTO_MS) finish(c);
        else scene_wake(c, 200);
    }
}

// ---------------------------------------------------------------------------------------------------- drawing
static void draw_world(SceneCtx* c, void* d) {
    ES* s = d;
    LvlBuf lvl = c->frame->lvl;
    lvl_fill(lvl, 0);
    if(s->mode == M_SHOW) {
        if(s->pic.blk) blit_masked(lvl, &s->pic.b, (SCREEN_W - s->pic.b.w) / 2, PIC_Y);
    } else if(s->mode == M_CREDITS) {
        const int top = (int)(s->pos / 1000);
        for(int i = 0; i < s->n_items; i++) {
            const Item* it = &s->item[i];
            if(it->kind != IT_PIC) continue;
            for(int sl = 0; sl < PIC_SLOTS; sl++)
                if(s->par_of[sl] == i && s->par[sl].blk) blit_masked(lvl, &s->par[sl].b, (SCREEN_W - s->par[sl].b.w) / 2, it->y - top + (PIC_H - s->par[sl].b.h) / 2);
        }
    }
}

static void centered(const tchar* s, int y) {
    font_draw_text(s, (SCREEN_W - font_text_width(s)) / 2, y);
}

static void draw_count(int n, int max, int x, int y, bool right) {
    char t[12];
    int k = hud_num(t, n);
    t[k++] = '/';
    hud_num(t + k, max);
    tiny_draw(t, right ? x - tiny_width(t) : x, y);
}

static void draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    ES* s = d;
    const int p = font_pitch();
    if(s->mode == M_SHOW) {
        centered(s->l_title, 0);
        font_draw_text(s->name, 3, SCREEN_H - p);
        char t[12];
        hud_level(t, s->cur.mon[s->k].level);
        tiny_draw(t, SCREEN_W - 3 - tiny_width(t), tiny_y(SCREEN_H - p));
        draw_count(s->k + 1, s->cur.n, 2, 14, false);
        if(s->browse) {
            draw_count(s->run + 1, s->nruns, SCREEN_W - 2, 14, true);
            font_draw_text(s->tm, SCREEN_W - 2 - font_text_width(s->tm), 20);
        }
    } else if(s->mode == M_SUM) {
        ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
        centered(s->l_title, 2);
        const int yn = 2 + p + 2, y = yn + p + 4, r = SCREEN_W - 12; // (12-px fonts: 2 + 12 + 2 + 12 + 4 + 12 + 1 + 12 = 57 rows)
        text_draw_line(s->who, 0, (SCREEN_W - text_line_px(s->who, 0)) / 2, yn, 99);
        font_draw_text(s->l_team, 12, y);
        char t[12];
        hud_num(t, s->cur.n);
        tiny_draw(t, r - tiny_width(t), tiny_y(y));
        font_draw_text(s->l_time, 12, y + p + 1);
        font_draw_text(s->tm, r - font_text_width(s->tm), y + p + 1);
    } else if(s->mode == M_CREDITS) {
        const int top = (int)(s->pos / 1000);
        for(int i = 0; i < s->n_items; i++) {
            const Item* it = &s->item[i];
            if(it->kind != IT_TEXT) continue;
            const int y0 = it->y - top;
            if(y0 >= SCREEN_H || y0 + it->h <= 0) continue;
            const tchar* b = s->blk[it->arg & (BLK_SLOTS - 1)];
            if(s->blk_of[it->arg & (BLK_SLOTS - 1)] != i) continue;
            int ln = 0;
            for(int pos = 0;; ln++) {
                const int y = y0 + ln * p;
                if(y > -p && y < SCREEN_H) text_draw_line(b, pos, (SCREEN_W - text_line_px(b, pos)) / 2, y, 99);
                while(b[pos] && b[pos] != TX_NL && b[pos] != TX_PAGE) pos++;
                if(!b[pos]) break;
                pos++;
            }
        }
    } else { // M_FIN
        centered(s->fin, (SCREEN_H - p) / 2);
    }
}

const SceneOps scene_ending_ops = {
    .enter = s_enter,
    .leave = s_leave,
    .input = s_input,
    .update = s_update,
    .draw_world = draw_world,
    .draw_overlay = draw_overlay,
    .pauses_below = true,
};

// ---------------------------------------------------------------------------------------------------- the plugin's entry point
bool ending_run(SceneCtx* c, uint8_t op, uint8_t arg, uint8_t* out) { // (native 22 passes op = EN_OP and arg = the sub-operation EN_RUN / EN_BROWSE / EN_CREDITS)
    *out = 0;
    if(op != EN_OP || g || !c->world || c->n + 3 > MAX_SCENES) return false;
    ES* s = calloc(1, sizeof(ES));
    if(!s) return false;
    if(arg == EN_RUN) {
        s->seq = 1;
        if(!hof_from_party(&s->cur)) goto fail;
    } else if(arg == EN_BROWSE) {
        s->browse = 1;
        s->nruns = hof_count(NULL);
        bool ok = false;
        for(uint8_t r = 0; r < s->nruns && !ok; r++) ok = hof_get(r, &s->cur) && (s->run = r, true);
        if(!ok) { // no record (or none readable): one message, no scene
            free(s);
            *out = 1;
            return scene_dialog_open(c, TXT_GL_C9_HOF_NONE, -1);
        }
    } else if(arg == EN_CREDITS) {
        s->mode = M_CREDITS;
    } else {
        goto fail;
    }
    g = s;
    if(!scene_push(c, &scene_ending_ops, s)) {
        g = NULL;
        goto fail;
    }
    *out = 1;
    return true;
fail:
    free(s);
    return false;
}

#ifndef NUCLEO_DEVICE
// host tests: what the scene shows
int scene_ending_mode(void) { return g ? g->mode : -1; }
int scene_ending_k(void) { return g ? g->k : -1; }
int scene_ending_run(void) { return g ? g->run : -1; }
int scene_ending_pos(void) { return g ? (int)(g->pos / 1000) : -1; }
int scene_ending_height(void) { return g ? (int)g->height : -1; }
int scene_ending_pics(void) { return g ? g->n_pics : -1; }
int scene_ending_state_size(void) { return (int)sizeof(ES); }
int scene_ending_loaded_pics(void) {
    int n = 0;
    if(!g) return -1;
    for(int i = 0; i < PIC_SLOTS; i++) n += g->par[i].blk != NULL;
    return n;
}
#endif
