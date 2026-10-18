// PC screen (contract section 25, 74), a screen of the PC plugin (ez_pc.fal): the "PC". "¡Encendiste el PC!", then a box Sacar / Dejar / Liberar /
// Cambiar caja / Dejar obj. / Sacar obj. / Salir. 12 boxes of 20 (Cambiar caja scrolls); the two item rows are PC "Deposit / Withdraw item" (50 kinds of up to 99, with a
// quantity box; "Toss item" is the bag's own Tirar once the item is back in it). The lists show name + tiny level (or the count xN); Liberar asks YES / NO; leaving saves save.bin and box.bin together (box.c). The whole visit is a
// transaction on a copy of box.bin (box_begin ... box_commit): Back long (exit the app) throws it away, a save that fails twice puts the party and the bag back.
// scene_box_store_open() is the same machinery without a menu: the pending capture goes to a box, the game is saved, one message, done.
// Layout and idioms are scene_shop.c's (white screen, 1-px frame, list rows with the cursor glyph, scroll arrows at x 121, dialogs above).
#include <stdlib.h>
#include <string.h>
#include "scene_box.h"
#include "scene_card.h"
#include "pc_host.h"
#include "box.h"
#include "party.h"
#include "bag.h"
#include "text.h"
#include "font.h"
#include "frame.h"
#include "lvl.h"
#include "hud_gfx.h"
#include "gen/maps_gen.h"
#include "gen/reg_gen.h"

#define CAP 16       // codes of a name / label
#define MENU_CAP 18  // codes of a menu label: the Hall of Legacy row's "Salon del Legado" has 16 + END
#define VIS_MAX 6    // rows kept loaded (the list shows (64 - 2 - header) / pitch: 4 / 4 / 3)
#define LIST_X 3     // cursor x
#define RIGHT_R 118  // right edge of the tiny level (the scroll arrows live at 121..125)
#define N_ITEMS 9    // C9 (contract 50.4) + 74: Sacar, Dejar, Liberar, Cambiar caja, Dejar obj., Sacar obj., Tirar obj., [Salon del Legado], Salir; the Hall of Legacy row only exists once the league is won (flag league_champion)

enum { M_MENU = 0, M_TAKE, M_PUT, M_FREE, M_BOXES, M_STORE, M_IDEP, M_IWD, M_IQTY }; // (contract 74: the two item lists, the quantity box over the list that asked)
// what to do when the child above (dialog, YES/NO) has closed
enum { W_NONE = 0, W_ON, W_MENU, W_LIST, W_ASK, W_CLOSE };

typedef struct {
    uint8_t mode, cur, top, vis, wait, ask, mcur, changed, fails, open, sel;
    uint8_t mtop, nmenu, hof_go; // the menu's first visible row (a window when the rows do not fit the screen), its rows (6, or 7 with the Hall of Legacy), "leave for the Hall of Legacy browser"
    uint8_t lmode, item, qty, max, toss; // contract 74: the item list the quantity box came from, the item and the quantity being chosen (max = the most that may move), toss = the storage list is for Tirar obj.
    BoxSnap snap;                // the party / bag / pending state when the visit began (put back when the save fails for good)
    tchar nm[CAP];               // name of the creature being acted on
    tchar lab[VIS_MAX][CAP];     // names of rows top .. top + vis - 1
    uint8_t lv[VIS_MAX];         // their levels (0 = a record that is not a creature) or, in an item list, their counts
    tchar l_menu[N_ITEMS][MENU_CAP], l_box[CAP], l_cancel[CAP], l_team[CAP], l_title[CAP];
    tchar l_items[CAP], l_bag[CAP]; // contract 74: "Objetos" (the storage's header), "Mochila" (the bag's)
} BS;

static BS* g;

bool scene_box_active(void) {
    return g != NULL;
}

static const tchar GLYPHS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '/', ' ', '?', 0}; // prefetched (zh streams its glyphs)

#ifndef NUCLEO_DEVICE
static uint16_t g_last_msg; // host tests: the text id of the last message the screen showed
#endif

// ---------------------------------------------------------------------------------------------------- data
static void ld(tchar* d, uint16_t id) {
    if(text_load(id, d, CAP) < 0) d[0] = 0;
}

static uint8_t lmod(const BS* s) { // the list on the screen (the quantity box sits over the one that asked)
    return s->mode == M_IQTY ? s->lmode : s->mode;
}

static bool is_item_list(uint8_t m) {
    return m == M_IDEP || m == M_IWD;
}

