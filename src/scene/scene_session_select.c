#include "src/cuberzero.h"

typedef struct {
	PCUBERZERO instance;
	FuriMessageQueue* queue;
} SESSIONSELECTSCENE, *PSESSIONSELECTSCENE;

static void callbackDraw(Canvas* const canvas, void* const context) {
	furi_check(canvas);
	UNUSED(context);
	canvas_clear(canvas);
	canvas_set_font(canvas, FontPrimary);
	canvas_set_color(canvas, ColorBlack);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Test String");
	canvas_draw_dot(canvas, 4, 1);
	canvas_draw_line(canvas, 3, 2, 5, 2);
	canvas_draw_line(canvas, 2, 3, 6, 3);
	canvas_draw_line(canvas, 1, 4, 7, 4);
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
