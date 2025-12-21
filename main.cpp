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

#define TARGET_FRAMERATE 60 // эмулируемый
#define DISPLAY_FPS      15 // выводимый чтобы не грузить систему
#define GAME_ID          34

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
    if(gameState < STATE_GAME_NEXT_LEVEL && arduboy.everyXFrames(10)) {
        sparkleFrames = (sparkleFrames + 1) % 5;
    }
    arduboy.clear();
    mainGameLoop[gameState]();
    arduboy.display();
}

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define BUFFER_SIZE    (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

typedef enum {
    EvRenderTick = 1,
    EvExit = 2,
} AppEvent;

typedef struct {
    FuriMutex* game_mutex;   // защищает screen_buffer + игровую логику
    FuriMutex* swap_mutex;   // защищает xbm_front swap

    // Таймер ТОЛЬКО ДЛЯ "пора рисовать" (без конвертации)
    FuriTimer* render_timer;

    // Игровой поток для физики 100 Гц
    FuriThread* game_thread;

    // Очередь событий (UI поток ждёт здесь)
    FuriMessageQueue* queue;

    // Arduboy page buffer
    uint8_t screen_buffer[BUFFER_SIZE];

    // XBM double buffer
    uint8_t xbm_a[BUFFER_SIZE];
    uint8_t xbm_b[BUFFER_SIZE];
    volatile uint8_t* xbm_front;

    // input_state дергается из Arduboy2Base::FlipperInputCallback
    volatile uint8_t input_state;

    // Флаг: есть новый кадр (логика изменила screen_buffer)
    volatile bool frame_dirty;

    // Выход
    volatile bool exit_requested;
} FlipperState;

static FlipperState* g_state = NULL;

// ---------------- Fast convert (page buffer -> XBM) ----------------
static void convert_screen_to_xbm_into(const uint8_t* screen, uint8_t* dst) {
    constexpr int XBM_STRIDE = DISPLAY_WIDTH / 8;
    for(int page = 0; page < DISPLAY_HEIGHT / 8; page++) {
        const int page_offset = page * DISPLAY_WIDTH;
        const int y_base = page * 8;

        for(int x = 0; x < DISPLAY_WIDTH; x += 8) {
            uint8_t c0 = screen[page_offset + x + 0] ^ 0xFF;
            uint8_t c1 = screen[page_offset + x + 1] ^ 0xFF;
            uint8_t c2 = screen[page_offset + x + 2] ^ 0xFF;
            uint8_t c3 = screen[page_offset + x + 3] ^ 0xFF;
            uint8_t c4 = screen[page_offset + x + 4] ^ 0xFF;
            uint8_t c5 = screen[page_offset + x + 5] ^ 0xFF;
            uint8_t c6 = screen[page_offset + x + 6] ^ 0xFF;
            uint8_t c7 = screen[page_offset + x + 7] ^ 0xFF;

            const int dst_xbyte = x / 8;
            int d = (y_base * XBM_STRIDE) + dst_xbyte;

            for(int b = 0; b < 8; b++) {
                uint8_t out = (((c0 >> b) & 1) << 0) | (((c1 >> b) & 1) << 1) |
                              (((c2 >> b) & 1) << 2) | (((c3 >> b) & 1) << 3) |
                              (((c4 >> b) & 1) << 4) | (((c5 >> b) & 1) << 5) |
                              (((c6 >> b) & 1) << 6) | (((c7 >> b) & 1) << 7);

                dst[d] = out;
                d += XBM_STRIDE;
            }
        }
    }
}

// ---------------- Render callback (GUI thread) ----------------
static void render_callback(Canvas* canvas, void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state || !canvas) return;

    const uint8_t* frame;

    // Стараемся взять swap_mutex, но если не получилось — читаем без него
    if(furi_mutex_acquire(state->swap_mutex, 0) == FuriStatusOk) {
        frame = (const uint8_t*)state->xbm_front;
        furi_mutex_release(state->swap_mutex);
    } else {
        frame = (const uint8_t*)state->xbm_front;
    }

    canvas_draw_xbm(canvas, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, frame);
}

// ---------------- Render timer: только шлёт событие ----------------
static void render_timer_callback(void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state || !state->queue) return;

    AppEvent ev = EvRenderTick;

    // НЕ блокируемся: если очередь заполнена — пропустим тик рендера
    // (это ок: мы не хотим душить систему)
    furi_message_queue_put(state->queue, &ev, 0);
}

// ---------------- Game thread: фиксированный тик 100 Гц, без конвертации ----------------
static int32_t game_thread_fn(void* ctx) {
    FlipperState* state = (FlipperState*)ctx;
    if(!state) return 0;

    const uint32_t hz = furi_kernel_get_tick_frequency();
    const uint32_t period_ticks = (TARGET_FRAMERATE > 0) ? (hz / TARGET_FRAMERATE) : 1;
    uint32_t next = furi_get_tick();

    while(!state->exit_requested) {
        // Ровный периодический тик без busy-loop
        next += (period_ticks > 0 ? period_ticks : 1);
        furi_delay_until_tick(next);

        // Игровая логика под мьютексом, чтобы не рвать screen_buffer
        furi_mutex_acquire(state->game_mutex, FuriWaitForever);
        game_loop_tick();
        state->frame_dirty = true;
        furi_mutex_release(state->game_mutex);
    }

    return 0;
}

