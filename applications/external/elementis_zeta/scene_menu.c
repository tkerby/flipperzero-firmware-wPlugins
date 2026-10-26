// MENU scene: right-side pause menu. Labels are global text keys loaded once on enter (so the menu
// follows the active language, and are reloaded by scene_menu_reload() when the options screen
// switches it); Equipo -> the party screen (scene_menu_team_hook, a plugin), Mochila -> the bag screen (menu_open_bag, a plugin), <NAME> -> the trainer card
// (menu_open_bond MB_CARD, M5-C: the label is the player's name, text "{player}"), Guardar -> the save screen (scene_save.c, UI plugin),
// Opciones -> options screen, Salir -> quit. Registro (the first entry, C2-S2, contract 30) -> the Registro screen (menu_open_bond BOND_REG, casino plugin since contract 76); it is only there
// once the player has the Registro (world flag FLAG_REGISTRO = parcel_delivered): without it the menu is the six entries of before (`first` = 1).
// To add an entry: put its text id in ITEM_TXT (Quit stays last) and handle it in m_input().
#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-tree-vrp,no-reorder-blocks,no-tree-reassoc,no-move-loop-invariants") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "ui_host.h"
#include "menu_host.h"
#include "sound.h"
#include "pc_host.h" // FLAG_REGISTRO
#include "casino_host.h" // BOND_REG (menu_open_bond)
#include "font.h"
#include "frame.h"
#include "bag.h"
#include "party.h" // contract 81: the Reserva HUD reads PartyState.rv_steps
#include "gen/maps_gen.h"
#include <string.h>
#include <stdlib.h>

#define MENU_W_MIN 56
#define MENU_RV_W \
    52 // the Reserva HUD's box (contract 81): "500/500" and "Cristal" are 42 px, and the menu box (>= 56 px) starts at x <= 72
#define MENU_ROW0_Y    2
#define MENU_PAD_X     10 // label x offset inside the frame (cursor sits at +3)
#define MENU_RIGHT_PAD 8 // M5-C: room for the scroll arrow (six entries scroll)
#define MENU_LABEL_CODES \
    12 // (the longest label is "Registro" / "Registry" = 8 codes: 12 keeps the bss of seven labels at the size of six at 16)

static const uint16_t ITEM_TXT[] = {
    TXT_GL_SAVE_REG,
    TXT_GL_MENU_TEAM,
    TXT_GL_MENU_BAG,
    TXT_GL_MENU_CARD,
    TXT_GL_MENU_SAVE,
    TXT_GL_MENU_OPTIONS,
    TXT_GL_MENU_QUIT};
#define N_ALL ((int)(sizeof(ITEM_TXT) / sizeof(ITEM_TXT[0])))
#define N_ITEMS(m) \
    (N_ALL - (m)->first) // the entries shown: the Registro (index 0) only with the flag

_Static_assert(
    TXT_GL_MISC_RV_BALLS_N == TXT_GL_MISC_RV_STEPS + 3,
    "gl.misc.rv_* (the Reserva HUD's four lines) must be consecutive (the HUD loads them as +k)");

typedef struct {
    uint8_t cur, top,
        vis; // M5-C: six entries do not fit 64 px at pitch 11 / 12: the box shows `vis` rows and scrolls with the cursor
    uint8_t
        first; // C2-S2: 0 = the Registro is the first entry, 1 = it is hidden (labels and cursor index the shown entries: ITEM_TXT[first + k])
    int16_t x, w, h; // frame, right-aligned; derived from the labels and the font pitch
    bool hidden; // the save screen (UI plugin) is on: it replaces the menu box
    tchar rv[4]
            [9]; // contract 81: inside the Reserva Salvaje the box of start menu, four one-line texts gl.misc.rv_steps / rv_steps_n / rv_balls / rv_balls_n (a caption and a number each, the numbers are {v2} / {v3} tokens); [0][0] = 0 outside
    tchar label[N_ALL][MENU_LABEL_CODES];
} Menu;

static Menu*
    g_menu; // a heap block while the menu is on the stack (contract 69: it was 184 B of .bss all the time; the follower's memory budget)

bool (*scene_menu_team_hook)(SceneCtx* c) = NULL;