static int count_rows(const BS* s) { // list rows: creatures / items + Cancelar
    const uint8_t m = lmod(s);
    return (m == M_PUT ? party_player()->count : m == M_IDEP ? bag_kinds() : m == M_IWD ? box_it_n() : box_n(box_cur())) + 1;
}

// "Caja 2" as codes (the label + a space + the box number: 12 boxes, two digits from the tenth).
static void make_title(BS* s, uint8_t b) {
    int n = 0;
    const int num = b + 1;
    while(s->l_box[n] && n < CAP - 4) {
        s->l_title[n] = s->l_box[n];
        n++;
    }
    s->l_title[n++] = ' ';
    if(num >= 10) s->l_title[n++] = '1';
    s->l_title[n++] = (tchar)('0' + (num >= 10 ? num - 10 : num));
    s->l_title[n] = 0;
}

static void load_rows(BS* s) {
    const tchar* all[VIS_MAX + 7];
    int n = 0;
    const int items = count_rows(s) - 1;
    for(int v = 0; v < s->vis; v++) {
        const int k = s->top + v;
        s->lab[v][0] = 0;
        s->lv[v] = 0;
        if(k < items) {
            if(is_item_list(lmod(s))) { // contract 74: the bag's or the storage's item k
                uint8_t it = 0, cnt = 0;
                if((lmod(s) == M_IDEP ? bag_get((uint8_t)k, &it, &cnt) : box_it_get((uint8_t)k, &it, &cnt)) && text_load(reg_it_txt(it), s->lab[v], CAP) >= 0) s->lv[v] = cnt;
                else s->lab[v][0] = 0;
            } else if(s->mode == M_PUT) {
                party_mon_name((uint8_t)k, s->lab[v], CAP);
                s->lv[v] = party_player()->mon[k].level;
            } else {
                BoxRec r;
                if(box_get(box_cur(), (uint8_t)k, &r)) {
                    box_name(&r, s->lab[v], CAP);
                    s->lv[v] = r.mon.level;
                } else {
                    s->lab[v][0] = s->lab[v][1] = s->lab[v][2] = '?';
                    s->lab[v][3] = 0;
                }
            }
        }
        all[n++] = s->lab[v];
    }
    all[n++] = s->l_cancel;
    all[n++] = s->l_title;
    all[n++] = s->l_team;
    all[n++] = s->l_items;
    all[n++] = s->l_bag;
    all[n++] = GLYPHS;
    text_prefetch_many(all, n);
}

static void clamp(BS* s) {
    const int n = count_rows(s);
    if(s->cur >= n) s->cur = (uint8_t)(n - 1);
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + s->vis) s->top = (uint8_t)(s->cur - s->vis + 1);
    if(n <= s->vis) s->top = 0;
    else if(s->top + s->vis > n) s->top = (uint8_t)(n - s->vis);
    load_rows(s);
}

static int bvis(void) { // rows of the box list (Cambiar caja) that fit the screen: no header, 12 boxes
    const int v = (SCREEN_H - 2) / font_pitch();
    return v < 1 ? 1 : v;
}

static void bclamp(BS* s) { // the box list's window follows the cursor
    const int v = bvis();
    if(s->top > s->cur) s->top = s->cur;
    if(s->cur >= s->top + v) s->top = (uint8_t)(s->cur - v + 1);
    if(s->top + v > BOX_COUNT) s->top = (uint8_t)(BOX_COUNT > v ? BOX_COUNT - v : 0);
}

// ---------------------------------------------------------------------------------------------------- scene
static int list_y0(void) { // first list row: below the header and its rule
    return font_pitch() + 5;
}

