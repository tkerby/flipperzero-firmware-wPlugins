#include "cuberzero.h"

struct ViewDispatcher {
	bool is_event_loop_owned;
	FuriEventLoop* event_loop;
	FuriMessageQueue* input_queue;
	FuriMessageQueue* event_queue;
	Gui* gui;
	ViewPort* view_port;
};

static void callbackDraw(Canvas* const canvas, const void* const model) {
	UNUSED(model);
	FuriString* string = furi_string_alloc();
	uint32_t tick = furi_get_tick();
	uint32_t seconds = tick / 1000;
	furi_string_printf(string, "%lu:%02lu.%03lu", seconds / 60, seconds % 60, tick % 1000);
	canvas_set_font(canvas, FontBigNumbers);
	canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(string));
	furi_string_free(string);
}

void SceneTimerEnter(const PCUBERZERO instance) {
	UNUSED(instance);
	callbackDraw(NULL, NULL);
	//view_set_draw_callback(instance->view.view, (ViewDrawCallback) callbackDraw);
	//view_dispatcher_switch_to_view(instance->dispatcher, CUBERZERO_VIEW_VIEW);
	//gui_remove_view_port(instance->dispatcher->gui, instance->dispatcher->view_port);
}

bool SceneTimerEvent(const PCUBERZERO instance, const SceneManagerEvent event) {
	UNUSED(instance);
	UNUSED(event);
	return false;
}

void SceneTimerExit(const PCUBERZERO instance) {
	UNUSED(instance);
}
