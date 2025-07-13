#include "src/cuberzero.h"
#include <gui/elements.h>

typedef struct {
	PCUBERZERO instance;
	FuriMessageQueue* queue;
} SESSIONSELECTSCENE, *PSESSIONSELECTSCENE;

static void callbackDraw(Canvas* const canvas, void* const context) {
	furi_check(canvas);
	UNUSED(context);
	canvas_clear(canvas);
	elements_text_box(canvas, 10, 10, 50, 50, AlignLeft, AlignTop, "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of \" de Finibus Bonorum et Malorum \" (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, \" Lorem ipsum dolor sit amet..\", comes from a line in section 1.10.32.", 1);
	elements_frame(canvas, 10, 10, 50, 50);
}

static void callbackInput(InputEvent* const event, void* const context) {
	if(!event || !context) {
		return;
	}

	if(event->type == InputTypeShort && event->key == InputKeyBack) {
		furi_message_queue_put(((PSESSIONSELECTSCENE) context)->queue, event, FuriWaitForever);
	}
}

void SceneSessionSelectEnter(void* const context) {
	furi_check(context);
	const PSESSIONSELECTSCENE instance = malloc(sizeof(SESSIONSELECTSCENE));
	instance->instance = context;
	ViewPort* const viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	view_port_draw_callback_set(viewport, callbackDraw, instance);
	view_port_input_callback_set(viewport, callbackInput, instance);
	view_dispatcher_stop(((PCUBERZERO) context)->dispatcher);
	gui_remove_view_port(((PCUBERZERO) context)->interface, ((PCUBERZERO) context)->dispatcher->viewport);
	gui_add_view_port(((PCUBERZERO) context)->interface, viewport, GuiLayerFullscreen);
	const InputEvent* event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		break;
	}

	gui_remove_view_port(((PCUBERZERO) context)->interface, viewport);
	gui_add_view_port(((PCUBERZERO) context)->interface, ((PCUBERZERO) context)->dispatcher->viewport, GuiLayerFullscreen);
	furi_message_queue_free(instance->queue);
	view_port_free(viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
	view_dispatcher_run(((PCUBERZERO) context)->dispatcher);
}