static void s_enter(SceneCtx* c, void* d) {
    BS* s = d;
    static const uint16_t MENU_TXT[N_ITEMS] = {TXT_GL_BOX_TAKE, TXT_GL_BOX_PUT, TXT_GL_BOX_FREE, TXT_GL_BOX_CHANGE, TXT_GL_F2_BOX_DEP_IT, TXT_GL_F2_BOX_WD_IT, TXT_GL_F2_BOX_TOSS_IT, TXT_GL_C9_HOF_MENU, TXT_GL_BOX_QUIT};
    for(int i = 0; i < N_ITEMS; i++)
        if(text_load(MENU_TXT[i], s->l_menu[i], MENU_CAP) < 0) s->l_menu[i][0] = 0;
    s->nmenu = (c->world->flags[FLAG_LEAGUE_CHAMPION >> 3] >> (FLAG_LEAGUE_CHAMPION & 7)) & 1 ? N_ITEMS : N_ITEMS - 1;
    ld(s->l_box, TXT_GL_BOX_NAME);
    ld(s->l_cancel, TXT_GL_PARTY_CANCEL);
    ld(s->l_team, TXT_GL_MENU_TEAM);
    ld(s->l_items, TXT_GL_F2_BOX_ITEMS);
    ld(s->l_bag, TXT_GL_MENU_BAG);
    make_title(s, box_cur());
    s->vis = (uint8_t)((SCREEN_H - 2 - list_y0()) / font_pitch());
    if(s->vis > VIS_MAX) s->vis = VIS_MAX;
    if(s->vis < 1) s->vis = 1;
    const tchar* all[N_ITEMS + 3];
    int n = 0;
    for(int i = 0; i < N_ITEMS; i++) all[n++] = s->l_menu[i];
    all[n++] = s->l_title;
    all[n++] = GLYPHS;
    text_prefetch_many(all, n);
    scene_dirty(c, D_WORLD);
}

static void s_leave(SceneCtx* c, void* d) {
    (void)c;
    BS* s = d;
    if(s->open) box_abort(); // the visit was cut short (Back long): nothing of it reaches the card
    if(g == s) g = NULL;
    free(s);
}

static void close_screen(SceneCtx* c) {
    if(g && g->hof_go) c->world->flags[FLAG_LEAGUE_HOF_REQ >> 3] |= (uint8_t)(1u << (FLAG_LEAGUE_HOF_REQ & 7)); // C9 (contract 50.4): the script (mapc's `pc`) opens the Hall of Legacy browser after this screen
    scene_pop(c);
    scene_wake(c, 1);
}

static void say(SceneCtx* c, BS* s, uint16_t id, uint8_t after) {
#ifndef NUCLEO_DEVICE
    g_last_msg = id;
#endif
    s->wait = after;
    if(!scene_dialog_open(c, id, -1)) { // no dialog: carry on as if it had been read
        s->wait = W_NONE;
        if(after == W_CLOSE) close_screen(c);
        else if(after == W_MENU) s->mode = M_MENU;
    }
}

static void go_menu(BS* s) {
    s->mode = M_MENU;
}

static void go_list(BS* s, uint8_t mode) {
    s->mode = mode;
    clamp(s);
}

// A box operation failed on the card: the visit is over, the party (and the bag) go back to how they were, the card keeps its old files.
static void io_fail(SceneCtx* c, BS* s) {
    s->hof_go = 0;
    box_snap_restore(&s->snap);
    box_abort();
    s->open = 0;
    s->changed = 0;
    say(c, s, TXT_GL_SAVE_FAIL, W_CLOSE);
}

static void name_to_var(const tchar* nm, int box) {
    text_set_var(0, nm);
    if(box) text_set_var_num(2, box);
}

// Leaving the PC: save the game and the boxes together (only when something changed: "the box was saved").
static void leave_pc(SceneCtx* c, BS* s) {
    if(!s->changed) {
        box_abort();
        s->open = 0;
        close_screen(c);
        return;
    }
    if(box_commit(c->world)) {
        s->open = 0;
        s->changed = 0;
        say(c, s, TXT_GL_SAVE_DONE, W_CLOSE);
        return;
    }
    if(++s->fails >= 2) { // the second failure: give the party back and go (a stuck card must not trap the player)
        io_fail(c, s);
        return;
    }
    s->hof_go = 0;
    say(c, s, TXT_GL_SAVE_FAIL, W_MENU); // the visit goes on: Salir tries again
}

// The pending capture into the session's boxes (the greeting has been read).
static void store_pending(SceneCtx* c, BS* s, uint8_t after) {
    BoxRec t;
    if(!party_pending_get(&t.mon)) {
        s->mode = M_MENU;
        return;
    }
    memcpy(t.nick, party_pending_nick(), sizeof(t.nick));
    t.nick[PARTY_NICK_CAP - 1] = 0;
    box_name(&t, s->nm, CAP);
    const int r = box_store_pending();
    if(r < 0) {
        io_fail(c, s);
        return;
    }
    make_title(s, box_cur());
    name_to_var(s->nm, r);
    if(r > 0) s->changed = 1;
    say(c, s, r > 0 ? TXT_GL_BOX_SENT : TXT_GL_BOX_ALL_FULL, after);
}

