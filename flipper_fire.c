#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define FIRE_WIDTH 128
#define FIRE_HEIGHT 64
#define FIRE_MAX_INTENSITY 36
#define FRAME_MS 33

static const uint8_t bayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

typedef struct {
    uint8_t* pixels;
    FuriMutex* mutex;
} FireState;

static void fire_set_source(FireState* state, uint8_t intensity) {
    uint8_t* row = &state->pixels[(FIRE_HEIGHT - 1) * FIRE_WIDTH];
    for(int x = 0; x < FIRE_WIDTH; x++) row[x] = intensity;
}

static void fire_decrease_source(FireState* state) {
    uint8_t* row = &state->pixels[(FIRE_HEIGHT - 1) * FIRE_WIDTH];
    for(int x = 0; x < FIRE_WIDTH; x++) {
        int decay = rand() % 14;
        int v = row[x] - decay;
        row[x] = v > 0 ? v : 0;
    }
}

static void fire_increase_source(FireState* state) {
    uint8_t* row = &state->pixels[(FIRE_HEIGHT - 1) * FIRE_WIDTH];
    for(int x = 0; x < FIRE_WIDTH; x++) {
        int inc = rand() % 14;
        int v = row[x] + inc;
        row[x] = v < FIRE_MAX_INTENSITY ? v : FIRE_MAX_INTENSITY;
    }
}

static void fire_propagate(FireState* state) {
    uint8_t* p = state->pixels;
    for(int x = 0; x < FIRE_WIDTH; x++) {
        for(int y = 1; y < FIRE_HEIGHT; y++) {
            int src = y * FIRE_WIDTH + x;
            uint8_t intensity = p[src];
            if(intensity == 0) {
                p[src - FIRE_WIDTH] = 0;
                continue;
            }
            int r = rand() & 3;
            int dst = src - r + 1 - FIRE_WIDTH;
            if(dst < 0) dst = 0;
            if(dst >= FIRE_WIDTH * FIRE_HEIGHT) dst = FIRE_WIDTH * FIRE_HEIGHT - 1;
            int v = intensity - (r & 1);
            p[dst] = v > 0 ? v : 0;
        }
    }
}

static void fire_sound_tick(FireState* state) {
    const uint8_t* row = &state->pixels[(FIRE_HEIGHT - 1) * FIRE_WIDTH];
    int sum = 0;
    for(int x = 0; x < FIRE_WIDTH; x++) sum += row[x];
    int avg = sum / FIRE_WIDTH;

    // Random short pops whose density and loudness follow the source intensity
    if(avg > 0 && rand() % (FIRE_MAX_INTENSITY * 3) < avg) {
        float freq = 80 + rand() % 1500;
        float volume = 0.1f + 0.6f * avg / FIRE_MAX_INTENSITY;
        furi_hal_speaker_start(freq, volume);
    } else {
        furi_hal_speaker_stop();
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    FireState* state = ctx;
    canvas_clear(canvas);
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    for(int y = 0; y < FIRE_HEIGHT; y++) {
        for(int x = 0; x < FIRE_WIDTH; x++) {
            // Ordered dithering maps the 37 intensity levels onto a 1-bit display
            uint8_t level = state->pixels[y * FIRE_WIDTH + x] * 17 / (FIRE_MAX_INTENSITY + 1);
            if(level > bayer4[y & 3][x & 3]) canvas_draw_dot(canvas, x, y);
        }
    }
    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

int32_t flipper_fire_app(void* p) {
    UNUSED(p);

    FireState state;
    state.pixels = malloc(FIRE_WIDTH * FIRE_HEIGHT);
    memset(state.pixels, 0, FIRE_WIDTH * FIRE_HEIGHT);
    state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    fire_set_source(&state, FIRE_MAX_INTENSITY);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    bool sound_on = false;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(queue, &event, FRAME_MS) == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                furi_mutex_acquire(state.mutex, FuriWaitForever);
                switch(event.key) {
                case InputKeyLeft:
                    fire_decrease_source(&state);
                    break;
                case InputKeyRight:
                    fire_increase_source(&state);
                    break;
                case InputKeyDown:
                    fire_set_source(&state, 0);
                    break;
                case InputKeyUp:
                    fire_set_source(&state, FIRE_MAX_INTENSITY);
                    break;
                case InputKeyOk:
                    if(event.type != InputTypePress) break;
                    if(sound_on) {
                        furi_hal_speaker_stop();
                        furi_hal_speaker_release();
                        sound_on = false;
                    } else {
                        sound_on = furi_hal_speaker_acquire(30);
                    }
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
                furi_mutex_release(state.mutex);
            }
            continue;
        }

        furi_mutex_acquire(state.mutex, FuriWaitForever);
        fire_propagate(&state);
        if(sound_on) fire_sound_tick(&state);
        furi_mutex_release(state.mutex);
        view_port_update(view_port);
    }

    if(sound_on) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state.mutex);
    free(state.pixels);

    return 0;
}
