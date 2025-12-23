#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

#define COLOUR_WHITE     1
#define COLOUR_BLACK     0
#define TONES_END        0x8000

#define TARGET_FRAMERATE 50
#define DISPLAY_FPS      16
#define GAME_ID          34

#ifndef HOLD_AB_TO_EXIT_FRAMES
#define HOLD_AB_TO_EXIT_FRAMES 50
#endif

#include "lib/Arduboy2.h"
#include "lib/Arduino.h"
#include "game/globals.h"
#include "game/menu.h"
#include "game/game.h"
#include "game/inputs.h"
#include "game/player.h"
#include "game/enemies.h"
#include "game/elements.h"
#include "game/levels.h"

using FunctionPointer = void (*)();
static FunctionPointer mainGameLoop[] = {
    stateMenuIntro,
    stateMenuMain,
    stateMenuHelp,
    stateMenuPlaySelect,
    stateMenuInfo,
    stateMenuSoundfx,
    stateGameNextLevel,
    stateGamePlaying,
    stateGamePause,
    stateGameOver,
    stateMenuPlayContinue,
    stateMenuPlayNew,
};

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define BUFFER_SIZE    (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

typedef struct {
    uint8_t screen_buffer[BUFFER_SIZE];
    uint8_t xbm_buffers[2][BUFFER_SIZE];
    
    FuriMutex* mutex;
    ViewPort* view_port;
    FuriTimer* timer;
    
    volatile uint8_t xbm_read_idx;
    volatile uint8_t input_state;
    volatile bool exit_requested;
} FlipperState;

static FlipperState* g_state = NULL;

static inline void convert_screen_fast(const uint8_t* screen, uint8_t* dst) {
    const int XBM_STRIDE = DISPLAY_WIDTH / 8;
    
    for(int page = 0; page < 8; page++) {
        const int page_offset = page * DISPLAY_WIDTH;
        const int y_base = page * 8;
        
        for(int x = 0; x < DISPLAY_WIDTH; x += 16) {
            uint8_t c[16];
            for(int i = 0; i < 16; i++) {
                c[i] = screen[page_offset + x + i] ^ 0xFF;
            }
            
            const int dst_xbyte = x / 8;
            int d = (y_base * XBM_STRIDE) + dst_xbyte;
            
            dst[d] = (c[0] & 1) | ((c[1] & 1) << 1) | ((c[2] & 1) << 2) | ((c[3] & 1) << 3) |
                     ((c[4] & 1) << 4) | ((c[5] & 1) << 5) | ((c[6] & 1) << 6) | ((c[7] & 1) << 7);
            dst[d + 1] = (c[8] & 1) | ((c[9] & 1) << 1) | ((c[10] & 1) << 2) | ((c[11] & 1) << 3) |
                         ((c[12] & 1) << 4) | ((c[13] & 1) << 5) | ((c[14] & 1) << 6) | ((c[15] & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 1) & 1) | (((c[1] >> 1) & 1) << 1) | (((c[2] >> 1) & 1) << 2) | (((c[3] >> 1) & 1) << 3) |
                     (((c[4] >> 1) & 1) << 4) | (((c[5] >> 1) & 1) << 5) | (((c[6] >> 1) & 1) << 6) | (((c[7] >> 1) & 1) << 7);
            dst[d + 1] = ((c[8] >> 1) & 1) | (((c[9] >> 1) & 1) << 1) | (((c[10] >> 1) & 1) << 2) | (((c[11] >> 1) & 1) << 3) |
                         (((c[12] >> 1) & 1) << 4) | (((c[13] >> 1) & 1) << 5) | (((c[14] >> 1) & 1) << 6) | (((c[15] >> 1) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 2) & 1) | (((c[1] >> 2) & 1) << 1) | (((c[2] >> 2) & 1) << 2) | (((c[3] >> 2) & 1) << 3) |
                     (((c[4] >> 2) & 1) << 4) | (((c[5] >> 2) & 1) << 5) | (((c[6] >> 2) & 1) << 6) | (((c[7] >> 2) & 1) << 7);
            dst[d + 1] = ((c[8] >> 2) & 1) | (((c[9] >> 2) & 1) << 1) | (((c[10] >> 2) & 1) << 2) | (((c[11] >> 2) & 1) << 3) |
                         (((c[12] >> 2) & 1) << 4) | (((c[13] >> 2) & 1) << 5) | (((c[14] >> 2) & 1) << 6) | (((c[15] >> 2) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 3) & 1) | (((c[1] >> 3) & 1) << 1) | (((c[2] >> 3) & 1) << 2) | (((c[3] >> 3) & 1) << 3) |
                     (((c[4] >> 3) & 1) << 4) | (((c[5] >> 3) & 1) << 5) | (((c[6] >> 3) & 1) << 6) | (((c[7] >> 3) & 1) << 7);
            dst[d + 1] = ((c[8] >> 3) & 1) | (((c[9] >> 3) & 1) << 1) | (((c[10] >> 3) & 1) << 2) | (((c[11] >> 3) & 1) << 3) |
                         (((c[12] >> 3) & 1) << 4) | (((c[13] >> 3) & 1) << 5) | (((c[14] >> 3) & 1) << 6) | (((c[15] >> 3) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 4) & 1) | (((c[1] >> 4) & 1) << 1) | (((c[2] >> 4) & 1) << 2) | (((c[3] >> 4) & 1) << 3) |
                     (((c[4] >> 4) & 1) << 4) | (((c[5] >> 4) & 1) << 5) | (((c[6] >> 4) & 1) << 6) | (((c[7] >> 4) & 1) << 7);
            dst[d + 1] = ((c[8] >> 4) & 1) | (((c[9] >> 4) & 1) << 1) | (((c[10] >> 4) & 1) << 2) | (((c[11] >> 4) & 1) << 3) |
                         (((c[12] >> 4) & 1) << 4) | (((c[13] >> 4) & 1) << 5) | (((c[14] >> 4) & 1) << 6) | (((c[15] >> 4) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 5) & 1) | (((c[1] >> 5) & 1) << 1) | (((c[2] >> 5) & 1) << 2) | (((c[3] >> 5) & 1) << 3) |
                     (((c[4] >> 5) & 1) << 4) | (((c[5] >> 5) & 1) << 5) | (((c[6] >> 5) & 1) << 6) | (((c[7] >> 5) & 1) << 7);
            dst[d + 1] = ((c[8] >> 5) & 1) | (((c[9] >> 5) & 1) << 1) | (((c[10] >> 5) & 1) << 2) | (((c[11] >> 5) & 1) << 3) |
                         (((c[12] >> 5) & 1) << 4) | (((c[13] >> 5) & 1) << 5) | (((c[14] >> 5) & 1) << 6) | (((c[15] >> 5) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 6) & 1) | (((c[1] >> 6) & 1) << 1) | (((c[2] >> 6) & 1) << 2) | (((c[3] >> 6) & 1) << 3) |
                     (((c[4] >> 6) & 1) << 4) | (((c[5] >> 6) & 1) << 5) | (((c[6] >> 6) & 1) << 6) | (((c[7] >> 6) & 1) << 7);
            dst[d + 1] = ((c[8] >> 6) & 1) | (((c[9] >> 6) & 1) << 1) | (((c[10] >> 6) & 1) << 2) | (((c[11] >> 6) & 1) << 3) |
                         (((c[12] >> 6) & 1) << 4) | (((c[13] >> 6) & 1) << 5) | (((c[14] >> 6) & 1) << 6) | (((c[15] >> 6) & 1) << 7);
            d += XBM_STRIDE;
            
            dst[d] = ((c[0] >> 7) & 1) | (((c[1] >> 7) & 1) << 1) | (((c[2] >> 7) & 1) << 2) | (((c[3] >> 7) & 1) << 3) |
                     (((c[4] >> 7) & 1) << 4) | (((c[5] >> 7) & 1) << 5) | (((c[6] >> 7) & 1) << 6) | (((c[7] >> 7) & 1) << 7);
            dst[d + 1] = ((c[8] >> 7) & 1) | (((c[9] >> 7) & 1) << 1) | (((c[10] >> 7) & 1) << 2) | (((c[11] >> 7) & 1) << 3) |
                         (((c[12] >> 7) & 1) << 4) | (((c[13] >> 7) & 1) << 5) | (((c[14] >> 7) & 1) << 6) | (((c[15] >> 7) & 1) << 7);
        }
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state || !canvas) return;
    
    uint8_t idx = state->xbm_read_idx;
    canvas_draw_xbm(canvas, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, state->xbm_buffers[idx]);
}

static void game_setup() {
    arduboy.boot();
    arduboy.audio.begin();
    arduboy.bootLogoSpritesSelfMasked();
    arduboy.setFrameRate(TARGET_FRAMERATE);
    loadSetEEPROM();
    arduboy.pollButtons();
    arduboy.clear();
    mainGameLoop[gameState]();
    arduboy.display();
}

static void game_loop_tick() {
    arduboy.pollButtons();
    if(!(arduboy.nextFrame())) return;

    static uint16_t ab_hold = 0;
    const bool ab_pressed = arduboy.pressed(A_BUTTON) && arduboy.pressed(B_BUTTON);
    
    if(ab_pressed) {
        if(ab_hold < 0xFFFF) ab_hold++;
        if(ab_hold >= HOLD_AB_TO_EXIT_FRAMES) {
            if(g_state) g_state->exit_requested = true;
            ab_hold = 0;
        }
    } else {
        ab_hold = 0;
    }

    if(gameState < STATE_GAME_NEXT_LEVEL && arduboy.everyXFrames(10)) {
        sparkleFrames = (sparkleFrames + 1) % 5;
    }

    arduboy.clear();
    mainGameLoop[gameState]();
    arduboy.display();
}

static void timer_callback(void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state) return;
    
    static uint32_t logic_counter = 0;
    const uint32_t convert_ratio = TARGET_FRAMERATE / DISPLAY_FPS;
    
    if(furi_mutex_acquire(state->mutex, 0) != FuriStatusOk) return;
    
    game_loop_tick();
    logic_counter++;
    
    if(logic_counter >= convert_ratio) {
        uint8_t read_idx = state->xbm_read_idx;
        uint8_t write_idx = 1 - read_idx;
        
        convert_screen_fast(state->screen_buffer, state->xbm_buffers[write_idx]);
        state->xbm_read_idx = write_idx;
        
        view_port_update(state->view_port);
        logic_counter = 0;
    }
    
    furi_mutex_release(state->mutex);
}