static void item_vars(const BS* s);
// The child above (dialog, YES/NO) has closed.
static void resume(SceneCtx* c, BS* s) {
    const uint8_t a = s->wait;
    s->wait = W_NONE;
    scene_dirty(c, D_WORLD);
    if(a == W_CLOSE) {
        close_screen(c);
    } else if(a == W_ON) {
        go_menu(s);
        if(party_pending_get(NULL)) store_pending(c, s, W_MENU); // a capture that could not be stored before (card error): now
    } else if(a == W_ASK && s->toss) { // Tirar obj.: YES takes the items out of the storage for good
        if(!s->ask) {
            go_list(s, M_IWD);
        } else if(!box_it_take(s->item, s->qty)) {
            io_fail(c, s);
        } else {
            s->changed = 1;
            item_vars(s);
            say(c, s, TXT_GL_BAG_TOSSED, W_LIST);
        }
    } else if(a == W_ASK) {
        if(s->ask) { // YES: release it
            if(!box_remove(box_cur(), s->sel)) {
                io_fail(c, s);
                return;
            }
            s->changed = 1;
            name_to_var(s->nm, 0);
            say(c, s, TXT_GL_BOX_FREED, W_MENU);
        } else {
            go_list(s, M_FREE);
        }
    } else if(a == W_LIST) {
        go_list(s, s->mode);
    } else {
        go_menu(s); // W_MENU
    }
}

// OK on an item row of a list.
static void pick(SceneCtx* c, BS* s) {
    const uint8_t k = s->cur, b = box_cur();
    BoxRec r;
    if(s->mode == M_TAKE) {
        if(!box_get(b, k, &r)) return; // not a creature: Liberar can remove it
        if(party_player()->count >= BC_MAX_PARTY) {
            say(c, s, TXT_GL_BOX_PARTY_FULL, W_LIST);
            return;
        }
        box_name(&r, s->nm, CAP);
        box_to_party(&r);
        if(!box_remove(b, k)) {
            io_fail(c, s);
            return;
        }
        s->changed = 1;
        name_to_var(s->nm, 0);
        say(c, s, TXT_GL_BOX_TOOK, W_MENU);
    } else if(s->mode == M_PUT) {
        if(party_player()->count <= 1) {
            say(c, s, TXT_GL_BOX_LAST, W_LIST);
            return;
        }
        if(box_n(b) >= BOX_CAP) {
            say(c, s, TXT_GL_BOX_FULL, W_LIST);
            return;
        }
        party_mon_name(k, s->nm, CAP);
        box_from_party(&r, k);
        if(!box_add(b, &r)) {
            io_fail(c, s);
            return;
        }
        box_party_drop(k);
        s->changed = 1;
        name_to_var(s->nm, b + 1);
        say(c, s, TXT_GL_BOX_SENT, W_MENU);
    } else { // M_FREE: ask first
        if(box_get(b, k, &r)) box_name(&r, s->nm, CAP);
        else {
            s->nm[0] = s->nm[1] = s->nm[2] = '?';
            s->nm[3] = 0;
        }
        s->sel = k;
        s->ask = 0;
        name_to_var(s->nm, 0);
        s->wait = W_ASK;
        if(!scene_ask_open(c, TXT_GL_BOX_ASK_FREE, &s->ask)) {
            s->wait = W_NONE;
            go_list(s, M_FREE);
        }
    }
    scene_dirty(c, D_COMPOSE);
}

// ---- the item storage (contract 74)
static void item_vars(const BS* s) { // {v0} = the item, {v2} = the quantity
    text_set_var_id(0, reg_it_txt(s->item));
    text_set_var_num(2, s->qty);
}

