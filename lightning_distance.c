#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <furi_hal.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdio.h>

// App state machine
typedef enum {
    LightningAppState_WaitFlash,
    LightningAppState_Counting,
    LightningAppState_ShowResult
} LightningAppState;

// App data structure
typedef struct {
    LightningAppState state;
    uint32_t start_time;
    uint32_t end_time;
    float distance_km;
    float temperature_C;  // User adjustable temperature in Celsius
    bool running;
} LightningApp;

// Input handler
static void lightning_input_callback(InputEvent* event, void* context) {
    LightningApp* app = context;

    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        switch(app->state) {
            case LightningAppState_WaitFlash:
                if(event->key == InputKeyLeft) {
                    app->temperature_C -= 0.5f;
                    if(app->temperature_C < -50.0f) app->temperature_C = -50.0f;
                } else if(event->key == InputKeyRight) {
                    app->temperature_C += 0.5f;
                    if(app->temperature_C > 60.0f) app->temperature_C = 60.0f;
                } else if(event->key == InputKeyOk && event->type == InputTypePress) {
                    app->start_time = furi_get_tick();
                    app->state = LightningAppState_Counting;
                } else if(event->key == InputKeyBack && event->type == InputTypePress) {
                    app->running = false;
                }
                break;

            case LightningAppState_Counting:
                if(event->key == InputKeyOk && event->type == InputTypePress) {
                    app->end_time = furi_get_tick();
                    float time_diff = (app->end_time - app->start_time) / 1000.0f;
                    float speed_of_sound = 331.3f + 0.606f * app->temperature_C;
                    app->distance_km = (time_diff * speed_of_sound) / 1000.0f;
                    app->state = LightningAppState_ShowResult;
                } else if(event->key == InputKeyBack && event->type == InputTypePress) {
                    app->state = LightningAppState_WaitFlash;
                }
                break;

            case LightningAppState_ShowResult:
                if(event->key == InputKeyOk && event->type == InputTypePress) {
                    app->state = LightningAppState_WaitFlash;
                } else if(event->key == InputKeyBack && event->type == InputTypePress) {
                    app->running = false;
                }
                break;
        }
    }
}

// Drawing UI
static void lightning_draw(Canvas* canvas, LightningApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(app->state) {
        case LightningAppState_WaitFlash: {
            // Draw each line manually to avoid \n
            char buf[32];
            snprintf(buf, sizeof(buf), "Set Temp: %.1f C", (double)app->temperature_C);
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buf);
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "< / > to adjust");
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "OK on Lightning");
            break;
        }
    // Hi, Muffin here. I'm worse at math than I am at programming; So if you want to validate these calculations, feel fucking free!
        case LightningAppState_Counting: {
    // Get the current system tick (milliseconds since boot)
    uint32_t now = furi_get_tick();

    // Calculate time difference in seconds between flash and now
    float time_diff = (now - app->start_time) / 1000.0f;

    // Calculate speed of sound in air at given temperature (in m/s)
    // Formula: v = 331.3 + 0.606 × T(°C)
    float speed = 331.3f + 0.606f * app->temperature_C;

    // Compute the estimated distance in kilometers
    // distance = time × speed, converted from meters to kilometers
    float distance = (time_diff * speed) / 1000.0f;

    // Draw the "Counting..." label
    char buf[32];
    snprintf(buf, sizeof(buf), "Counting...");
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buf);

    // Display the live-updated distance in km
    snprintf(buf, sizeof(buf), "%.2f km", (double)distance);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, buf);

    // Show instructions to press OK when thunder is heard
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "OK on Thunder");
    break;
}


        case LightningAppState_ShowResult: {
            char buf[32];
            snprintf(buf, sizeof(buf), "Final Distance:");
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buf);
            snprintf(buf, sizeof(buf), "%.2f km", (double)app->distance_km);
            canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, buf);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "OK to restart");
            break;
        }
    }
}


// Main app entry point
int32_t lightning_distance_app(void* p) {
    UNUSED(p);

    LightningApp app = {
        .state = LightningAppState_WaitFlash,
        .running = true,
        .temperature_C = 20.0f, // Default temperature
    };

    Gui* gui = furi_record_open("gui");
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, (ViewPortDrawCallback)lightning_draw, &app);
    view_port_input_callback_set(view_port, lightning_input_callback, &app);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(app.running) {
        view_port_update(view_port);
        furi_delay_ms(50);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");

    return 0;
}
