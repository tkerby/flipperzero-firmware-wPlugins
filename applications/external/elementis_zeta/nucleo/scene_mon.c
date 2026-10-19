// MONPIC scene: framed Elementi picture above the text box (shows the creature when you pick a starter).
// It sits BELOW the dialog that follows it, draws the world plus the picture into the gray planes (lvl) and closes
// itself the first time it is on top again (= the dialog / YES-NO box above it closed). The picture is loaded
// when the scene is opened and freed when it closes (about 300-600 B of heap while it is up).
#include <stddef.h>
#include "scene.h"
#include "world.h"
#include "text.h"
#include "monpic.h"
#include "ui_host.h"

#define PAD_X 3    // blank columns between the picture and the frame (frame line included)
#define PAD_Y 2
#define AREA_W 100 // the picture is centred in the left part of the screen (the YES / NO box lives at the right)

static MonPic g_pic;

bool scene_mon_open(SceneCtx* c, uint8_t species) {
    if(c->n >= MAX_SCENES) return false;
    if(g_pic.blk) monpic_free(&g_pic); // a stale one (cannot normally happen)
    if(!monpic_load(&g_pic, species, false)) return false;
    if(!scene_push(c, &scene_mon_ops, &g_pic)) {
        monpic_free(&g_pic);
        return false;
    }
    return true;
}

static void m_enter(SceneCtx* c, void* d) {
    (void)c;
    (void)d;
    ui_live_enter();
}

static void m_leave(SceneCtx* c, void* d) {
    (void)c;
    ui_live_leave();
    monpic_free((MonPic*)d);
}

static void m_input(SceneCtx* c, void* d, const InputEv* ev) {
    (void)d;
    if(ev->key == KEY_BACK && ev->type == EV_LONG) scene_clear(c);
}

static void m_update(SceneCtx* c, void* d, uint32_t dt) {
    (void)d;
    (void)dt;
    scene_pop(c); // on top again: whatever was opened after us has closed
    scene_wake(c, 1); // let the world run (the script resumes)
}

static void put(LvlBuf lvl, int x, int y, uint8_t v) {
    lvl_putc(lvl, x, y, v);
}

static void m_draw_world(SceneCtx* c, void* d) {
    const MonPic* p = d;
    world_draw_frozen(c); // the world as it is (frozen)
    if(!p || !p->blk) return;
    LvlBuf lvl = c->frame->lvl;
    const TbGeom* g = tb_geom();
    const int fw = p->b.w + 2 * PAD_X, fh = p->b.h + 2 * PAD_Y;
    const int fx = (AREA_W - fw) / 2;
    const int fy = g->box_y - 1 - fh; // one blank row above the text box
    for(int y = 0; y < fh; y++)
        for(int x = 0; x < fw; x++) {
            bool edge = x == 0 || y == 0 || x == fw - 1 || y == fh - 1;
            bool corner = (x == 0 || x == fw - 1) && (y == 0 || y == fh - 1);
            if(!corner) put(lvl, fx + x, fy + y, edge ? 3 : 0); // white box, black frame, trimmed corners
        }
    blit_masked(lvl, &p->b, fx + PAD_X, fy + PAD_Y);
}

const SceneOps scene_mon_ops = {
    .enter = m_enter,
    .leave = m_leave,
    .input = m_input,
    .update = m_update,
    .draw_world = m_draw_world,
    .draw_overlay = NULL,
    .pauses_below = true,
};