// The quantity is chosen (or there is only one): deposit or withdraw. The destination clips it (99 a kind, 50 kinds in the storage, 20 in the bag): how many really moved is what the message says, none = no room.
static void item_go(SceneCtx* c, BS* s) {
    s->mode = s->lmode;
    if(s->toss) { // Toss item: a key item stays, anything else asks YES / NO first (the bag's own texts)
        if(party_data()->items[s->item].kind == IK_KEY) {
            say(c, s, TXT_GL_BAG_CANT_TOSS, W_LIST);
            return;
        }
        item_vars(s);
        s->ask = 0;
        s->wait = W_ASK;
        if(!scene_ask_open(c, TXT_GL_BAG_TOSS_ASK, &s->ask)) {
            s->wait = W_NONE;
            go_list(s, M_IWD);
        }
        return;
    }
    const bool dep = s->lmode == M_IDEP;
    int r = dep ? box_it_add(s->item, s->qty) : bag_add(s->item, s->qty); // (the storage first, the file; or the bag first, RAM: a failed write ends the visit and the snapshot puts the bag back)
    if(r > 0 && !dep && !box_it_take(s->item, (uint8_t)r)) r = -1;
    if(r < 0) {
        io_fail(c, s);
        return;
    }
    if(!r) {
        say(c, s, dep ? TXT_GL_F2_BOX_NOROOM : TXT_GL_FOUND_FULL, W_LIST);
        return;
    }
    for(int i = 0; dep && i < r; i++) bag_take(s->item);
    s->qty = (uint8_t)r;
    s->changed = 1;
    item_vars(s);
    say(c, s, dep ? TXT_GL_F2_BOX_DEP : TXT_GL_F2_BOX_WD, W_LIST);
}

// OK on an item row of M_IDEP / M_IWD: the quantity box (or at once when there is only one).
static void item_pick(SceneCtx* c, BS* s) {
    uint8_t it = 0, n = 0;
    if(s->mode == M_IDEP ? !bag_get(s->cur, &it, &n) : !box_it_get(s->cur, &it, &n)) return;
    s->item = it;
    s->max = n;
    s->qty = 1;
    s->lmode = s->mode;
    s->mode = M_IQTY;
    if(n == 1) item_go(c, s);
}

static int menu_rows(const BS* s) { // rows the framed menu shows at once: all of them, or what the 64-px screen holds (5 rows of the 11 - 12 px fonts)
    const int fit = (SCREEN_H - 4) / font_pitch();
    return s->nmenu < fit ? s->nmenu : fit;
}

static int mitem(const BS* s, int row) { // the item a menu row shows: without the Hall of Legacy row 6 is Salir
    return row == 7 && s->nmenu < N_ITEMS ? 8 : row;
}

static void menu_ok(SceneCtx* c, BS* s) {
    const int it = mitem(s, s->mcur);
    switch(it) {
    case 0: // Sacar
    case 2: // Liberar
        if(box_n(box_cur()) == 0) {
            say(c, s, TXT_GL_BOX_EMPTY, W_MENU);
        } else {
            s->cur = s->top = 0;
            go_list(s, it == 0 ? M_TAKE : M_FREE);
        }
        break;
    case 1: // Dejar
        if(party_player()->count <= 1) say(c, s, TXT_GL_BOX_LAST, W_MENU);
        else if(box_n(box_cur()) >= BOX_CAP) say(c, s, TXT_GL_BOX_FULL, W_MENU);
        else {
            s->cur = s->top = 0;
            go_list(s, M_PUT);
        }
        break;
    case 3: // Cambiar caja
        s->mode = M_BOXES;
        s->cur = box_cur();
        s->top = 0;
        bclamp(s);
        break;
    case 4: // Dejar obj. / Sacar obj. / Tirar obj. (contract 74): the bag's list / the storage's list (twice)
    case 5:
    case 6:
        if(it == 4 ? bag_kinds() == 0 : box_it_n() == 0) {
            say(c, s, it == 4 ? TXT_GL_F2_BOX_NONE_BAG : TXT_GL_F2_BOX_NONE_PC, W_MENU);
        } else {
            s->cur = s->top = 0;
            s->toss = it == 6;
            go_list(s, (uint8_t)(M_IDEP + (it > 4)));
        }
        break;
    case 7: // Salon del Legado (C9): leave the PC (saving what changed); the script opens the browser once this screen is gone
        s->hof_go = 1;
        leave_pc(c, s);
        break;
    default:
        leave_pc(c, s);
        break;
    }
    scene_dirty(c, D_COMPOSE);
}