extern "C" int32_t mybl_app(void* p) {
    UNUSED(p);

    g_state = (FlipperState*)malloc(sizeof(FlipperState));
    if(!g_state) return -1;
    memset(g_state, 0, sizeof(FlipperState));

    g_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!g_state->mutex) {
        free(g_state);
        g_state = NULL;
        return -1;
    }

    g_state->exit_requested = false;
    g_state->xbm_read_idx = 0;
    memset(g_state->screen_buffer, 0x00, BUFFER_SIZE);
    memset(g_state->xbm_buffers, 0x00, sizeof(g_state->xbm_buffers));

    convert_screen_fast(g_state->screen_buffer, g_state->xbm_buffers[0]);

    arduboy.begin(g_state->screen_buffer, &g_state->input_state, g_state->mutex);
    Sprites::setArduboy(&arduboy);

    g_state->view_port = view_port_alloc();
    view_port_draw_callback_set(g_state->view_port, render_callback, g_state);
    view_port_input_callback_set(g_state->view_port, Arduboy2Base::FlipperInputCallback, arduboy.inputContext());

    Gui* gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, g_state->view_port, GuiLayerFullscreen);

    furi_mutex_acquire(g_state->mutex, FuriWaitForever);
    game_setup();
    convert_screen_fast(g_state->screen_buffer, g_state->xbm_buffers[0]);
    furi_mutex_release(g_state->mutex);

    g_state->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, g_state);
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    uint32_t period = tick_hz / TARGET_FRAMERATE;
    if(period == 0) period = 1;
    furi_timer_start(g_state->timer, period);

    while(!g_state->exit_requested) {
        furi_delay_ms(100);
    }

    arduboy_tone_sound_system_deinit();

    if(g_state->timer) {
        furi_timer_stop(g_state->timer);
        furi_timer_free(g_state->timer);
    }

    gui_remove_view_port(gui, g_state->view_port);
    view_port_free(g_state->view_port);
    furi_record_close(RECORD_GUI);

    if(g_state->mutex) furi_mutex_free(g_state->mutex);
    free(g_state);
    g_state = NULL;

    return 0;
}