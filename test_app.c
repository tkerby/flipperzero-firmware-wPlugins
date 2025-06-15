#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <string.h>

// Struct for coin data
typedef struct {
    int32_t counter, counter_heads, counter_tails;
    bool flipped, heads;
} Coin;

static Coin coin = {.counter = 0, .counter_heads = 0, .counter_tails = 0, .flipped = 0, .heads = 0};

// UART Serial Handle
static FuriHalSerialHandle* serial_handle = NULL;

static const NotificationSequence coinflip_start = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_blink_start_10,
    &message_blink_set_color_magenta,
    &message_delay_100,
    &message_blink_stop,
    NULL,
};

static const NotificationSequence coinflip_stop = {
    &message_blink_stop,
    NULL,
};

// Random flip
bool random_flip() {
    return rand() % 2 == 0;
}

// 🆕 Initialize UART using GPIO 2 (PA7)
void init_uart_serial() {
    serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    furi_assert(serial_handle);

    // Redirect UART TX to PA7 (GPIO 2)
    furi_hal_gpio_init_ex(
        &gpio_ext_pa7,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn8LPUART1);

    furi_hal_serial_init(serial_handle, 9600);
}

// 🆕 Deinit UART after use
void deinit_uart_serial() {
    if(serial_handle) {
        furi_hal_serial_deinit(serial_handle);
        furi_hal_serial_control_release(serial_handle);
        serial_handle = NULL;
    }
}

// 🖨️ Send thermal printer commands
void send_to_thermal(uint32_t count, bool is_heads) {
    if(!serial_handle) return;

    // Reset printer
    const uint8_t cmd_reset[] = {0x1b, 0x40};
    furi_hal_serial_tx(serial_handle, cmd_reset, sizeof(cmd_reset));
    furi_delay_ms(300);

    // Print message
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Count: %lu - %s\n", count, is_heads ? "Heads" : "Tails");
    furi_hal_serial_tx(serial_handle, (uint8_t*)buffer, strlen(buffer));
    furi_delay_ms(300);

    // Feed and cut
    const uint8_t cmd_feed[] = {0x1b, 0x64, 0x03};
    const uint8_t cmd_cut[] = {0x1d, 0x56, 0x01};
    furi_hal_serial_tx(serial_handle, cmd_feed, sizeof(cmd_feed));
    furi_hal_serial_tx(serial_handle, cmd_cut, sizeof(cmd_cut));
    furi_delay_ms(300);
}

// Draw UI
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_draw_circle(canvas, 64, 25, 24);
    canvas_draw_circle(canvas, 64, 25, 18);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, coin.heads ? 50 : 55, 29, coin.heads ? "heads" : "tails");
    elements_button_center(canvas, "flip");
}

// Handle input
static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Main
int32_t coinflip_main(void* p) {
    UNUSED(p);

    // 🆕 Init UART before loop
    init_uart_serial();

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                switch(event.key) {
                    case InputKeyOk:
                        notification_message(notification, &coinflip_start);
                        coin.heads = random_flip();
                        coin.counter++;
                        send_to_thermal(coin.counter, coin.heads); // 🖨️
                        notification_message(notification, &coinflip_stop);
                        break;
                    case InputKeyBack:
                        running = false;
                        break;
                    default:
                        break;
                }
            }
        }
        view_port_update(view_port);
    }

    // 🧹 Clean up
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    deinit_uart_serial(); // 🧹 Deinit UART

    return 0;
}