static void s_input(SceneCtx* c, void* d, const InputEv* ev) {
    BS* s = d;
    if(s->wait) resume(c, s);
    if(!c->n || scene_top_ops(c) != &scene_box_ops || s->mode == M_STORE) return;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG) {
            scene_clear(c);
        } else if(ev->type == EV_SHORT) {
            if(s->mode == M_MENU) leave_pc(c, s);
            else if(s->mode == M_IQTY) s->mode = s->lmode; // back to the item list
            else go_menu(s);
            scene_dirty(c, D_COMPOSE);
        }
        return;
    }
    const bool press = ev->type == EV_PRESS, rep = ev->type == EV_REPEAT;
    if(!press && !rep) return;
    const bool up = ev->key == KEY_UP, down = ev->key == KEY_DOWN;
    if(s->mode == M_MENU) {
        if(up || down) {
            s->mcur = (uint8_t)((s->mcur + (down ? 1 : s->nmenu - 1)) % s->nmenu);
            const int mv = menu_rows(s);
            if(s->mcur < s->mtop) s->mtop = s->mcur;
            if(s->mcur >= s->mtop + mv) s->mtop = (uint8_t)(s->mcur - mv + 1);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            menu_ok(c, s);
        }
    } else if(s->mode == M_BOXES) {
        if(up || down) {
            s->cur = (uint8_t)((s->cur + (down ? 1 : BOX_COUNT - 1)) % BOX_COUNT);
            bclamp(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->cur != box_cur()) {
                if(!box_set_cur(s->cur)) {
                    io_fail(c, s);
                    return;
                }
                s->changed = 1;
                make_title(s, s->cur);
            }
            go_menu(s);
            scene_dirty(c, D_COMPOSE);
        }
    } else if(s->mode == M_IQTY) { // Up / Down +-1 (wrap), Left / Right +-10, like the bag's Tirar
        int q = s->qty;
        if(up || down) q = up ? (q >= s->max ? 1 : q + 1) : (q <= 1 ? s->max : q - 1);
        else if(ev->key == KEY_RIGHT) q = q + 10 > s->max ? s->max : q + 10;
        else if(ev->key == KEY_LEFT) q = q - 10 < 1 ? 1 : q - 10;
        else if(ev->key == KEY_OK && press) {
            item_go(c, s);
            scene_dirty(c, D_COMPOSE);
            return;
        }
        if(q != s->qty) {
            s->qty = (uint8_t)q;
            scene_dirty(c, D_COMPOSE);
        }
    } else {
        const int n = count_rows(s);
        if(up || down) {
            s->cur = (uint8_t)((s->cur + (down ? 1 : n - 1)) % n);
            clamp(s);
            scene_dirty(c, D_COMPOSE);
        } else if(ev->key == KEY_OK && press) {
            if(s->cur == n - 1) { // Cancelar
                go_menu(s);
                scene_dirty(c, D_COMPOSE);
            } else if(is_item_list(s->mode)) {
                item_pick(c, s);
                scene_dirty(c, D_COMPOSE);
            } else {
                pick(c, s);
            }
        }
    }
}

static void s_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)dt;
    BS* s = d;
    if(s->wait) resume(c, s);
}

// ---------------------------------------------------------------------------------------------------- drawing
static void draw_world(SceneCtx* c, void* d) {
    (void)d;
    lvl_fill(c->frame->lvl, 0);
}

static int tiny_y(int row_y) { // tiny glyphs sit on the language font's baseline
    return row_y + font_baseline() - 5;
}

// "7/20" in the tiny font, right edge at r.
static void draw_count(int n, int max, int r, int y) {
    char t[12];
    int k = hud_num(t, n);
    t[k++] = '/';
    hud_num(t + k, max);
    tiny_draw(t, r - tiny_width(t), y);
}

static void draw_menu(BS* s) {
    const int p = font_pitch();
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    int w = 0;
    for(int r = 0; r < s->nmenu; r++) {
        const int wi = font_text_width(s->l_menu[mitem(s, r)]);
        if(wi > w) w = wi;
    }
    const int mv = menu_rows(s), bw = font_cell_w() + 9 + w + 4 + (mv < s->nmenu ? 7 : 0); // (a window: room for its arrows at the right)
    if(!s->wait) { // (the greeting and the messages are dialogs over the bare screen)
        const int bh = mv * p + 4;
        ov_fill(0, 0, bw, bh, 0);
        ov_frame(0, 0, bw, bh, 1);
        for(int i = 0; i < mv; i++) {
            const int y = 2 + i * p, r = s->mtop + i;
            if(s->mcur == r) font_draw_glyph(GLYPH_CURSOR, 3, y, 1);
            font_draw_text(s->l_menu[mitem(s, r)], 3 + font_cell_w() + 2, y);
        }
        static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20}; // the window of a menu longer than the screen
        if(s->mtop > 0) ov_bitmap(bw - 8, 3, 5, 3, UP, 1, 1);
        if(s->mtop + mv < s->nmenu) ov_bitmap(bw - 8, bh - 6, 5, 3, DN, 1, 1);
    }
    // the current box, top right (when the menu is not wide enough to reach it: the Hall of Legacy row can be)
    const int tw = font_text_width(s->l_title);
    if(bw + 2 > SCREEN_W - 4 - tw) return;
    font_draw_text(s->l_title, SCREEN_W - 4 - tw, 2);
    draw_count(box_n(box_cur()), BOX_CAP, SCREEN_W - 4, 2 + p);
}

