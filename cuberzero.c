#include "cuberzero.h"
#include <input/input.h>
#include <furi_hal_light.h>

#define LIGHT(red, green, blue)                \
	do {                                       \
		furi_hal_light_set(LightRed, red);     \
		furi_hal_light_set(LightGreen, green); \
		furi_hal_light_set(LightBlue, blue);   \
	} while(0)

static void callbackInput(const InputEvent* const event, const PCUBERZERO instance) {
	furi_message_queue_put(instance->queue, event, 0);
}

static void callbackRender(const Canvas* const event, const PCUBERZERO context) {
	UNUSED(event);
	UNUSED(context);
}

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	CUBERZERO_LOG("Initializing");
	PCUBERZERO instance = malloc(sizeof(CUBERZERO));
	instance->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
	instance->viewport = view_port_alloc();
	instance->interface = furi_record_open(RECORD_GUI);
	view_port_input_callback_set(instance->viewport, (ViewPortInputCallback) callbackInput, instance);
	view_port_draw_callback_set(instance->viewport, (ViewPortDrawCallback) callbackRender, instance);
	gui_add_view_port(instance->interface, instance->viewport, GuiLayerFullscreen);

	InputEvent event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		if(event.key == InputKeyOk) {
			if(event.type == InputTypePress) {
				LIGHT(255, 0, 0);
			} else if(event.type == InputTypeRelease) {
				LIGHT(0, 255, 0);
			}
		}

		if(event.key == InputKeyBack) {
			LIGHT(0, 0, 0);
			break;
		}
	}

	gui_remove_view_port(instance->interface, instance->viewport);
	furi_record_close(RECORD_GUI);
	view_port_free(instance->viewport);
	furi_message_queue_free(instance->queue);
	free(instance);
	CUBERZERO_LOG("Exiting");
	return 0;
}
