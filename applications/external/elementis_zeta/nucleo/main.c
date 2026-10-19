#ifdef NUCLEO_DEVICE
#pragma GCC optimize("no-ira-hoist-pressure,no-rerun-cse-after-loop") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include <furi.h>
#include <gui/gui.h>
#include <string.h>
#include "game.h"
#include "input.h"
#include "assets.h"
#include "debug.h"

// ---- game thread <-> flip loop hand-off -------------------------------------------------------
// The flip loop releases this binary semaphore each time it adopts a published set; the game
// thread blocks on it instead of polling. Stale tokens are harmless: game_wait_ready re-checks.
static FuriSemaphore* adopt_sem;

void game_wait_ready(Frame* f) {
    while(!frame_ready(f)) {
        if(adopt_sem)
            furi_semaphore_acquire(adopt_sem, 50);
        else
            furi_delay_ms(1);
    }
}

// ---- display framebuffer capture --------------------------------------------------------------
// canvas_commit() sends the canvas's static 1024-byte u8g2 buffer (page layout) to the LCD. The
// buffer pointer is not exported, but the GUI hands it to framebuffer callbacks, so we register
// one just long enough to learn the pointer and then remove it.
static struct {
    uint8_t* volatile data;
    volatile size_t size;
    volatile CanvasOrientation orient;
    volatile bool seen;
} cap;

static void fb_capture_cb(uint8_t* data, size_t size, CanvasOrientation orient, void* ctx) {
    UNUSED(ctx);
    if(cap.seen) return;
    cap.data = data;
    cap.size = size;
    cap.orient = orient;
    cap.seen = true; // last
}

// Step 1 (before gui_direct_draw_acquire): registering requests a GUI redraw, which dispatches
// framebuffer callbacks from the GUI thread.
static void fb_capture_begin(Gui* gui) {
    memset((void*)&cap, 0, sizeof(cap));
    gui_add_framebuffer_callback(gui, fb_capture_cb, NULL);
}

// Step 2 (after acquire): in case this firmware dispatches the callback from canvas_commit()
// instead, commit once and wait for the callback. Returns the framebuffer or NULL (caller falls
// back to canvas_draw_xbm).
static uint8_t* fb_capture_end(Gui* gui, Canvas* canvas) {
    canvas_reset(canvas);
    canvas_commit(canvas);
    for(int i = 0; i < 60 && !cap.seen; i++) furi_delay_ms(5);
    gui_remove_framebuffer_callback(gui, fb_capture_cb, NULL); // exported; takes the GUI lock

    if(!cap.seen) return NULL;
    uint8_t* d = cap.data;
    // Sanity: exact size, normal orientation (flipped/vertical would change the layout),
    // pointer inside on-chip SRAM (0x2000_0000..0x2003_FFFF on the STM32WB55).
    if(cap.size != PLANE_BYTES || cap.orient != CanvasOrientationHorizontal || !d) return NULL;
    if(((uintptr_t)d >> 18) != (0x20000000u >> 18)) return NULL;
    return d;
}

int32_t nucleo_app(void* p) {
    UNUSED(p);

    Game* g = malloc(sizeof(Game));
    memset(g, 0, sizeof(Game));
    g->frame = malloc(sizeof(Frame));
    Frame* f = g->frame;
    if(!frame_init(f)) { // 6 KB of display sets: without them there is nothing to show
        frame_deinit(f);
        free(f);
        free(g);
        return 0;
    }
    adopt_sem = furi_semaphore_alloc(1, 0);

    Gui* gui = furi_record_open(RECORD_GUI);
    input_init();
    fb_capture_begin(gui);
    Canvas* canvas = gui_direct_draw_acquire(gui);
    uint8_t* fb = fb_capture_end(gui, canvas);
    debug_set_fast_fb(fb != NULL);

    FuriThread* gt = furi_thread_alloc_ex("nucleo_game", GAME_STACK, game_thread, g);
    furi_thread_set_priority(gt, FuriThreadPriorityNormal);
    furi_thread_start(gt);

    // Flips are now cheap (memcpy + SPI send ~3 ms), so the flip loop runs above the game thread:
    // it wakes on its tick, preempts rendering for ~3 ms and gives even 13 ms spacing.
    furi_thread_set_current_priority(FuriThreadPriorityHigh);

    // Flip loop: shows the 3 temporal sub-frames in sequence, adopting a newly published
    // set only on the sub-frame-0 boundary so a frame is never shown half old / half new.
    // Paced by a fixed period (furi_delay_until_tick) so flips do not drift with their cost.
    int s = 0;
    uint32_t next = furi_get_tick();
    while(!g->quit) {
        debug_flip_begin();
        if(fb) {
            memcpy(fb, f->sets[f->front][s], PLANE_BYTES);
        } else {
            canvas_reset(canvas);
            canvas_draw_xbm(canvas, 0, 0, SCREEN_W, SCREEN_H, f->sets[f->front][s]);
        }
        canvas_commit(canvas);
        debug_flip_end();
        debug_flip();
        s = (s + 1) % CYCLE;
        if(s == 0 && frame_adopt(f) && adopt_sem) furi_semaphore_release(adopt_sem);

        next += FLIP_PERIOD_MS;
        uint32_t now = furi_get_tick();
        if((int32_t)(next - now) <= 0) next = now + 1; // overran: resync, but always yield a tick
        furi_delay_until_tick(next);
    }

    furi_thread_set_current_priority(FuriThreadPriorityNormal);
    furi_thread_join(gt);
    furi_thread_free(gt);
    debug_log_stack("main (flip loop)", MAIN_STACK);
    assets_shutdown(); // belt and braces: nothing may stay open after the app
    gui_direct_draw_release(gui);
    input_deinit();
    furi_record_close(RECORD_GUI);
    furi_semaphore_free(adopt_sem);
    adopt_sem = NULL;
    frame_deinit(f);
    free(f);
    free(g);
    return 0;
}