static void draw_list(BS* s) {
    const int p = font_pitch(), n = count_rows(s), nx = LIST_X + font_cell_w() + 2, y0 = list_y0();
    const uint8_t m = lmod(s);
    const bool party = m == M_PUT, item = is_item_list(m);
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    font_draw_text(party ? s->l_team : m == M_IDEP ? s->l_bag : item ? s->l_items : s->l_title, LIST_X, 2);
    draw_count(n - 1, party ? BC_MAX_PARTY : m == M_IDEP ? BAG_MAX_KINDS : item ? BOX_IT_N : BOX_CAP, SCREEN_W - 4, tiny_y(2));
    ov_fill(1, p + 3, SCREEN_W - 2, 1, 1);
    for(int v = 0; v < s->vis && s->top + v < n; v++) {
        const int k = s->top + v, y = y0 + v * p;
        if(k == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X, y, 1);
        if(k == n - 1) {
            font_draw_text(s->l_cancel, nx, y);
            continue;
        }
        font_draw_text(s->lab[v], nx, y);
        if(s->lv[v]) {
            char t[12];
            if(item) {
                t[0] = 'x';
                hud_num(t + 1, s->lv[v]);
            } else {
                hud_level(t, s->lv[v]);
            }
            tiny_draw(t, RIGHT_R - tiny_width(t), tiny_y(y));
        }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    if(s->top > 0) ov_bitmap(121, y0 + 1, 5, 3, UP, 1, 1);
    if(s->top + s->vis < n) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
}

// "x05" with a pair of small arrows (the bag's quantity box): how many to move.
static void draw_qty(BS* s) {
    const int p = font_pitch(), adv = font_adv('0');
    const int w = 4 + 3 + 2 + 2 * adv + 4 + 5 + 3, h = p + 4;
    const int x = SCREEN_W - w, y = SCREEN_H - h;
    ov_fill(x, y, w, h, 0);
    ov_frame(x, y, w, h, 1);
    tiny_draw("x", x + 4, y + 2 + font_baseline() - 5);
    const int dx = x + 4 + 3 + 2;
    font_draw_glyph((tchar)('0' + s->qty / 10), dx, y + 2, 1);
    font_draw_glyph((tchar)('0' + s->qty % 10), dx + adv, y + 2, 1);
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20};
    ov_bitmap(x + w - 9, y + 3, 5, 3, UP, 1, 1);
    ov_bitmap(x + w - 9, y + h - 6, 5, 3, DN, 1, 1);
}

static void draw_boxes(BS* s) {
    const int p = font_pitch(), nx = LIST_X + font_cell_w() + 2, v = bvis();
    ov_frame(0, 0, SCREEN_W, SCREEN_H, 1);
    for(int i = 0; i < v && s->top + i < BOX_COUNT; i++) {
        const int b = s->top + i, y = 2 + i * p;
        make_title(s, (uint8_t)b);
        if(b == s->cur) font_draw_glyph(GLYPH_CURSOR, LIST_X, y, 1);
        font_draw_text(s->l_title, nx, y);
        draw_count(box_n((uint8_t)b), BOX_CAP, RIGHT_R, tiny_y(y));
        if(b == box_cur()) { // the box that is open: a small marker after the label
            static const uint8_t MARK[3] = {0x40, 0xE0, 0x40};
            ov_bitmap(nx + font_text_width(s->l_title) + 4, y + font_baseline() - 4, 3, 3, MARK, 1, 1);
        }
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8}, DN[3] = {0xF8, 0x70, 0x20}; // 12 boxes: the window's arrows
    if(s->top > 0) ov_bitmap(121, 3, 5, 3, UP, 1, 1);
    if(s->top + v < BOX_COUNT) ov_bitmap(121, SCREEN_H - 6, 5, 3, DN, 1, 1);
    make_title(s, box_cur());
}

static void draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    BS* s = d;
    if(s->mode == M_MENU || s->mode == M_STORE) draw_menu(s);
    else if(s->mode == M_BOXES) draw_boxes(s);
    else draw_list(s);
    if(s->mode == M_IQTY) draw_qty(s);
}

