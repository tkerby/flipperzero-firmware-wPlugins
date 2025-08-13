#include "src/cuberzero.h"
#include <dialogs/dialogs.h>

typedef struct {
	PCUBERZERO instance;
	ViewPort* viewport;
	FuriMessageQueue* queue;
	struct {
		uint8_t action : 1;
	};
} SESSIONSCENE, *PSESSIONSCENE;

enum ACTION {
	ACTION_EXIT
};

static void callbackRender(Canvas* const canvas, void* const context) {
	UNUSED(canvas);
	UNUSED(context);
}

static void callbackInput(InputEvent* const event, void* const context) {
	UNUSED(event);
	UNUSED(context);
}

void SceneSessionEnter(void* const context) {
	furi_check(context);
	const PSESSIONSCENE instance = malloc(sizeof(SESSIONSCENE));
	instance->instance = context;
	instance->viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	view_port_draw_callback_set(instance->viewport, callbackRender, instance);
	view_port_input_callback_set(instance->viewport, callbackInput, instance);
	gui_remove_view_port(instance->instance->interface, instance->instance->dispatcher->viewport);
	gui_add_view_port(instance->instance->interface, instance->viewport, GuiLayerFullscreen);
	const InputEvent* event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		switch(instance->action) {
		case ACTION_EXIT:
			goto functionExit;
		}
	}
functionExit:
	gui_remove_view_port(instance->instance->interface, instance->viewport);
	gui_add_view_port(instance->instance->interface, instance->instance->dispatcher->viewport, GuiLayerFullscreen);
	furi_message_queue_free(instance->queue);
	view_port_free(instance->viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}