bool scene_menu_open(SceneCtx* c) {
    Menu* m = calloc(1, sizeof(Menu));
    if(!m || !scene_push(c, &scene_menu_ops, m)) {
        free(m);
        return false;
    }
    g_menu = m;
    return true;
}

// (Re)loads the labels in the active language and derives the frame size from them.
void scene_menu_set_hidden(bool hidden) {
    if(g_menu) g_menu->hidden = hidden;
}

static void rv_vars(void) { // the HUD's {v2} = the steps left, {v3} = the crystals left
    text_set_var_num(2, party_state()->rv_steps);
    text_set_var_num(3, bag_count(IT_CRISTAL_RESERVA));
}

static void load_labels(Menu* m) {
    int maxw = 0;
    const tchar* lab[N_ALL + 4];
    const int n = N_ITEMS(m);
    for(int k = 0; k < n; k++) {
        const uint16_t id = ITEM_TXT[m->first + k];
        if(text_load(id, m->label[k], MENU_LABEL_CODES) < 0) m->label[k][0] = 0;
        lab[k] = m->label[k];
        int w = id == TXT_GL_MENU_CARD ?
                    text_line_px(m->label[k], 0) :
                    font_text_width(m->label[k]); // (the name is a token: expanded when drawn)
        if(w > maxw) maxw = w;
    }
    for(int k = 0; k < 4; k++) {
        if(!party_state()->rv_steps || text_load(TXT_GL_MISC_RV_STEPS + k, m->rv[k], 9) < 0)
            m->rv[k][0] = 0;
        lab[n + k] = m->rv[k];
    }
    rv_vars();
    text_prefetch_many(
        lab,
        n + 4); // SD-backed font: one open of the font file for all labels (and the HUD's captions)
    m->w = (int16_t)(MENU_PAD_X + maxw + MENU_RIGHT_PAD);
    if(m->w < MENU_W_MIN) m->w = MENU_W_MIN;
    m->x = (int16_t)(SCREEN_W - m->w);
    m->vis = (uint8_t)((SCREEN_H - 4) / font_pitch());
    if(m->vis > n) m->vis = (uint8_t)n;
    if(m->top + m->vis > n) m->top = (uint8_t)(n - m->vis);
    m->h = (int16_t)(m->vis * font_pitch() + 4);
}

static void follow(Menu* m) { // keep the cursor in the visible window
    if(m->cur < m->top) m->top = m->cur;
    if(m->cur >= m->top + m->vis) m->top = (uint8_t)(m->cur - m->vis + 1);
}

static void m_enter(SceneCtx* c, void* d) {
    Menu* m = d;
    m->top = 0;
    m->first = c->world && world_flag_get(c->world, FLAG_REGISTRO) ? 0 : 1;
    load_labels(m);
    scene_dirty(c, D_COMPOSE);
}

#ifndef NUCLEO_DEVICE
size_t scene_menu_heap(
    void) { // host tests: the bytes of the menu's own block while it is open (the leak check of the harness does not count it)
    return g_menu ? sizeof(Menu) : 0;
}
#endif

static void m_leave(SceneCtx* c, void* d) {
    (void)c;
    free(d);
    g_menu = NULL;
}

void scene_menu_reload(SceneCtx* c) {
    if(g_menu) load_labels(g_menu); // (the options screen of the title has no menu below it)
    scene_dirty(c, D_COMPOSE);
}

const tchar* scene_menu_label(int k) {
    return g_menu && k >= 0 && k < N_ITEMS(g_menu) ? g_menu->label[k] : NULL;
}

static void m_close(SceneCtx* c) {
    sound_play(SND_FX_CANCEL); // (contract 28)
    scene_pop(c);
    scene_wake(c, 1);
}