const SceneOps scene_box_ops = {
    .enter = s_enter,
    .leave = s_leave,
    .input = s_input,
    .update = s_update,
    .draw_world = draw_world,
    .draw_overlay = draw_overlay,
    .pauses_below = true,
};

// The state + the working copy + the scene. The store scene is pushed even when the copy cannot be made (s->open stays 0): it then only says
// that it could not save, which is more than a creature that silently stays pending.
static BS* begin_visit(SceneCtx* c, uint8_t mode) {
    if(g || !c->world || c->n + 3 > MAX_SCENES) return NULL; // room for the dialog + YES/NO above us
    BS* s = calloc(1, sizeof(BS));
    if(!s) return NULL;
    s->open = box_begin();
    if(!s->open && mode != M_STORE) {
        free(s);
        return NULL;
    }
    s->mode = mode;
    box_snap_take(&s->snap);
    g = s;
    if(!scene_push(c, &scene_box_ops, s)) {
        g = NULL;
        if(s->open) box_abort();
        free(s);
        return NULL;
    }
    return s;
}

bool scene_box_open(SceneCtx* c) {
    BS* s = begin_visit(c, M_MENU);
    if(!s) return false;
    say(c, s, TXT_GL_BOX_ON, W_ON);
    return true;
}

bool scene_box_store_open(SceneCtx* c) {
    if(!party_pending_get(NULL)) return false;
    BS* s = begin_visit(c, M_STORE);
    if(!s) return false;
    if(!s->open) { // the card cannot be written
        say(c, s, TXT_GL_SAVE_FAIL, W_CLOSE);
        return true;
    }
    BoxRec t;
    party_pending_get(&t.mon);
    memcpy(t.nick, party_pending_nick(), sizeof(t.nick));
    t.nick[PARTY_NICK_CAP - 1] = 0;
    box_name(&t, s->nm, CAP);
    const int r = box_store_pending();
    uint16_t msg = TXT_GL_SAVE_FAIL;
    if(r >= 0) {
        name_to_var(s->nm, r);
        msg = r > 0 ? TXT_GL_BOX_SENT : TXT_GL_BOX_ALL_FULL;
        if(r > 0 && box_commit(c->world)) {
            s->open = 0;
        } else if(r > 0) { // the save failed: the creature stays pending (the next PC visit stores it)
            msg = TXT_GL_SAVE_FAIL;
            box_snap_restore(&s->snap);
        }
    } else {
        box_snap_restore(&s->snap);
    }
    if(s->open) {
        box_abort();
        s->open = 0;
    }
    say(c, s, msg, W_CLOSE);
    return true;
}

// The PC plugin's entry points (pc_host.h): what = PC_OPEN_CARD / PC_OPEN_BOX / PC_OPEN_STORE / PC_OPEN_LIST + var (the card was the casino plugin's from contract 74 to 76; the Registro, once PC_OPEN_REG, is the casino plugin's since contract 76).
bool scene_pc_open(SceneCtx* c, uint8_t what) {
    if(what >= PC_OPEN_LIST) return scene_list_open(c, (uint8_t)(what - PC_OPEN_LIST)); // C4-A: the elevator's floor list (contract 35.5)
    return what == PC_OPEN_CARD    ? scene_card_open(c) // (contract 76: back from the casino plugin)
           : what == PC_OPEN_BOX   ? scene_box_open(c)
           : what == PC_OPEN_STORE ? scene_box_store_open(c)
                                   : false; // (the Registro, once what = 3, is the casino plugin's since contract 76)
}

bool scene_pc_active(void) {
    return scene_box_active() || scene_card_active() || scene_list_active();
}

#ifndef NUCLEO_DEVICE
int scene_box_mode(void) { return g ? g->mode : -1; }
int scene_box_mcur(void) { return g ? g->mcur : -1; }
int scene_box_nmenu(void) { return g ? g->nmenu : -1; }
int scene_box_cur(void) { return g ? g->cur : -1; }
int scene_box_top(void) { return g ? g->top : -1; }
int scene_box_qty(void) { return g ? g->qty : -1; }
int scene_box_last_msg(void) { return g_last_msg; }
void scene_box_heap(uint32_t* bytes, uint32_t* blocks) {
    *bytes = *blocks = 0;
    if(!g) return;
    *bytes = sizeof(BS);
    *blocks = 1;
}
#endif