// ---------------- App entry ----------------
extern "C" int32_t mybl_app(void* p) {
    UNUSED(p);

    g_state = (FlipperState*)malloc(sizeof(FlipperState));
    if(!g_state) return -1;
    memset(g_state, 0, sizeof(FlipperState));

    g_state->game_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g_state->swap_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g_state->queue = furi_message_queue_alloc(8, sizeof(AppEvent)); // маленькая очередь событий

    if(!g_state->game_mutex || !g_state->swap_mutex || !g_state->queue) {
        if(g_state->queue) furi_message_queue_free(g_state->queue);
        if(g_state->swap_mutex) furi_mutex_free(g_state->swap_mutex);
        if(g_state->game_mutex) furi_mutex_free(g_state->game_mutex);
        free(g_state);
        g_state = NULL;
        return -1;
    }

    g_state->exit_requested = false;
    g_state->frame_dirty = true;

    memset(g_state->screen_buffer, 0x00, BUFFER_SIZE);
    memset(g_state->xbm_a, 0x00, BUFFER_SIZE);
    memset(g_state->xbm_b, 0x00, BUFFER_SIZE);
    g_state->xbm_front = g_state->xbm_a;

    // Первичная конвертация пустого буфера
    convert_screen_to_xbm_into(g_state->screen_buffer, (uint8_t*)g_state->xbm_front);

    // Arduboy init
    arduboy.begin(g_state->screen_buffer, &g_state->input_state, g_state->game_mutex);
    Sprites::setArduboy(&arduboy);

    // Viewport
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, g_state);
    view_port_input_callback_set(view_port, Arduboy2Base::FlipperInputCallback, arduboy.inputContext());

    Gui* gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Setup game once
    furi_mutex_acquire(g_state->game_mutex, FuriWaitForever);
    game_setup();
    g_state->frame_dirty = true;
    furi_mutex_release(g_state->game_mutex);

    // Start game thread (physics)
    g_state->game_thread = furi_thread_alloc();
    furi_thread_set_name(g_state->game_thread, "ArduboyGame");
    furi_thread_set_stack_size(g_state->game_thread, 4096);
    furi_thread_set_context(g_state->game_thread, g_state);
    furi_thread_set_callback(g_state->game_thread, game_thread_fn);
    furi_thread_start(g_state->game_thread);

    // Start render timer: только событие, без конвертации
    g_state->render_timer = furi_timer_alloc(render_timer_callback, FuriTimerTypePeriodic, g_state);
    const uint32_t tick_hz = furi_kernel_get_tick_frequency();
    uint32_t render_period = 1;
    if(DISPLAY_FPS > 0) {
        render_period = tick_hz / DISPLAY_FPS;
        if(render_period == 0) render_period = 1;
    }
    furi_timer_start(g_state->render_timer, render_period);

    // ---------- UI loop: блокируется на очереди ----------
    while(!g_state->exit_requested) {
        AppEvent ev;
        FuriStatus st = furi_message_queue_get(g_state->queue, &ev, FuriWaitForever);
        if(st != FuriStatusOk) continue;

        if(ev == EvExit) {
            g_state->exit_requested = true;
            break;
        }

        if(ev == EvRenderTick) {
            // Если логика ничего не меняла — можно вообще пропустить
            if(!g_state->frame_dirty) {
                // даже без конвертации можно иногда делать update, но обычно не нужно
                continue;
            }

            // Конвертация ТОЛЬКО НА DISPLAY_FPS (10..30 Гц)
            // И под game_mutex, чтобы screen_buffer не рвался
            furi_mutex_acquire(g_state->game_mutex, FuriWaitForever);
            g_state->frame_dirty = false;

            uint8_t* front = (uint8_t*)g_state->xbm_front;
            uint8_t* back  = (front == g_state->xbm_a) ? g_state->xbm_b : g_state->xbm_a;

            convert_screen_to_xbm_into(g_state->screen_buffer, back);
            furi_mutex_release(g_state->game_mutex);

            // swap front buffer
            furi_mutex_acquire(g_state->swap_mutex, FuriWaitForever);
            g_state->xbm_front = back;
            furi_mutex_release(g_state->swap_mutex);

            // попросили GUI перерисовать viewport (это НЕ рисование, а запрос)
            view_port_update(view_port);
        }
    }

    // ---------- Shutdown ----------
    arduboy_tone_sound_system_deinit();

    if(g_state->render_timer) {
        furi_timer_stop(g_state->render_timer);
        furi_timer_free(g_state->render_timer);
        g_state->render_timer = NULL;
    }

    if(g_state->game_thread) {
        // Сигналим выход и ждём поток
        g_state->exit_requested = true;
        furi_thread_join(g_state->game_thread);
        furi_thread_free(g_state->game_thread);
        g_state->game_thread = NULL;
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    if(g_state->queue) furi_message_queue_free(g_state->queue);
    if(g_state->swap_mutex) furi_mutex_free(g_state->swap_mutex);
    if(g_state->game_mutex) furi_mutex_free(g_state->game_mutex);

    free(g_state);
    g_state = NULL;

    return 0;
}
