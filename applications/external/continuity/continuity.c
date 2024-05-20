#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_speaker.h>

#include <gui/gui.h>
#include <input/input.h>

// extern const GpioPin gpio_ext_pc0;

// These should be an app context, I know.
bool pinState = false; // yeah globals ftw!
float volume = 100.0f;

void continuity_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

void continuity_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Continuity");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Circuit:");
    if(pinState) {
        canvas_draw_str(canvas, 64, 24, "SHORTED");
    } else {
        canvas_draw_str(canvas, 64, 24, "OPEN");
    }

    char volume_str[16];
    char* volumeString = itoa((int)volume, volume_str, 10);
    canvas_draw_str(canvas, 2, 36, "Volume: ");
    canvas_draw_str(canvas, 64, 36, strcat(volumeString, "%"));

    canvas_draw_str(canvas, 2, 52, "Probe using pins 9 and 16");
    canvas_draw_str(canvas, 2, 64, "Press back to exit");
}

int32_t continuity_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("Continuity", "App started");

    // Init Message queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    InputEvent event;

    // Init GPIO
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeInput, GpioPullDown, GpioSpeedLow);
    // furi_hal_gpio_add_int_callback(&gpio_ext_pc0, continuity_gpio_callback, NULL);

    // Create view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, continuity_draw_callback, NULL);
    view_port_input_callback_set(view_port, continuity_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    if(!furi_hal_speaker_acquire(1000)) {
        FURI_LOG_E("Continuity", "Failed to acquire speaker");
        return -1;
    };

    // Always run the speaker, we modulate the volume in callbacks
    if(furi_hal_speaker_is_mine()) {
    } else {
        FURI_LOG_E("Continuity", "Lost ownership of speaker??");
        return -1;
    }

    FURI_LOG_I("Continuity", "Init complete");

    // Main loop
    do {
        bool readPin = furi_hal_gpio_read(&gpio_ext_pc0);

        if(readPin != pinState) {
            // Update the display!
            view_port_update(view_port);

            pinState = readPin;

            if(pinState) {
                furi_hal_speaker_start(1000.0f, (volume / 100.0f));
            } else {
                furi_hal_speaker_stop();
            }
        }

        if(furi_message_queue_get(event_queue, &event, 1) == FuriStatusOk) {
            FURI_LOG_I("Continuity", "Input event from queue");

            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                break;
            }
            if(event.type == InputTypeShort && event.key == InputKeyDown) {
                volume -= 25.0f;
            }
            if(event.type == InputTypeShort && event.key == InputKeyUp) {
                volume += 25.0f;
            }

            if(volume < 0.0f) {
                volume = 0.0f;
            }
            if(volume > 100.0f) {
                volume = 100.0f;
            }
        }
    } while(true);

    // CLEANUP
    furi_hal_gpio_remove_int_callback(&gpio_ext_pc0);
    furi_hal_speaker_release();

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}