static void m_input(SceneCtx* c, void* d, const InputEv* ev) {
    Menu* m = d;
    if(ev->key == KEY_BACK) {
        if(ev->type == EV_LONG)
            scene_clear(c);
        else if(ev->type == EV_SHORT)
            m_close(c);
        return;
    }
    if(ev->type != EV_PRESS && ev->type != EV_REPEAT) return;
    if(ev->key == KEY_UP) {
        m->cur = (uint8_t)((m->cur + N_ITEMS(m) - 1) % N_ITEMS(m));
        follow(m);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_DOWN) {
        m->cur = (uint8_t)((m->cur + 1) % N_ITEMS(m));
        follow(m);
        scene_dirty(c, D_COMPOSE);
    } else if(ev->key == KEY_OK && ev->type == EV_PRESS) {
        sound_play(SND_FX_CONFIRM); // (contract 28)
        const uint16_t id = ITEM_TXT[m->first + m->cur];
        if(id == TXT_GL_MENU_QUIT) {
            c->game->quit = true;
            scene_clear(c);
        } else if(id == TXT_GL_MENU_OPTIONS) {
            ui_open_options(c);
        } else if(id == TXT_GL_SAVE_REG && menu_open_bond(c, BOND_REG)) {
            // the Registro is open above the menu
        } else if(id == TXT_GL_MENU_TEAM && scene_menu_team_hook && scene_menu_team_hook(c)) {
            // the party screen (or the "no Elementis" dialog) is open above the menu
        } else if(id == TXT_GL_MENU_BAG && menu_open_bag(c)) {
            // the bag screen (or the "empty bag" dialog) is open above the menu
        } else if(id == TXT_GL_MENU_CARD && menu_open_bond(c, MB_CARD)) {
            // the trainer card is open above the menu
        } else if(id == TXT_GL_MENU_SAVE && ui_open_save(c)) {
            // the save screen (info box, question, YES / NO) is open above the menu, which stops drawing
        } else {
            scene_dialog_open(c, TXT_GL_PRONTO, -1);
        }
    }
}

// C4-B (contract 36): a stone used from the bag (scene_party.c, menu plugin) leaves its evolution here: the bag and party screens have popped themselves, so the menu
// plugin can go and the UI plugin's evolution scene can open (the plugin must not be swapped from inside its own code). The stone was already taken; if the scene
// cannot open it is given back.
static void m_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)d;
    (void)dt;
    const uint16_t v = c->stone_evo;
    if(!v) return;
    c->stone_evo = 0;
    if(!ui_open_evo(c, (uint8_t)v)) bag_add((uint8_t)(v >> 8), 1);
}

static void m_draw_overlay(SceneCtx* c, void* d) {
    (void)c;
    const Menu* m = d;
    if(m->hidden) return;
    if(m->rv[0][0]) { // contract 81: the Reserva's steps and crystals, in a box at the top left
        const int p = font_pitch();
        rv_vars();
        ov_fill(0, 0, MENU_RV_W, 4 * p + 4, 0);
        ov_frame(0, 0, MENU_RV_W, 4 * p + 4, 1);
        for(int r = 0; r < 4; r++)
            text_draw_line(m->rv[r], 0, 3, 2 + r * p, 99);
    }
    ov_fill(m->x, 0, m->w, m->h, 0);
    ov_frame(m->x, 0, m->w, m->h, 1);
    for(int v = 0; v < m->vis; v++) {
        const int k = m->top + v, y = MENU_ROW0_Y + v * font_pitch();
        if(k == m->cur) font_draw_glyph(GLYPH_CURSOR, m->x + 3, y, 1);
        if(ITEM_TXT[m->first + k] == TXT_GL_MENU_CARD)
            text_draw_line(m->label[k], 0, m->x + MENU_PAD_X, y, 99);
        else
            font_draw_text(m->label[k], m->x + MENU_PAD_X, y);
    }
    static const uint8_t UP[3] = {0x20, 0x70, 0xF8},
                         DN[3] = {0xF8, 0x70, 0x20}; // more rows above / below
    if(m->top > 0) ov_bitmap(m->x + m->w - 7, 1, 5, 3, UP, 1, 1);
    if(m->top + m->vis < N_ITEMS(m)) ov_bitmap(m->x + m->w - 7, m->h - 4, 5, 3, DN, 1, 1);
}

const SceneOps scene_menu_ops = {
    .enter = m_enter,
    .leave = m_leave,
    .input = m_input,
    .update = m_update,
    .draw_world = NULL,
    .draw_overlay = m_draw_overlay,
    .pauses_below = true,
};
