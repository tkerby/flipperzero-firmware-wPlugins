#include "enc_reader.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>

#include <gui/gui.h>
#include <gui/elements.h>

#include <input/input.h>

/*static void enc_reader_app_input_callback(InputEvent* input_event, void* context) {
	furi_assert(context);

	FuriMessageQueue* event_queue = context;
	furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}*/

static void enc_reader_app_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    EncApp* app = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 16, 16, AlignCenter, AlignTop, "Abs:");

    canvas_set_font(canvas, FontBigNumbers);
	static char* abs_coord = "00000000 ";
	snprintf(abs_coord, strlen(abs_coord), "%8d", (int)app->coordinates.abs);
    elements_multiline_text_aligned(canvas, 64, 16, AlignCenter, AlignTop, (const char*)abs_coord);
}

static void enc_reader_app_int_callback(void* context) {
	furi_assert(context);
	EncApp* app = context;

	app->input_value.a = !furi_hal_gpio_read(app->input_pin.a);
	app->input_value.b = !furi_hal_gpio_read(app->input_pin.b);

	if (app->input_value.a)	app->coordinates.abs += app->input_value.b ? 1 : -1;
	else					app->coordinates.abs += app->input_value.b ? -1 : 1;
}

EncApp* enc_reader_app_alloc() {
	EncApp* app = malloc(sizeof(EncApp));

	app->gui				= furi_record_open(RECORD_GUI);
	app->view_port			= view_port_alloc();
	app->event_queue		= furi_message_queue_alloc(8, sizeof(InputEvent));
	app->input_pin.a		= &gpio_ext_pa4;
	app->input_pin.b		= &gpio_ext_pa7;
	app->coordinates.abs	= 0;
	app->coordinates.start	= 0;
	app->coordinates.curr	= 0;

	furi_hal_gpio_init(app->input_pin.a, GpioModeInterruptRiseFall,	GpioPullUp, GpioSpeedVeryHigh);
	furi_hal_gpio_init(app->input_pin.b, GpioModeInput,	GpioPullUp, GpioSpeedVeryHigh);


	gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

	furi_hal_gpio_add_int_callback(app->input_pin.a, enc_reader_app_int_callback, app);
	furi_hal_gpio_enable_int_callback(app->input_pin.a);

	//view_port_input_callback_set(app->view_port, enc_reader_app_input_callback, app->event_queue);
	view_port_draw_callback_set(app->view_port, enc_reader_app_draw_callback, app);

	return app;
}

void enc_reader_app_free(EncApp* app) {
	furi_assert(app);

	furi_message_queue_free(app->event_queue);
	view_port_enabled_set(app->view_port, false);
	gui_remove_view_port(app->gui, app->view_port);
	view_port_free(app->view_port);

	furi_record_close(RECORD_GUI);
}

int32_t enc_reader_app(void *p) {
	UNUSED(p);
	EncApp* app = enc_reader_app_alloc();

	InputEvent event;

	//bool a_trig = false;

	bool running = true;
	while (running) {
		/*app->input_value.a = !furi_hal_gpio_read(app->input_pin.a);
		app->input_value.b = !furi_hal_gpio_read(app->input_pin.b);

		if (app->input_value.a && !a_trig) {
			a_trig = true;
			app->coordinates.abs += app->input_value.b ? 1 : -1;
		} else if (!app->input_value.a && a_trig) {a_trig = false;}*/


    	if (furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
    	    if (event.type == InputTypePress) {
    	        if (event.key == InputKeyBack)
    	            running = false;
    	    }
    	}
	}

	enc_reader_app_free(app);
	return 0;
}